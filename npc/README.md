# NPC 使用说明

这是一个基于 Chisel 和 Verilator 的 RV32E NPC。它从 `0x80000000` 复位，加载裸机二进制镜像，并提供批处理运行、SDB 调试、DiffTest、指令/内存/函数追踪和 VCD 波形。

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

NPC 的 DiffTest 和文本 trace 使用独立的 Kconfig 配置，不会修改 NEMU 的 `.config`：

```bash
make -C npc default_defconfig   # 使用默认配置（全部关闭）
make -C npc menuconfig          # 交互式启用配置项
```

可配置项为 `CONFIG_NPC_DIFFTEST`、`CONFIG_NPC_DIFFTEST_REF_PATH`、
`CONFIG_NPC_ITRACE`、`CONFIG_NPC_MTRACE` 和 `CONFIG_NPC_FTRACE`。配置文件保存在
`npc/.config`，生成的头文件保存在 `npc/include/generated/autoconf.h`。首次执行
`make`、`make sim` 或 `make run` 前必须先运行上面的任一配置命令。

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
- 通过本文的 `make -C npc run` 流程，仿真器初始化成功后会生成或覆盖 `npc/build/waveform.vcd`。该路径相对于 NPC 工作目录，直接运行可执行文件时会随当前目录变化。VCD 独立于文本 trace，可用查看器打开，例如：

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
- `q` 只表示退出 SDB，并不等价于测试通过；批处理回归应以 `HIT GOOD TRAP` 和退出状态 `0` 为准。
