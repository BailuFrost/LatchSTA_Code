# FPGA Latch-based Timing Analysis Tool

本项目是一款针对 FPGA 设计的时序分析与优化工具。主要功能是通过解析基于寄存器（Register-based）的时序报告，建立锁存器（Latch）时序模型。利用锁存器特有的时序借用（Time Borrowing）特性，并结合椭球法（Ellipsoid Method）优化算法，计算设计在引入锁存器后能够达到的最高运行频率。

---

## 一、 环境配置说明

### 1. 包管理 (Conda)
推荐使用 Conda 管理编译环境。

*   **安装 Conda 与依赖：**
    ```bash
    # 下载并安装 Miniconda
    wget "http://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh" -O miniconda.sh
    export CONDA_PREFIX=~/miniconda3
    bash miniconda.sh -b -p $CONDA_PREFIX

    # 安装必要的开发库
    conda install pkg-config
    conda install -c conda-forge \
        cmake ninja bison flex \
        cppcheck valgrind doxygen \
        xtensor-fftw xtensor-blas xtensor \
        openblas boost catch2 benchmark gfortran
    ```

### 2. 外部依赖库
*   **GLPK (GNU Linear Programming Kit)**: 
    *   需从 [官网](http://www.gnu.org/software/glpk/) 下载安装。
    *   编译时 CMake 会自动通过 `find_package` 或手动指定的路径链接 `-lglpk`。
*   **LEDA**: 
    *   需从 [官网](http://www.algorithmic-solutions.com/) 下载免费版。
    *   编译选项需包含：`-I$LEDAROOT/incl -L$LEDAROOT -lleda -lX11 -lm`。

---

## 二、 编译指南

请在项目根目录下执行以下命令进行构建：

```bash
# 1. 创建并进入构建目录
mkdir -p build && cd build

# 2. 运行 CMake 配置
cmake ..

# 3. 编译（使用 8 线程并行）
make -j8
```

## 三、 使用说明

程序编译完成后生成 `main` 可执行文件，运行格式如下：

```bash
./main <input_file> <iterations> <ellipsoid_radius> [latch_timing.json] [--duty-cycle fraction] [--export-sdc new_file.sdc]
```

参数说明：
input_file: 输入的网表或时序报告文件路径（如 ../benchmark/s27.blif.out）。
iterations: 算法优化迭代次数（如 1 或 100）。
ellipsoid_radius: 椭球法搜索的初始半径（如 100）。
latch_timing.json: 可选的逐实例六参数文件，按实例名称匹配，单位 ns。
省略时保留统一常量；指定时用于 MCR 的 Howard、YTO、LP 和可行性检查。
ELL 使用独立旧模型；加载 v3 真实 IO 时跳过 ELL。
--duty-cycle: MCR 高电平占空比，范围 (0, 0.5]；v1/v2 默认 0.5，v3 默认取自 SDC。
每次运行固定一个占空比，透明窗口为 [s_k+(1-duty)*T, s_k+T]。
JSON 加载真实 latch 的六项参数、v2 起的脉宽检查及 v3 的 IO 约束；端口与零参考点的六参数为零。
此选项同时用于 Howard、YTO、LP 和原式验证；ELL 不使用该占空比。

逐实例 JSON v2 还包含 `clock_checks_ns.min_pulse_width_high/low`，单位 ns。
数值表示库要求，`null` 表示已检查该库弧但没有对应检查；v2 必须显式提供两项。
MCR 将周期下界设为所有实例的 `max(high/duty, low/(1-duty))`，图、LP 和原式验证一致。
v1 和未提供 JSON 的旧输入仍可运行，但没有脉宽约束；程序打印实例覆盖数量和周期下界。
改变工艺角或时钟 transition 后应重新提取；此模型仍假设理想时钟、固定占空比。

JSON v3 还加载 `io_constraints`：单输入/单输出、同一理想时钟上升沿参考、
显式 min/max IO delay 和 setup/hold uncertainty。输入由本周期上升沿发射，
输出 setup/hold 分别使用下一/本周期上升沿；无反馈数据路径按同次事务传播，不再逐边减 T。
加载器将压缩输入中合并的 IO 拆成真实输入端口、真实输出端口和独立零参考点。
v3 默认占空比取自 SDC 波形；`--duty-cycle` 可覆盖，保持下降沿为 T。
反馈、其他周期映射或多个端口不在此首版范围内，会拒绝输入。
Latch 发射路径的 hold 使用开启响应下界作保守检查，不把可行势变量当作物理最早到达时间。
v1/v2 继续保留旧合并 IO 和路径周期关系；ELL 在 v3 模式下跳过。

`--export-sdc new_file.sdc` 导出同次 Howard 求解的周期和全部 latch 偏移。
需要重新生成的 v3 JSON，包含 `sta_conditions`（时钟端口、clock/input transition、output load）。
Howard 和 LP 均须为 Optimal、通过图及原式检查，并在周期容差内一致；不导出回退求解器的结果。
输出必须是尚不存在的 `.sdc` 文件，父目录须已存在。导出保留 IO、工况和 uncertainty，
以各 latch GATE 引脚的理想 network latency 表达偏移；注释记录周期、占空比和透明窗口。
仅支持扁平简单实例名及 D/GATE/Q 接口，尚不表示实际时钟树实现或 OpenSTA 验证通过。
新增源文件后需重新运行 CMake 配置，再构建。案例完整命令见 `examples/tiny_sky130/README.md`。

运行示例：
```bash
./main ../benchmark/s27.blif.out 1 100
```


## 四、 目录结构说明
src/: 项目核心源代码目录 (.cpp)。
include/: 项目头文件目录 (.h)。
benchmark/: 存放标准测试数据集。
benchmark_rslt/: 存放参考测试结果数据。
CSTIC/:专门用于对基于寄存器的时序报告进行解析，重点分析在锁存器依赖环境下FPGA 设计能够达到的时序提升空间。
cmake/: 包含 CMake 构建系统的相关配置文件。
