# ysyx-workbench 协作指南

## 项目概览

这是“一生一芯”（ysyx）学习工作区，采用多个相互配合的子项目实现并验证 RISC-V 系统：

- `nemu/`：NEMU 全系统模拟器，本工作树当前扩展了 RV32E NPC 所需的 DiffTest 参考端能力。
- `abstract-machine/`：AbstractMachine（AM）硬件抽象层、运行时和交叉编译构建框架。
- `am-kernels/`：AM 内核、基准与测试集，是构建 NPC 回归镜像的主要消费者。它是独立 Git 仓库。
- `npc/`：当前主要开发目标。Chisel 实现 RV32E 核心，经 SystemVerilog 与 Verilator/C++ 仿真宿主运行。
- `nvboard/`：基于 SDL2 的虚拟 FPGA 开发板，独立 Git 仓库；当前 NPC 的 batch 验证不依赖其图形界面。
- `fceux-am/`：移植到 AM 的 NES 模拟器，独立 Git 仓库。
- `npc.backup/`：旧 NPC 备份，仅供参考；不要把它当作当前实现或构建入口。

根目录是普通 Git 工作树；`am-kernels/`、`nvboard/`、`fceux-am/` 各自有独立 Git 元数据。操作 Git、清理生成物或检查改动时必须分清所属仓库。

## 目录与职责

```text
.
├── abstract-machine/       AM 库、平台脚本和交叉编译通用规则
├── am-kernels/             AM 应用与回归测试（独立仓库）
├── nemu/                   NEMU 模拟器及 DiffTest 参考实现
├── npc/                    Chisel RV32E 核心、RTL 包装、C++ 仿真器
├── nvboard/                SDL2 虚拟板及引脚绑定工具（独立仓库）
├── fceux-am/               AM 上的 FCEUX（独立仓库）
├── init.sh                 初始化上游子项目并写入环境变量的脚本
```

`init.sh` 面向初次初始化，会克隆指定上游仓库并修改 `~/.bashrc`。已有工作树中不要随意重复执行，尤其不要运行其会删除并重新初始化 `npc/` 的分支。

## 环境与依赖

常用环境变量应指向本工作区的绝对路径：

```bash
export NEMU_HOME=$PWD/nemu
export AM_HOME=$PWD/abstract-machine
export NPC_HOME=$PWD/npc
export NVBOARD_HOME=$PWD/nvboard
```

NPC 构建依赖 `/usr/bin/mill`、Chisel、Verilator、C++20 编译器、Capstone 和 `dlopen()`。`npc/.mill-version` 与 `build.mill` 约束 Mill 1.1.0；应优先显式使用 `MILL=/usr/bin/mill` 或确认 `command -v mill` 没有选中旧的 `/usr/local/bin/mill`。旧 launcher 会把 `build.mill` 误当成缺失的 `build.sc`，或在下载版本时请求错误 URL。

NVBoard 图形运行还需要 SDL2、SDL2_image 与 SDL2_ttf 的开发文件；仅有 `sdl2-compat` 不足以提供 `SDL_image.h` 和相关链接库。可用 `pkg-config --cflags --libs capstone` 与 `sdl2-config --cflags --libs` 排查宿主依赖。

## NPC：当前主线实现

### 架构和源码

- Chisel 源码在 `npc/src/`。`Rv32eCore.scala` 定义单周期 `Rv32eCore`，`NPC` 是其顶层别名，复位 PC 为 `0x80000000`。
- 实现目标是 RV32E：16 个通用寄存器，`x0` 恒为 0。当前覆盖 LUI/AUIPC/JAL/JALR、六种分支、整数 `OP-IMM`/`OP` 子集、加载存储、FENCE/FENCE.I、基础 CSR、ECALL/EBREAK/MRET。
- 访问不对齐数据时，核心将一次访问拆为至多两笔相邻对齐的内存读写；修改访存接口或写掩码时必须同时检查两笔事务。
- `npc/src/Elaborate.scala` 必须生成 `new npc.NPC()`，输出 RTL 为 `npc/build/rtl/NPC.sv`。
- `npc/test/src/Rv32eCoreSpec.scala` 是核心单元测试，覆盖 minirv 子集、不对齐访存、算术控制流、CSR/陷阱及非法 RV32E 高寄存器编码等行为。

### 仿真接口

- `npc/vsrc/NpcTop.sv` 是 Verilator 顶层，模块名必须与 `npc/Makefile` 中的 `TOP_MODULE := NpcTop` 一致。
- 它将 Chisel 的 `NPC` 端口映射到 DPI：`pmem_read`、`pmem_write` 与 `npc_commit`。指令读、数据读和最多两笔数据写均通过 DPI 完成。
- `npc/csrc/main.cpp` 提供 128 MiB 物理内存，基址 `0x80000000`，加载二进制镜像、驱动时钟、判定 trap、生成 VCD，并实现 itrace/mtrace/ftrace、SDB 及动态 DiffTest。
- 波形路径为 `npc/build/waveform.vcd`。VCD 需要同时保留 Verilator 的 `--trace` 和 C++ 中的 `VerilatedVcdC`、`traceEverOn()`、`trace()`、每周期 `dump()`；只启用其中一侧不会得到有效波形。
- AM 的 `halt(int code)` 通过将退出码写入 `a0` 后执行 `ebreak`，仿真器据此判定 good/bad trap。改动 AM trap ABI 或 `npc_commit` 的寄存器顺序会破坏这一约定。

### 构建与运行

在根目录执行：

```bash
make -C npc test                         # Chisel 单元测试
make -C npc build/rtl/NPC.sv             # 生成 SystemVerilog
make -C npc sim                          # 构建 Verilator 可执行文件
make -C npc run IMAGE=/abs/path/app.bin BATCH=1
make -C npc ref                          # 临时配置 NEMU，构建 DiffTest 参考 .so 后恢复原配置
```

`npc/Makefile` 的默认目标为 `all -> sim`。生成物必须保留在 `npc/build/`：RTL 位于 `build/rtl/`，Verilator 目录为 `build/obj_dir/`，可执行文件为 `build/obj_dir/VNpcTop`。`clean` 会删除整个 `build/`。

`run` 要求 `IMAGE` 非空；可选参数包括 `ELF`、`DIFF`、`MAX_CYCLES`、`BATCH`、`ITRACE`、`MTRACE`、`FTRACE`。例如：

```bash
make -C npc run \
  IMAGE=/abs/path/app.bin ELF=/abs/path/app.elf \
  DIFF=$PWD/nemu/build/riscv32-nemu-interpreter-so \
  BATCH=1 MAX_CYCLES=100000
```

Verilator 的详细输出会写到 `npc/build/.verilator.log`；成功时 Makefile 仅显示项目源的 `+V`/`+CC` 行和 Verilator report/summary，失败时会打印完整日志并返回非零。不要把 `make -n` 当作真实编译、链接或仿真成功。

## AM 到 NPC 的路径

AM 的通用规则在 `abstract-machine/Makefile`，按 `ARCH=<ISA>-<platform>` 选择 `abstract-machine/scripts/*.mk`。当前 NPC 使用：

- `abstract-machine/scripts/riscv32e-npc.mk`：`-march=rv32e_zicsr -mabi=ilp32e`，并链接 RV32E 软件实现的除法/乘法辅助例程。
- `abstract-machine/scripts/platform/npc.mk`：PMEM 起点与入口均为 `0x80000000`，构建 ELF、反汇编文本和 `.bin` 镜像，并将运行请求转交给 `npc/Makefile`。
- `ARCH=minirv-npc` 和 `ARCH=riscv32e-npc` 共用当前 NPC 仿真器。

常用回归入口：

```bash
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-npc batch \
  MAX_CYCLES=10000000 DIFF=$PWD/nemu/build/riscv32-nemu-interpreter-so
make -C am-kernels/tests/am-tests ARCH=riscv32e-npc batch mainargs=h \
  MAX_CYCLES=100000 DIFF=$PWD/nemu/build/riscv32-nemu-interpreter-so
```

`batch` 使用无交互仿真，适合自动回归；`run` 会以 `BATCH=0` 启动 SDB。`wrong.c` 是故意 bad trap 的负例，CPU tests 的顶层规则会对它作特殊处理；不要把它作为普通回归失败。`hello_intr` 设计为无限循环，也不适合用 batch 正常退出作为通过标准。

## NEMU 与 DiffTest

NEMU 的构建先依赖 `.config`。常规配置入口为：

```bash
make -C nemu menuconfig
make -C nemu <name>_defconfig
make -C nemu app
```

用于 NPC 的 `nemu/configs/riscv32-npc-ref_defconfig` 选择 RV32 解释器和 `CONFIG_TARGET_SHARE=y`，内存布局为 `MBASE=0x80000000`、`MSIZE=0x08000000`。参考共享库输出为：

```text
nemu/build/riscv32-nemu-interpreter-so
```

`npc/scripts/build-nemu-ref.sh` 会备份 NEMU 的 `.config`、`.config.old`、`include/config` 和 `include/generated`，切换到该 defconfig 构建 `.so`，最后恢复原状态。脚本内部会删除本次参考配置的旧输出以避免复用过期链接结果；不要在其执行期间并行运行其他会改动 NEMU 配置的 make 命令。

`nemu/src/cpu/difftest/ref.c` 是 DiffTest ABI 的实现位置。NPC 仅同步比较 `x0..x15` 和 PC；NEMU 仍使用 RV32I 的 32 寄存器状态，高 16 个寄存器在 NPC 侧按零处理。改动寄存器结构或 `DIFFTEST_REG_SIZE` 时必须保证两端 ABI 和比较策略一致。构建共享库后，至少用 `nm -D --defined-only`、`ldd -r` 和真实 `dlopen()` 消费者检查导出符号和可加载性。

## NEMU 源码导航

- `nemu/src/cpu/cpu-exec.c`：执行主循环；`nemu/src/engine/`：解释器。
- `nemu/src/isa/riscv32/`：RV32 指令、初始化、异常/中断与寄存器定义；当前 CSR 支持集中于 `local-include/csr.h` 和 `system/intr.c`。
- `nemu/src/memory/`：物理/虚拟内存；`nemu/src/monitor/sdb/`：表达式、命令、监视点；`nemu/src/utils/`：日志、计时和指令环形缓冲区。
- `nemu/src/filelist.mk` 控制条件性源文件集合。`CONFIG_TARGET_SHARE` 不应链接 monitor、`nemu-main.c` 或解释器初始化入口。

## NVBoard 与其他子项目

NVBoard 的接入链是 `.nxdc` 约束文件 -> `nvboard/scripts/auto_pin_bind.py` -> `auto_bind.cpp` -> `nvboard_bind_all_pins()` -> `include $(NVBOARD_HOME)/scripts/nvboard.mk`。图形仿真循环需按绑定、`nvboard_init()`、每周期 `nvboard_update()`、`nvboard_quit()` 的顺序运行。当前 NPC 主线尚未把 NVBoard GUI 作为回归标准；batch 验证不应因 SDL 或窗口事件阻塞。

`fceux-am/` 以 `ARCH=native` 等 AM 平台构建，ROM 放在 `fceux-am/nes/rom/`，通过 `mainargs=<rom-name>` 选择。它不属于当前 RV32E NPC 验证链，改动时按其独立仓库状态处理。

## 开发约定与验证边界

- 先确认命令实际选中的路径、环境变量、配置和生成文件，再归因于源码。`NEMU_HOME`、`AM_HOME`、`NPC_HOME`、`NVBOARD_HOME` 指向错误会导致跨工作树构建。
- `npc/Makefile` 递归发现 `csrc/` 下的 C/C++ 源和 `src/` 下的 Scala 源。新增源文件通常无需手工列举；新增 Verilog 包装文件时仍需确认 `VSRC` 和顶层模块名。
- 根与子项目 Makefile 中的 `git_commit` 钩子带 `-@`，受限 `.git` 权限下可能报出可忽略的记录错误。需要根据编译、链接、测试或仿真命令本身的退出状态判断结果。
- 修改 Chisel 端口、DPI 参数、C++ 回调或 NEMU DiffTest 状态结构时，必须作为一个跨模块变更处理，并至少运行单元测试、RTL 生成、Verilator 编译和一个实际 AM 镜像回归。
- 对共享库、镜像与波形做消费者验证：`make -n` 只验证规则展开；文件存在不证明 `.so` 可加载、VCD 有效或仿真完成。
- 保持构建产物在各自 `build/`、`out/` 等忽略目录中；不要提交 Verilator 生成的 `obj_dir`、Mill `out/`、VCD 或临时日志。
- 开始编辑前先执行 `git status --short`，并保留用户已有未提交改动。特别是 `am-kernels/tests/cpu-tests/Makefile` 已有独立修改，除非任务明确涉及它，否则不得覆盖或回退。
