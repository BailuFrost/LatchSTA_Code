# SKY130 主从 latch 计数器

本例已有 `counter.v`、`counter.sdc`、`export.ys`、`latch_map.v`。新增 `run_sta.tcl`
和 `run_flow.sh` 补齐综合、静态时序分析、完整引脚图导出与运行归档。
现有 RTL、映射规则和 SDC 的实验预算保持不变；不沿用 tiny 的单元参数 JSON。

四个流程文件的职责：

| 文件 | 内容 |
|---|---|
| `counter.sdc` | clk 10 ns、波形 0/5 ns；en、rst_n 和四位 count 的 IO、slew/load 与 uncertainty |
| `export.ys` | 从行为 RTL 综合；用现有 latch_map.v 映射四个 dlrtn_1 与四个 dlrtp_1；ABC 映射组合逻辑 |
| `run_sta.tcl` | 动态查询八个 latch，分别识别 GATE_N/GATE；保留 RESET_B、recovery/removal 和两种时钟相位 |
| `run_flow.sh` | 新目录中复制输入，执行 Yosys/OpenSTA、导出引脚图、校验输入哈希并汇总报告 |

在仓库根目录（本工作区的 `code/`）运行，需 Yosys 0.68+、兼容图导出接口的 OpenSTA
3.1、Python 3 和 sha256sum：

```sh
sh examples/counter/run_flow.sh ./run_counter_01
```

输出目录须不存在、父目录须存在。此入口只有一个参数，不接收 tiny 求解器路径。
会复用相邻 `tiny_3_latch_circuit/` 或 `tiny_sky130/` 的 `export_pin_graph.tcl`，
并将它复制到本轮工作目录；保留 `examples/` 和 `sky130_pdk/` 的相对目录结构。

主要输出：

- 根部 `flow_status.json`、`inputs.sha256`、源码版本/状态（可用时）和工具版本。
- `logs/yosys.log`、`logs/opensta.log`、`logs/input_integrity.log`。
- `examples/counter/counter_mapped.v` 及全部执行输入副本。
- `examples/counter/reports/` 下的 setup/hold、recovery/removal、脉宽、综合检查和违例报告。
- `latches.tsv` 记录实际实例及 D/Q、GATE/GATE_N、RESET_B 引脚；每实例另有 DQ/CQ、reset→Q min/max 等报告。
- `reports/pin_graph/` 下的八份完整图 TSV，含结构边界极值与见证。

也可先在本案例目录执行 `yosys -l reports/yosys.log -s export.ys`，再执行
`sta -no_init -no_splash -exit run_sta.tcl`。重复 STA 时用环境变量
`COUNTER_REPORT_DIR` 指定尚不存在的目录；已有 STA 结果不覆盖。

时序违例会如实保留并使总流程返回非零；报告不完整或警告也要求复核。返回零只表示
所报告检查通过，检查数量有限，仍需检查相位、周期归属、复位释放假设与所有端点。
不设置 false path，也不固定 rst_n=1 来绕开复位检查。

**当前不执行优化。** 该电路包含异步复位、双极性 latch、反馈和多个数据 IO；tiny 的
转换器和 MCR v3 模型尚不支持这些语义。因此 `flow_status.json` 始终明确标记
`optimization.status=unsupported`，不生成 `.blif.out`、`latch_timing.json` 或候选 SDC。
完整引脚图仍可导出；reset 的检查/异步传播弧保留在完整图中，不能将组合边界图当成
完整的复位行为或时序模型。后续支持优化需要扩展单元表征、周期/极性、复位与 IO 模型。

`run_counter_01/` 已保存一次真实运行尝试，包括映射网表和 82 份时序报告。
该轮在图导出时遇到 `Indistinguishable parallel timing edges`，因此
`flow_status.json` 为 `incomplete`，不能记作完整流程验收通过。原始失败日志保留，
平行边标识问题待修复。没有进行功能仿真或物理时钟树签核。
