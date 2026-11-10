# V0.2 原始确认记录（已实施）

> 本文件保留下一轮授权时的需求记录。当前实现、参数和测试结果见 [GAMEPLAY_V0_2.md](GAMEPLAY_V0_2.md)，下文“尚未实施/当前仍为”属于该历史时点。

2026-11-10 用户明确：本轮先只完成 TTC Spawn Safety，下轮再继续以下 Gameplay 调整。本文件记录工作区需求，不代表已实现或已平衡。

- 车型选择取消独立均匀 25% 的目标，改为受约束随机；另一车道 active/pending/encounter-relevant 的类型不可同时生成，本车道连续生成不能同型。
- 结合玩家速度、可接性和下一段 encounter 意义，优先下一速度档，保留随机性，不强求各型出现次数相等。
- 阈值计划调整为 Bike/Bus/Taxi/Express = 9/19/20/22；当前实际仍为 8/17/18/20。Catch Zone 不变，尤其 Bus=7。
- Immediate Opportunity：当前速度下实际可接的目标。
- Prepared Opportunity：当前不能接，但完成一次合理 progression 后可接的下一档目标。例如 Run12 对 Bike20 是 Immediate，对 Bus32 是 Prepared。
- 当前立即可接类型因去重受限时，可以准备下一阶车型，不应让调度整体停滞。Prepared 必须有合理的空间位置和 progression；不能靠修改阈值、玩家/车辆速度，或突然在旁边出生来绕过速度阶梯。
- Run12 不允许一个 transfer 直接接 Bus32。Prepared Bus 不成为当前可选接车目标；先骑 Bike 后才逐渐具备即时接入资格。
- 两车道独立调度，所有入场继续服从恒速 TTC Spawn Safety。
- Debug 增加 Immediate Candidates / Prepared Candidates，以及完整 Transfer 速度、位置区间、Catch Zone 区间、重叠起止与最终成败诊断。
- 统计真实入场次数，按车型×车道显示，并显示每车道 Last Spawned Type。

实施时需区分“全局类型占用预约”与“本车道上一辆实际生成类型”，避免 pending 重抽、取消或重试污染统计及连续重复判定。
