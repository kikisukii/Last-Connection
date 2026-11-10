# LAST CONNECTION — Gameplay V0.2

> Historical V0.2 snapshot. Current rules and verification: [V0.3 Movement Feel](GAMEPLAY_V0_3.md).
2026-11-10。当前实现说明，替代此前 V0 的车型选择、boarding threshold 和调试显示描述。用户明确的目标为 Run → Bike → Bus → Taxi → Express，同时保留受约束的随机性。

## 本轮范围

仅调整车型选择/机会规划、接车阈值与 Debug。TTC 核心源码及 Movement 积分源码哈希未变；车速、Catch Zone、0.25 s 横移、Run 12、减速 10/s、Miss 5、Rank、Deadline、终点和服务距离保持原值。

## 车型选择与预约

真实游戏的 `TimeAttack → LiveTraffic::update(Game)` 不再独立抽四种车型各 25%。每条车道仍拥有自己的 timer、pending、active、RNG；初次请求左 1 s / 右 1.5 s，成功选出请求后下次为 2–4 s。每 0.25 s 检查可行机会；没有合法候选时等待下次检查，不伪造一辆能接的车。

所有新的真实游戏请求经过以下条件：

1. 对侧 active（包括尚未进画面但仍会接近的车）或 pending 已占用的类型，不可使用。
2. 本车道上一辆**实际入场**的类型不可再次使用。pending、重试和取消不会改写上一辆或统计。
3. 完全位于视口外，玩家轨迹与车型阈值能支持这次机会。
4. 通过既有恒速 TTC 安全检查。已生成车不移动位置、不改速度、不换道来配合规划。

同车道限制针对连续的实际生成序列，例如 Bike → Bike 禁止，Bike → Bus → Bike 可以。对侧类型预约在其 active 车辆完全退休且没有同型 pending 后释放；同车道上一辆的历史继续保留。

优先级在合法、TTC 安全的集合内决定：下一档较快的 Immediate 优先，然后可接的 recovery/alternative，再考虑一步 progression 的 Prepared。已经存在近期升级/接续机会时，优先准备下一档而不重复补相同机会。随机用于同一优先级中的合法类型及可行时机，使用各车道 RNG；不追求各类型数量相等。16 个固定种子的整局测试产生 3 种不同车型生成序列，而非写死单一循环。

历史交通/碰撞测试可关闭 `constrainedVehicleSelection` 来隔离旧夹具；正式默认值为 true。旧 RandomVehicleStream 的均匀抽样仅是历史测试，不驱动新游戏。

## Immediate / Prepared Opportunity

- **Immediate**：无需先骑上另一辆车，就能按当前状态完成短期预测中的接车。预测包含正常的回 CENTER、横移和减速；不表示本帧已经处于 Catch Zone。
- **Prepared**：当前速度门槛不允许直接接，但先接上已经存在、可合理接入的前置车辆，再进行一次正常 progression 后，可以接下一档。

Prepared 使用真实 Game 的副本预测：先找现有车的可行交会，模拟接车完成，再预测下一档接车轨迹，最后按该车恒速反推其当前生成位置。例如 Run12 → 已存在 Bike20 → 预备 Bus32。只准备前置车型的下一档，不从 Run 直接准备可接 Express。

没有真实可行的前置 progression 时不会随意生成更高档来充数。预备车一旦进入 active 就仍是普通恒速交通实体；它是否成为 Immediate 根据玩家实际状态重新计算，不靠修改速度或放宽接车规则。

Run12 对 Bus32 的绝对差是 20，大于新阈值 19，所以 Prepared Bus 不会进入成功接车调度。它可能出现在失败诊断中（Rejected car），这不是成功目标或自动挂载。玩家先骑上 Bike 后，Bus 才可能成为实际可接目标。

硬约束仍可能导致等待，尤其错过前置车、两侧被占用或保守 TTC 拒绝时。本实现不承诺任意玩家输入下都在 1.5–2.5 s 有车可接。无输入的 15 s 确定性场景验证交通仍能继续通过预备机会生成，并非只能刷最初两辆 Bike 后停住。

## 新阈值与不变参数

| 车型 | 速度 | maxBoardingRelativeSpeed | Catch Zone | 车身 | 服务距离 |
|---|---:|---:|---:|---:|---:|
| Bike | 20 | **9** | 2 | 3 | 160 |
| Bus | 32 | **19** | **7** | 10 | 220 |
| Taxi | 45 | **20** | 3.5 | 5 | 200 |
| Express | 60 | **22** | 7 | 15 | 150 |

全部在 `include/gameplay_config.hpp`。旧 8/17/18/20 的推导说明保留为基线，新值明确标注为 V0.2 Prototype tolerance，非最终平衡。

仍然检查完整 0.25 s 未接车预测窗口的最大绝对速度差，不只检查按键或交会瞬间。测试验证 Bike → Bus 的窗口差 18.5 可通过新 19 阈值，而 Run → Bus/Taxi/Express 即使位置重叠也失败。不能把这次放宽当作已经确认“难接由浮点错误造成”；本轮是对照调参，Catch Zone 没变。

## 如何读 Transfer Debug

原游戏区域及世界摄像机范围不变；窗口/画布扩至 1360×690，为右侧诊断和下方统计提供空间，不增加操作键。网页可按窗口宽度缩放。

右侧保留最近一次向交通道发起转移的快照，回 CENTER 或服务结束不会清除它，直到下一次接车尝试：

- TARGET TYPE / ID：有合法目标则显示它；速度拒绝时标记 Rejected；没有几何交会时显示该车道最近中心位置的未 consumed 车辆，标记 diagnostic only。诊断回退不会改变目标选择、挂载或处罚。
- PLAYER SPEED：发起时 → 未接车预测窗口末尾。
- TARGET SPEED、ABS DIFF 两端值、整个窗口 MAX DIFF 与 ALLOWED。
- SPEED CHECK：整个窗口的速度门槛是否通过。
- PLAYER FORWARD RANGE：玩家起止世界位置。
- TARGET CATCH ZONE：t=0 和 t=0.25 s 时车尾/车头方向的 Catch Zone 区间；恒速区间在两者间平移。
- OVERLAP：几何交会 start/end，相对于本次转移开始的秒数；无重叠显示 none。
- POSITION CHECK：几何是否交会，与速度检查独立。
- FINAL RESULT：SUCCESS / MISS；尚未完成时 PENDING，无记录时 --。

这些位置/速度区间均为**未接车预测**；实际交会成功后玩家仍立即跟随车速，完成横移。无目标时速度检查 N/A，不伪造阈值失败。

判断 Bus：

| Speed | Position | 含义 |
|---|---|---|
| PASS | FAIL | 时机/位置未交会；不应据此继续提高 threshold |
| FAIL | PASS | 接车区重叠但完整窗口速度差过大 |
| FAIL | FAIL | 两个条件均不满足 |
| PASS | PASS | 完成后应成功；进行中先显示 PENDING |

## 统计 Debug

下方显示 Bike/Bus/Taxi/Express × LEFT/RIGHT 的实际入场次数，以及每条车道 Last Spawned Type。统计包含所有成功入场路径，不统计 pending/候选/取消。车辆离场不会扣减累计值；重开新局清零。

Immediate Candidates / Prepared Candidates 按车型显示当前 active、未 consumed 车辆的近期预测数量，每 0.25 s 更新。它们不是抽样权重，也不包括尚未入场的 pending；车道面板仍单独显示 pending。当前骑乘车辆不算下一次接车机会。

## 验证结果

- **11/11 CTest suites 通过**；Native Debug 和 Web Release 构建通过。
- 新 selection/debug suite：**6299** 次检查，覆盖新参数、Run 不越级、Bus 容差、独立速度/位置诊断、快照保留、实际 Bike Immediate＋Bus Prepared、pending 预约、离场释放、统计与重开。
- 默认配置 **16 个种子整局全部完成**；48 次 Express 接车、39 次服务结束；车型实际生成序列有 3 种；16 次接车对应此前实际出现过的 Prepared Vehicle ID。
- 整局逐步核对：对侧 active/pending 不同型、本车道不连续同型、真实入场统计一致、恒速轨迹、视口外出生、同车道间隙和 consumed 不重接。
- TTC 专项保留其安全夹具，继续通过；TTC 核心与 Movement 积分源码哈希一致。
- 证据：`evidence/v02-ctest.txt`、`v02-selection-tests.txt`、`v02-service-tests.txt`、`v02-protected-files.json`。旧“每型 25%”断言改为新规则对应的测试；没有把真实游戏退回无约束随机。
- Edge 实测：Run12 对 Bus32 的位置交会为 PASS、速度差 20 超过允许 19 为 FAIL，最终 MISS；右侧完整保留速度、世界范围、Catch Zone 和 overlap 窗口。证据 `evidence/v02-bus-debug.png`。最终扩展面板及修正后的列对齐见 `evidence/v02-overlay-web.png`。
- 浏览器采样日志出现 Zotero 扩展自身连接错误，未观察到本地游戏的应用错误；不将扩展错误报告为游戏故障或声称整个浏览器日志为空。原生图形未重试此前 GLFW 问题，原生证据为构建和逻辑测试。

尚需用户 Playtest：Bus 是否因为新容差更容易接；Speed PASS / Position FAIL 是否频繁；Prepared 车辆是否能被提前理解；去重后的车流密度、等待与 Express 后恢复是否自然。自动输入测试不等于用户手感验收。

最佳成绩规则指纹随新选择器更新，旧规则 BEST 不再混用。Rank、Deadline、终点与时钟算法不变。
