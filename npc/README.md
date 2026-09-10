# NPC 使用说明

这是一个基于 Chisel 的 RV32E NPC。它从 `0x80000000` 复位，加载裸机二进制镜像，并提供批处理运行、SDB 调试、DiffTest、指令/内存/函数追踪和 VCD 波形。RTL 可以用 Verilator（二值，DPI-C 驱动，支持 DiffTest 和 SDB）或 Icarus Verilog（四值，VPI 访存，用于检查 X 信号传播）进行仿真，两者通过 `menuconfig` 切换。

本文中的命令均假定当前目录是工作区根目录：

```bash
cd /home/prtysil/Desktop/ysyx-workbench
ROOT=$PWD
```

## 前置条件

- `mill`、`verilator`、C++ 编译器、`pkg-config`、Capstone 开发文件和 RISC-V 交叉编译工具链可用。
- AM 程序请使用 `ARCH=riscv32e-npc`。该目标会使用 `-march=rv32e_zicsr -mabi=ilp32e`，生成链接到 `0x80000000` 的镜像。
- 需要 DiffTest 时，工作区中的 NEMU 源码和其构建依赖也必须可用。

首次构建会生成 Chisel RTL 和 Verilator 仿真器；之后只会在相关源文件变更时重新构建。

## 配置

NPC 的平台、仿真器、DiffTest 和文本 trace 使用独立的 Kconfig 配置，不会修改 NEMU 的 `.config`：

```bash
make -C npc default_defconfig    # Direct NPC + Verilator（全部调试功能默认关闭）
make -C npc iverilog_defconfig   # Direct NPC + Icarus Verilog 四值仿真
make -C npc netlist_defconfig    # Direct NPC + Verilator 门级网表仿真
make -C npc iverilog_netlist_defconfig   # Direct NPC + Icarus Verilog 四值网表仿真
make -C npc ysyxsoc_mrom_defconfig   # ysyxSoC 平台（只支持 Verilator）
make -C npc menuconfig           # 交互式启用配置项
```

`menuconfig` 中先选择 `Simulation platform`，再在 `RTL simulator` 中选择
`Verilator (DPI-C, two-value)` 或 `Icarus Verilog (VPI, four-value)`。Icarus Verilog
只支持 Direct NPC 平台，因此该选项只在 Direct NPC 下可见；`CONFIG_NPC_NETLIST` 打开后
用 ECC 综合出的门级网表替换 RTL，见「Verilator 网表仿真」。其余可配置项为
`CONFIG_NPC_DIFFTEST`、`CONFIG_NPC_DIFFTEST_REF_PATH`、`CONFIG_NPC_ITRACE`、
`CONFIG_NPC_MTRACE` 和 `CONFIG_NPC_FTRACE`。配置文件保存在 `npc/.config`，生成的头文件
保存在 `npc/include/generated/autoconf.h`。首次执行 `make`、`make sim` 或 `make run`
前必须先运行上面的任一配置命令。

## cpu-test测试

下面的命令编译并运行 `dummy` AM 测试。它是验证 NPC 是否可以正常工作的推荐入口：

```bash
make -C am-kernels/tests/cpu-tests \
  ARCH=riscv32e-npc ALL=dummy batch
```

正常结束会输出类似：

```text
HIT GOOD TRAP at pc=0x80000034 after 14 instructions
```

`HIT GOOD TRAP` 且命令返回 `0` 表示成功。`HIT BAD TRAP`、`NPC error` 或非法指令均表示运行失败。

## 构建与直接运行镜像

只构建仿真器：

```bash
make -C npc sim
```

运行 Chisel 单元测试：

```bash
make -C npc test
```

生成 SystemVerilog：

```bash
make -C npc verilog
```

`make -C npc run` 会自动依赖 `sim`，因此通常无需手动先执行 `sim`。`IMAGE` 必填；它按原始字节加载，通常应传入链接到 `0x80000000` 的 `.bin` 镜像。`.elf` 不会被按 ELF 格式解析，不能代替该镜像。

```bash
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  BATCH=1
```

可用变量如下：

| 变量 | 含义 | 默认值 |
| --- | --- | --- |
| `IMAGE` | 待加载的原始二进制镜像，通常为 `.bin`，必填 | 无 |
| `ELF` | 对应 ELF 的符号信息，仅 FTrace 需要 | 无 |
| `BATCH` | `1` 为批处理，`0` 为进入 SDB | `1` |

`DIFF`、`ITRACE`、`MTRACE`、`FTRACE` 不再是 Make 变量；传入这些旧变量会直接报迁移错误。

批处理和 SDB 的 `c` 均不设仿真周期上限，会持续运行直到 trap、watchpoint、错误或 Verilator 结束仿真。`si [N]` 仍只执行用户显式指定的 `N` 个时钟周期。

## Icarus Verilog 四值仿真（检查 X 信号传播）

Verilator 是二值仿真器，未复位的触发器会得到 0 或 1 而不是不定态，因此无法发现“需要复位
但没有复位”的触发器。Icarus Verilog 是四值仿真器，未复位触发器的初值为 `X`；如果这样的
触发器存在，`X` 会沿数据通路传播并可能让程序运行失败。本仓库通过 `make menuconfig` 同时
支持两种仿真器：

- iverilog 编译时会自动定义宏 `__ICARUS__`。`vsrc/NpcTop.sv` 用 `__ICARUS__` 把对
  DPI-C 函数 `pmem_read()`/`pmem_write()` 和 `npc_commit()` 的调用替换为 VPI 系统
  任务/函数 `$pmem_read`/`$pmem_write`，并去掉 DPI-C 的 `import`；iverilog 环境下不实现
  DiffTest。
- `csrc/vpi.c` 被编译为 VPI 模块 `build/iverilog/npc_vpi.vpi`，注册 32 位系统函数
  `$pmem_read` 和系统任务 `$pmem_write`，并在仿真开始前（`cbStartOfSimulation`）把
  `+image=PATH` 指定的程序加载进 `csrc/memory.c` 的内存数组，因此复用了 Verilator 流程
  的物理内存实现。
- `vsrc/IverilogTop.sv` 是自驱动的仿真顶层：它产生时钟和复位、可选地 dump VCD，并在
  `NpcTop` 识别到 `ebreak` 时输出 `HIT GOOD/BAD TRAP` 后结束仿真。没有 DPI-C 时 C 宿主
  不再参与，仿真由 iverilog 自己驱动。

使用方式：

```bash
make -C npc iverilog_defconfig
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" BATCH=1
```

`EFFECTIVE_IMAGE` 会作为 `+image=` 传给 VPI 模块；`MAX_CYCLES=<N>` 会作为
`+max-cycles=` 传给仿真顶层，超过上限会以 `NPC TIMEOUT` 结束（`MAX_CYCLES=0` 表示不限制）。
`WAVEFORM=<vcd>` 会作为 `+wave=` 传给仿真顶层并生成波形：

```bash
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  MAX_CYCLES=200000 WAVEFORM=build/waveform.vcd BATCH=1
gtkwave npc/build/waveform.vcd
```

运行 AM 回归与 `microbench`：

```bash
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-npc batch
make -C am-kernels/benchmarks/microbench ARCH=riscv32e-npc mainargs=test batch
```

在当前的 RTL 上，37 个 cpu-tests 中 36 个 `PASS`（`wrong` 是预期的 bad trap 负例），
`microbench` 的 10 个基准全部 `Passed`。四值仿真的效果也可以用波形确认：仿真时刻 0 时
所有未初始化寄存器都是 `X`，复位结束后电路不再出现 `X`，说明全部需要复位的触发器都已
复位，无需修改 RTL。

iverilog 编译时可能输出讲义中提到的
`sorry: constant selects in always_* processes ...`，以及 `$display`/`$write` 等仿真系统
任务在 `always_ff` 中的 `warning: ... cannot be synthesized`。这些都是仿真专用代码的正常
提示，可以忽略；只看是否出现了 `error:` 以及命令的退出状态。

## 网表仿真

RTL 仿真接受的 Verilog 不一定可综合。为了检查 NPC 中是否含有综合前后行为不一致的代码，
需要把 ECC 综合出的门级网表接回仿真环境，用网表替换 Chisel 生成的 RTL：

```text
+--------------------------------+
| NpcTop                         |
|  +-------------+       +-----+ |
|  | NPC-netlist | <---> | Mem | |   Mem 由 NpcTop.sv 通过 DPI-C 实现
|  +-------------+       +-----+ |
+--------------------------------+
```

网表仿真只针对 Direct NPC 平台，因为讲义要求单独综合 NPC，不包含 ysyxSoC。

### 1. 综合出网表

`ECC` 工程位于 `ecc/npc/`。它默认使用 Direct NPC 的 RTL（`npc/build/rtl`），顶层是 `NPC`：

```bash
# 确保 npc 处于 Direct 平台配置
make -C npc default_defconfig
# 生成 RTL 并重新综合，结果写入 ecc/npc/runs/netlist-sim/
make -C ecc/npc netlist
```

ECC 会同时产生两个网表：`npc_Synthesis_sim.v.gz` 面向仿真，顶层端口与 RTL 的 `NPC`
完全一致，用于替换 RTL 模块；`npc_Synthesis.v.gz` 面向后端物理设计，向量端口被拆成单 bit。
网表仿真使用前者。它是压缩文件，Verilator 无法直接读取，`npc/Makefile` 会自动 gunzip 到
`npc/build/netlist/npc_Synthesis_sim.v`。

### 2. 用 Verilator 编译网表（二值功能检查）

```bash
make -C npc netlist_defconfig   # Direct NPC + Verilator + CONFIG_NPC_NETLIST
make -C npc sim
```

`menuconfig` 中的 `CONFIG_NPC_NETLIST` 打开后，`npc/Makefile` 会用网表替换全部 Chisel RTL
模块，并额外编译 ICsprout55 标准单元行为级模型：

```text
icsprout55-pdk/IP/STD_cell/ics55_LLSC_H7C_V1p10C100/ics55_LLSC_H7CH/verilog/ics55_LLSC_H7CH.v
icsprout55-pdk/IP/STD_cell/ics55_LLSC_H7C_V1p10C100/ics55_LLSC_H7CR/verilog/ics55_LLSC_H7CR.v
icsprout55-pdk/IP/STD_cell/ics55_LLSC_H7C_V1p10C100/ics55_LLSC_H7CL/verilog/ics55_LLSC_H7CL.v
```

并给 Verilator 加上讲义要求的选项：

```text
--timescale "1ns/1ns" --no-timing -D__VERILATOR__ -Dfunctional
```

`-Dfunctional` 让标准单元模型走功能分支、跳过 `specify` 时序检查，`--no-timing` 进一步忽略
时序信息，`--timescale` 固定时间单位，避免网表与模型的时间单位不一致。可用
`CONFIG_NPC_NETLIST_FILE` 指定网表路径，用 `CONFIG_NPC_PDK_ROOT` 指定 PDK 根目录。
网表模式使用独立的 `npc/build/obj_dir-netlist`，不会和 RTL 仿真的对象文件混用。

### 3. 在网表上运行程序

```bash
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" BATCH=1
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-npc batch
make -C am-kernels/benchmarks/microbench ARCH=riscv32e-npc mainargs=test batch
```

在当前 RTL 上，网表的 37 个 cpu-tests 中 36 个 `PASS`（`wrong` 为预期负例），`microbench`
的 10 个基准全部 `Passed`，且 `Scored time`/`Total time` 与 RTL 仿真完全一致，说明综合后的
电路与 RTL 行为一致。另外检查网表可以看到它只包含上升沿 `DFFQX0P5H7R`，没有 `LAT*` 锁存器，
满足流片前端对下降沿时钟和锁存器的要求。

网表中通用寄存器堆已被打平成触发器，且网表里无法再使用 DPI-C，因此网表仿真不方便使用
DiffTest（`netlist_defconfig` 默认关闭）；按讲义建议，应先在 RTL 仿真中用 DiffTest 把问题
排除干净，再做网表仿真。

### 4. 用 Icarus Verilog 做四值网表仿真

Verilator 是二值仿真器，看不到不定态。讲义还要求在 iverilog 上对网表做四值仿真，检查标准
单元模型和网表中是否存在 X 态传播：

```bash
make -C npc iverilog_netlist_defconfig   # Direct NPC + Icarus Verilog + CONFIG_NPC_NETLIST
make -C npc sim
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  MAX_CYCLES=200000 WAVEFORM=build/netlist.vcd BATCH=1
```

讲义指出这一步「无需进行代码上的调整」：访存已经由 VPI 的 `$pmem_read`/`$pmem_write`
实现，网表只替换 `NPC` 模块，`vsrc/NpcTop.sv`（`__ICARUS__` 分支）和 `vsrc/IverilogTop.sv`
都不用改。与 Verilator 网表仿真的区别：

- iverilog 只需 `-Dfunctional` 让标准单元模型走功能分支；`-D__ICARUS__` 由 iverilog 自动
  定义，`--no-timing`/`--timescale` 是 Verilator 专有选项，不需要。
- 网表里的触发器是无复位端的 `DFFQX0P5H7R`，同步复位由 D 端逻辑实现。仿真时刻 0 所有触发器
  都是 `X`，复位结束后应全部变成确定值；如果某个触发器没有复位，`X` 就会一直传播并使程序
  运行失败。

在当前网表上，短仿真波形的分析结果是：23205 个被 dump 的信号里，复位结束后只剩 895 个为
`X`，且全部是 895 个 `DFFQX0P5H7R` 实例内部**未被使用的 `NOTIFIER`**（标准单元模型里给
`specify` 时序检查用的通知寄存器，`-Dfunctional` 下不参与功能），没有任何功能信号为 `X`。
行为上也一致：37 个 cpu-tests 中 36 个 `PASS`（`wrong` 为预期负例），`microbench` 的 10 个
基准全部 `Passed`，`HIT GOOD TRAP at pc=0x80005738`，`$finish` 时间与 RTL 的四值仿真完全相同。
因此综合得到的网表不存在 X 态传播问题，也不需要修改 RTL。

## DiffTest

首次使用或 NEMU 参考端修改后，先构建参考共享库：

```bash
make -C npc ref
```

它会生成 `nemu/build/riscv32-nemu-interpreter-so`。然后在 NPC 配置中启用
`CONFIG_NPC_DIFFTEST`；`CONFIG_NPC_DIFFTEST_REF_PATH` 默认就是该路径（相对于 `npc/`）：

```bash
make -C npc menuconfig
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" BATCH=1
```

运行完整 CPU 回归：

```bash
make -C am-kernels/tests/cpu-tests \
  ARCH=riscv32e-npc batch
```

CPU tests 中的 `wrong` 是故意触发 bad trap 的负例。查看测试日志时应将它与普通测试失败区分开，它不应用于成功冒烟测试。

## 交互调试（SDB）

将 `BATCH=0` 传给 `make run` 后进入 `(npc)` 提示符。文本追踪需先在 `menuconfig` 中启用；启用
`CONFIG_NPC_FTRACE` 时每次运行必须提供 `ELF`。

```bash
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  ELF="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.elf" \
  BATCH=0
```

| 命令 | 作用 |
| --- | --- |
| `c` | 连续运行，直到 trap、watchpoint、错误或 Verilator 结束仿真 |
| `si [N]` | 执行 `N` 个时钟周期，省略时为 1 |
| `info r` | 显示 `x0..x15` 和 PC |
| `info w` | 显示全部 watchpoint |
| `p EXPR` | 计算表达式 |
| `x N EXPR` | 从 `EXPR` 指向的地址显示 `N` 个 32-bit 字 |
| `w EXPR` / `d N` | 创建 / 删除 watchpoint |
| `help` | 显示 SDB 命令列表 |
| `q` | 退出调试器 |

表达式可使用算术运算、括号、`*EXPR` 解引用，以及 `$pc`、`$x0` 到 `$x15` 和 `$a0` 等 ABI 寄存器名称。`si` 的计数单位是时钟周期，不是已提交指令数。

## Trace 与波形

- `CONFIG_NPC_ITRACE`：输出反汇编后的指令执行记录。
- `CONFIG_NPC_MTRACE`：输出内存读写记录。
- `CONFIG_NPC_FTRACE`：输出函数调用/返回记录，必须同时提供 `ELF=...`。
- 这些文本 trace 只在 Verilator 配置下可用。

Verilator 配置下，仿真器初始化成功后会在 `CONFIG_NPC_WAVEFORM_FILE`（默认
`npc/build/waveform.vcd`）生成或覆盖 VCD。该路径相对于 NPC 工作目录，直接运行可执行文件时
会随当前目录变化。Icarus Verilog 配置下用 `WAVEFORM=<vcd>` 指定波形路径，见上一节。VCD
独立于文本 trace，可用查看器打开，例如：

  ```bash
  gtkwave npc/build/waveform.vcd
  ```

## 自己的 AM 程序

建议通过 AM 的标准 Makefile 编译和启动，而不是手动编译宿主程序。若在 `am-kernels/tests/cpu-tests/tests/foo.c` 中新增测试，可以运行：

```bash
make -C am-kernels/tests/cpu-tests \
  ARCH=riscv32e-npc ALL=foo batch
```

AM 的 `npc.mk` 会自动生成 `.bin` 和 `.elf`，并将它们传给 `npc/Makefile`。当前核心是 RV32E 配置，不应手动使用 RV32I 的高位寄存器或 RV32M 指令扩展；需要这些功能时应先扩展 RTL 和 DiffTest 覆盖范围。

## 常见问题

- 首次执行时 Mill 需要下载尚未缓存的依赖；若出现 `repo1.maven.org` 的 DNS 或下载错误，应先检查网络和本地依赖缓存。
- `CONFIG_NPC_FTRACE requires --elf` 表示启用了函数 trace 但没有设置 `ELF`。
- `--diff`、`--itrace`、`--mtrace`、`--ftrace` 和 `--trace` 是已删除的旧参数，请使用 `make menuconfig`。
- Icarus Verilog 配置下没有 SDB、DiffTest 和文本 trace：`BATCH`、`ELF` 只对 Verilator 有效，`HIT GOOD TRAP`/`HIT BAD TRAP` 由 `vsrc/NpcTop.sv` 在 `ebreak` 时直接打印，退出状态分别为 `0`/`1`。
- `CONFIG_NPC_NETLIST` 打开时如果找不到网表，先用 `make -C ecc/npc netlist` 综合；找不到标准单元模型时检查 `CONFIG_NPC_PDK_ROOT` 是否指向 `icsprout55-pdk`。网表模式不支持 Icarus Verilog 和 ysyxSoC 平台，`make` 会直接报错。
- `make -C npc lint` 在 Verilator 配置下运行 `verilator --lint-only`，在 Icarus Verilog 配置下等价于一次成功的 iverilog 编译。
- `q` 只表示退出 SDB，并不等价于测试通过；批处理回归应以 `HIT GOOD TRAP` 和退出状态 `0` 为准。
