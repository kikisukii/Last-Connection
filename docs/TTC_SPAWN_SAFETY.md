# TTC Spawn Safety — 2026-11-10

> 此文记录 TTC 专项完成时点。之后的 [V0.2](GAMEPLAY_V0_2.md) 已实施 Gameplay 调整；本页 TTC 核心仍保持不变。

本轮只处理 Vehicle-to-Vehicle 安全。已撤回尚未交付的 V0.2 阈值/接车诊断改动；阈值仍为 **8 / 17 / 18 / 20**。车型多样性、Immediate / Prepared Opportunity、新接车 Debug 均留到下一轮。

## Collision Horizon 与有效区域

固定世界边界沿用 `worldEnd = finishDistance + visibleAheadDistance`，默认约 1219.846。前车车尾达到边界时退休；玩家在终点前能看到的区域不会超过该边界。

`frontExitTime = max(0, (worldEnd + frontLength/2 - frontPosition) / frontSpeed)`

这是前车保证退出的最晚时间，不是假定玩家继续当前速度而估算的镜头退出时间。普通生命周期仍可更早删除已经完全离屏的车辆；删除后它不再参与任何 TTC 检查。

这种固定边界证明较保守：不能证明安全时，新车等待本车道清空。保留这一点，避免玩家后续上/下车使镜头退出预测失效。

## TTC 与入场条件

按世界位置区分前后车，使用车身边界间隙：

`gap = frontPosition - frontLength/2 - rearPosition - rearLength/2`

`closingSpeed = rearSpeed - frontSpeed`

- gap 已小于既有 1 单位安全间隙：拒绝入场。
- closingSpeed ≤ 0：没有未来追赶，TTC 为无穷大（若已经接触/重叠则 TTC=0）。
- closingSpeed > 0：`TTC = gap / closingSpeed`。
- 保留原有安全间隙，因此实际入场要求更严格：`frontExitTime <= (gap - vehicleMinimumGap) / closingSpeed`，即前车先退出，再消耗保留间隙。浮点比较使用微小数值容差，不改变车身或接车区域。

**修复：删除固定“TTC < 1 秒就一律拒绝”的额外拦截。** 例如 TTC=0.5 秒、前车 0.1 秒后退出且仍保有安全间隙，现在允许入场。旧 `minimumSpawnTTC` 配置字段暂为兼容保留，不再作为独立入场门槛。

新车在前方、旧车在后方的组合也使用同一个检查；只检查同车道。检查 API 没有玩家速度参数。

## 生成后与错误证据

已生成车继续使用 `position += speed * dt`。没有主动刹车、加速、换道、速度跟随或碰撞后位置修正。

`LiveTraffic` 在步进前审计，防止未来碰撞状态被随后的离屏删除掩盖；步进和生命周期清理后再次审计。审计覆盖所有同车道配对（包含相邻车）。发现未来安全证明失效、间隙不足或实际 overlap 时：

- 保存结构化 `TrafficViolation`，可通过 `safetyViolation()` 读取。
- stderr 输出 **TRAFFIC SAFETY VIOLATION**；Web 版本进入浏览器错误控制台。
- 输出 simulation time、lane、reason、Leader/Follower 的 ID、speed、position、length、Spawn time、Spawn position，以及 Gap、TTC、front exit time 和 clearance time。
- 抛出携带同一完整记录的逻辑错误，停止有缺陷的模拟，不用改变车辆运动学掩盖 bug。正式正常运行的测试中没有触发它。

生成时间/位置记录的是车辆实际被加入 active 的时刻与位置，不是 pending 请求的时间。构造测试夹具视作 t=0 插入。

## 确定性测试结果

新增 `tests/traffic_safety_tests.cpp`，**9822 次检查通过**：

1. TTC 虽大于 1 秒，但碰撞早于前车退出时拒绝；短而安全的 0.5 秒 TTC / 0.1 秒退出组合允许。
2. Bus 32 与后方 Taxi 45 以三种积分步长运行，前车退出前始终保持间隙；双方速度不变。
3. 真实 Game 执行 Run → Bike → Bus，加速接车瞬间交通车辆对的 TTC 与退出时间不变；经过 0.5 秒时 TTC 仅减少 0.5 秒。
4. 左侧因 TTC 阻塞的运行与清空左侧的对照运行，右侧车辆位置和 nextSpawnTime 完全一致。
5. 前车完全离开视口后，左侧原 pending 后车正常生成。
6. 故意注入未来碰撞与实际重叠，验证详细诊断字段和 runtime 报错，验证没有修改输入车辆速度或位置。日志中的这条错误是故障注入的预期输出。

全部 **10/10 CTest suites** 通过。已有 16 种子整局回归仍完成 64 次 Express 接车、64 次服务结束，无同车道 overlap 或车辆速度修改。Native Debug 与 Web Release 构建通过，捕获的构建日志未发现 warning/error。

证据：

- `evidence/ttc-safety-tests.txt`
- `evidence/ttc-safety-ctest.txt`
- `evidence/ttc-protected-files.json`：撤回 V0.2 改动后建立基线，本轮前后配置、Game、Movement、Catch Detection、Encounter Planner、画面代码的哈希均一致。

本轮没有新增画面元素，未重新进行浏览器手动操作或原生图形验证。测试证明的是运动学/调度行为，不代表完成 Gameplay 平衡。

## 修改文件

`include/traffic_safety.hpp`、`src/gameplay/traffic_safety.cpp`、`include/live_traffic.hpp`、`src/gameplay/live_traffic.cpp`、`tests/traffic_safety_tests.cpp`、`CMakeLists.txt`，以及对应文档/证据。

没有改动车型抽样、boarding threshold、Catch Zone、横移时长、玩家速度参数、Miss、Rank、Deadline、终点或 Ride Service Distance。
