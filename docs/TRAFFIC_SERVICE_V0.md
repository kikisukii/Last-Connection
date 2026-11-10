# LAST CONNECTION — Traffic / Service V0

> 当前车型选择、接车阈值和 Debug 已升级为 [Gameplay V0.2](GAMEPLAY_V0_2.md)。此文保留有限服务等 V0 实现的历史记录。

> 后续 TTC 专项修复见 [TTC_SPAWN_SAFETY.md](TTC_SPAWN_SAFETY.md)：前车保证先安全退出时，不再因 TTC 小于固定 1 秒而额外拒绝，并补齐故障诊断。其余本版本 Gameplay 参数保持。

实现日期：2026-11-10。本次为当前 C++ 原型的定向修改。以用户附件和后续三项明确答复为准：速度阈值 8/17/18/20、车辆始终恒速、服务耗尽的 ID 永久不可再次接车（本局内）。

## 1. 保持不变

横移 0.25 s、Normal Run 12、减速 10/s、恢复 5/s、Miss Penalty 5、最低速度 1；终点 1200、Deadline 90、Rank S/A/B/C/D/F 与游戏内时间规则均保持原样。没有新增按键、路线、交通 AI、美术或音效。摄像机尺寸不变。

## 2. Vehicle Boarding

| 车型 | 恒速 | 车身长度 | Catch Zone | maxBoardingRelativeSpeed | rideServiceDistance |
|---|---:|---:|---:|---:|---:|
| Bike | 20 | 3 | 2 | 8 | 160 |
| Bus | 32 | 10 | 7 | 17 | 220 |
| Taxi | 45 | 5 | 3.5 | 18 | 200 |
| Express | 60 | 15 | 7 | 20 | 150 |

全部位于 `include/gameplay_config.hpp`。Express 仅缩小接车区，车身仍长 15。服务距离是首轮可调值，尚未经过用户平衡测试。

阈值说明（源码保留同义英文注释）：

> These are initial values derived from the current 0.25s lateral transfer and -10/s speed decay model. They are not final balance values and will be tuned through playtesting.

离开上一辆车回 CENTER 花 0.25 s，再向下一辆车花 0.25 s，两段均减速。窗口最末速度差依次为 20−12=8、32−15=17、45−27=18、60−40=20。

`predictBoardingCatch` 使用完整 0.25 s 的未接车预测轨迹。该模型速度单调趋向 12，因此最大绝对速度差等于两端绝对差的最大值；两端都在阈值内才允许接车。几何交会仍使用解析曲线计算。不能靠开始瞬间合格或提前交会来绕过窗口后段的速度限制；实际交会成功后仍立即跟随车辆速度，并继续完成横移。

候选仍为横移发起时已经进入车流、位于相邻目标车道的车辆；选最早合格交会，时间相同取较小 ID。pending 和中途新生成车辆不加入。新增唯一排除项：本局已 consumed 的 ID。

## 3. Ride Service / consumed ID

每次生成分配递增唯一 Vehicle ID；本局内不会复用。接上时记录 currentVehicle 和该 ID 的预算。预算从实际纵向交会瞬间开始，骑乘每秒消耗 `vehicleSpeed` 距离，横移尚未完成但已跟随的部分也消耗。

耗尽事件精确切分积分步：将该 ID 预算设为 0（`serviceConsumed(id)` 为 true）、解除骑乘、记录最近完成 ID，从当前横向位置用 0.25 s 返回 CENTER。事件瞬间保留车辆速度，然后回中间途中按原规则减速；例如 Express 释放瞬间 60，0.25 s 后 57.5。Service End 不扣 Miss，不重开，不自动接另一辆车。极短服务在原横移中途耗尽时也从当前位置返回，避免跳位置或重复结算。

**耗尽只取消玩家接入资格，不删除或改变交通实体。** 原车可以继续出现在画面中，以原速度移动；车身文字显示 USED。它按一般离屏规则退休。没有立即替换车辆的逻辑；缺少下一辆时玩家继续跑，等待正常 encounter。

手动提前下车同样保留该 ID 尚未使用的预算；再上同一辆只恢复剩余值，不能刷新满额。这样也避免“耗尽前下车再上车”变成无限续乘。重开新局才清空玩家的 ID 使用记录。

## 4. Per-Lane Scheduler / Spawn Safety

`LiveTraffic` 保留在原架构中，内部两个 LaneScheduler 各自拥有 nextSpawnTime、pending、activeVehicles、mt19937 和调试状态。左侧初次 1 s、右侧 1.5 s，之后各自 2–4 s。背景车型权重仍各 1，即每次背景抽样 25%；不再随机选择车道。阻塞保留本车道请求，右车道计时/RNG 不受左侧阻塞影响。

每次请求在正式加入车流前：

1. 根据车身完整边界生成于视口外；快于玩家从后方进入，慢于玩家从前方进入。
2. 与所有同车道 active 车辆检查车身间隙与 TTC，包含候选是前车、旧车是后车的情况。
3. 不安全则等待，不改变已生成车辆的位置或速度。规划请求若等待到位置已经进入视口，取消并下次重新规划，避免凭空出现。
4. 已进入车流后仅做 `position += speed * dt`。可见车不因服务结束而删除；离屏后按普通退休条件清理。

Planner 可以替换尚未生成的背景 pending，但不能移动/替换 active 车辆。Debug 的 Next 是游戏内绝对请求时刻，不是保证入场时刻；阻塞时可能已到时。

## 5. TTC：纯车辆运动学

前后车按世界位置排序，`gap = frontRearBumper − rearFrontBumper`。先要求 gap 至少 1。

- `rearSpeed <= frontSpeed`：无追赶问题。
- 否则 `TTC = gap / (rearSpeed − frontSpeed)`。
- 初版曾额外要求 TTC 至少 1 s；TTC 专项修复已移除此绝对限制，以是否能在间隙耗尽前保证退出为准。
- 还要求前车保证退出的时间，严格早于 `safeGapTime = (gap − 1) / closingSpeed`，即在消耗掉保留间隙之前退出。

保证退出使用固定世界边界：`worldEnd = finishDistance + visibleAheadDistance`（默认约 1219.846）。前车整个车身离开它所需时间：`max(0, (worldEnd + frontLength/2 − frontPosition)/frontSpeed)`。玩家在终点前的可见区域永远不会超过此边界；车尾越界即可安全退休。

这里没有使用玩家速度，也没有把当前镜头相对速度推算的离屏时间当成保证。因此上/下车不会改变车辆间 TTC。实际离屏允许更早退休；证明使用更晚、保守的固定边界。

例：Bus 32 在 1100，Taxi 45 在 892.5，车身间隙 200，TTC=200/13≈15.3846 s；Bus 保证退出约 3.90 s，允许生成快后车。间隙仅 2.5 的同型组合会拒绝。不是“后车更快一律拒绝”，但在远离终点且车道占用时可能比较保守。

每个 simulation step 步进前以及清理离屏车后，都检查 active 同车道的间隙和未来退出证明。审计失败记录完整 TRAFFIC SAFETY VIOLATION 并抛出逻辑错误，指出 Spawn Safety bug，**不启动刹车 AI**。

**任何地方都没有修改已生成交通车辆的 speed。** 玩家接车时调整的是玩家速度。历史 VehicleStream/RandomVehicleStream 仅用于旧回归夹具，不驱动当前车流。

## 6. Encounter Planner

每 0.25 s 检查一次，优先在未来 1.5–2.5 s 找机会。复制真实 Game 做短期预测，包含当前横移、返回 CENTER、减速及有限服务；按预测交会位置反推目标车当前世界位置，仍须完全在视口外并通过同一 Spawn Safety。

已存在可利用的车辆时，不重复补车。普通骑乘优先下一速度级：升级目标被旧车阻挡时等待清空，不为了补位不断生成更慢的车挡住升级通路。普通车临近服务结束仍可预测主动升级，不强迫等耗尽。Express 最高速则优先 Taxi 等较慢接续目标，在前方安排，并预测服务结束与自动回中间后的交会。

首轮优先区间不是硬保证：高速失误恢复可能需要更早机会；等待服务结束加两段横移可能超过 2.5 s，因此有较早/较晚候选。全部仍受视口、安全间距和速度门槛限制。没有瞬移、自动挂载或强制生成替代车。背景均匀随机加定向规划后，实际车型比例不再各 25%。

最低预告目标为 0.35 s。生成阶段用当前相对速度估计进入视口时刻，具体接车可行性用完整运动模型。固定 C 场景测得 Taxi 提前可见 1.40833 s；未为所有玩家输入路径证明最低可见预告。无需扩大摄像机即可通过此场景。

## 7. Debug Overlay

当前 Vehicle ID / 类型 / 恒速、Current Service Remaining、当前 Consumed:NO、最近 Completed/Consumed ID:YES；无骑乘时 Current ID 和剩余距离为 --。每条车道显示 active 数量、pending 类型、nextSpawnTime、READY/BLOCKED、原因；同时保留玩家速度、状态、横移进度、目标 ID/阈值、窗口和扣分值。ID 直接使用源码的从 0 起编号。

## 8. Deterministic tests

`tests/traffic_service_tests.cpp`：

- A：12→20→32→45→60；Run 不能直上 Express；早期交会不能绕过完整窗口阈值。
- B：有限 Express、事件瞬间保速、回中间不扣 Miss、重试同一 consumed ID 被排除、无车等待不强制挂载、后续新 ID 仍可接；提前下车再上不重置预算、中途耗尽和不同积分分片等价。
- C：Express 耗尽时原车仍以 60 行驶，随后正常接上新 Taxi，约 2.84167 s，提前可见约 1.40833 s。
- D：左侧阻塞与无阻塞对照，右侧车辆位置、生成计时、随机结果相同。
- E：短 TTC 拒绝，200/13 且先退出允许；玩家 12/32/60 对结果无影响。
- F：高速 miss 后，在跌至 12 前接上规划 Taxi。
- 整局：16 个固定种子、默认参数、脚本输入跑到终点；64 次 Express 接车、64 次服务结束；逐步检查已生成车恒速直线、无视口内出生、同车道保留间隙、无 consumed ID 重接。

本次原生构建与全部 9 个 CTest suite 通过；随后补充完整的 consumed→等待→新 ID 场景，新 suite 重跑通过，共 361450 次断言。旧数学测试中需要宽松阈值/长服务的场景改为显式历史夹具，新默认平衡由本 suite 验证。自动输入只用于测试，没有加入正式游戏。

证据：`evidence/traffic-service-tests.txt`、`evidence/traffic-service-ctest.log`。Web Release 编译通过；Edge 实际加载新版本，确认独立车道面板、车辆边缘入场、横移和 Miss 后恢复显示，截图 `evidence/traffic-service-web.png`；当次浏览器 warning/error 日志为空。浏览器没有手动完整重演 Express 接续链，该链由确定性测试验证。浏览器后台节流会使游戏时间慢于真实时间，沿用原时钟约定。

## 9. 交付与局限

核心文件：

- `include/gameplay_config.hpp`：集中可调参数。
- `include/catch_detection.hpp`、`src/gameplay/catch_detection.cpp`：完整窗口门槛。
- `include/game.hpp`、`src/gameplay/game.cpp`：服务事件、预算、消耗 ID。
- `include/movement.hpp`、`src/gameplay/movement.cpp`：服务结束从当前横向位置回中间。
- `include/live_traffic.hpp`、`src/gameplay/live_traffic.cpp`：双车道调度与持续安全审计。
- 新增 `include/traffic_safety.hpp` / `src/gameplay/traffic_safety.cpp`、`include/encounter_planner.hpp` / `src/gameplay/encounter_planner.cpp`。
- `src/gameplay/time_attack.cpp`：接入新 traffic 更新，升级成绩规则指纹。
- `src/main.cpp`：调试面板和 USED 状态。
- `CMakeLists.txt`、新 `tests/traffic_service_tests.cpp`、旧 `tests/catch_tests.cpp` / `tests/live_traffic_tests.cpp` / `tests/recovery_tests.cpp`。
- README、规则/设计/技术/迭代文档与 evidence；原请求存入 `TRAFFIC_SERVICE_REQUEST.txt`。

未重写游戏架构。

最佳成绩规则指纹升级以隔离此前无限骑乘成绩；Rank/Deadline/终点算法没有改变。旧 BEST 不再被新规则读取。

安全检查是保守的；某些占用组合仍可能等待超过目标 encounter 区间，不声称任意状态都能保证 1.5–2.5 s 必有可接车。规划器不会自动控制玩家。原生图形仍受之前记录的 GLFW 监视器初始化失败限制，原生编译/逻辑测试与 Web 显示验证分开记录。

需要用户 Playtest：服务距离 160/220/200/150、Express Zone 7、阈值 8/17/18/20、每车道 2–4 s、规划区间 1.5–2.5 s、服务预告 3 s、可见预告 0.35 s，以及保守 TTC 导致的等待感。以上都不是最终平衡结论。
