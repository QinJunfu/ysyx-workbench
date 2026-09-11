# NPC 运行指南

本文记录 NPC 在各种配置下的构建与运行方式。命令都假定当前目录是工作区根目录：

```bash
cd /home/prtysil/Desktop/ysyx-workbench
ROOT=$PWD
```

`npc/README.md` 介绍设计和实现细节，本文只关注「怎么配、怎么跑、怎么看结果」。

## 0. 配置一览

所有配置通过 Kconfig 保存在 `npc/.config`，用下表中的 `*_defconfig` 目标一键切换：

| defconfig 目标 | 平台 | 仿真器 | 网表 | 说明 |
| --- | --- | --- | --- | --- |
| `default_defconfig` | Direct NPC | Verilator | 否 | 默认配置，支持 SDB/DiffTest/trace |
| `iverilog_defconfig` | Direct NPC | Icarus Verilog | 否 | 四值仿真，检查 X 传播 |
| `netlist_defconfig` | Direct NPC | Verilator | 是 | ECC 门级网表，只做周期上限 + 波形检查 |
| `iverilog_netlist_defconfig` | Direct NPC | Icarus Verilog | 是 | 四值门级网表，只做周期上限 + 波形检查 |
| `ysyxsoc_mrom_defconfig` | ysyxSoC | Verilator | 否 | 从 MROM 启动，128 MiB SDRAM |
| `ysyxsoc_flash_32/64/128_defconfig` | ysyxSoC | Verilator | 否 | Flash XIP，对应 SDRAM 容量 |
| `ysyxsoc_chiplink_defconfig` | ysyxSoC | Verilator | 否 | MROM + 128 MiB + ChipLink |
| `ysyxsoc_mrom_difftest_defconfig` | ysyxSoC | Verilator | 否 | MROM + DiffTest |
| `ysyxsoc_flash_difftest_defconfig` | ysyxSoC | Verilator | 否 | Flash + DiffTest |

三条通用规则：

1. **切换配置后必须重新构建仿真器**（`make -C npc sim`）。`make … batch` / `run` 会自动依赖 `sim`，通常不必手动执行。
2. 每一步结束后建议 `make -C npc default_defconfig` 切回默认配置，避免下次跑错平台。
3. 判定标准：RTL 与 ysyxSoC 配置下出现 `HIT GOOD TRAP` 且 `make` 返回 0 即通过；`HIT BAD TRAP`、`NPC error` 或超时表示失败。**网表配置是例外**：网表不含仿真专用的 commit tap，没有 trap 输出，只能设周期上限并用波形检查（见第 3、4 节）。

## 1. Direct NPC + Verilator（二值，最常用）

```bash
make -C npc default_defconfig
```

### 1.1 跑 cpu-tests

```bash
# 全部 37 个
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-npc batch

# 指定一个或多个
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-npc ALL=dummy batch
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-npc ALL="add load-store csr" batch
```

单个测试正常输出：

```text
test list [1 item(s)]: dummy
[         dummy] PASS
```

> `wrong` 是故意触发 bad trap 的**负例**，它会显示 `***FAIL***`，这是预期行为；其余 36 个应为 `PASS`。

### 1.2 跑 microbench

```bash
make -C am-kernels/benchmarks/microbench ARCH=riscv32e-npc mainargs=test batch
```

### 1.3 直接运行单个镜像

`IMAGE` 必填，接原始 `.bin`（通常链接到 `0x80000000`，不能用 `.elf` 代替）：

```bash
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  BATCH=1
```

`BATCH=1` 批处理到底，`BATCH=0` 进入 SDB 交互调试。

### 1.4 SDB 交互调试

```bash
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  ELF="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.elf" \
  BATCH=0
```

进入 `(npc)` 提示符后可用的命令见 `npc/README.md` 的「交互调试（SDB）」一节（`c`、`si [N]`、`info r`、`x N EXPR`、`w EXPR`、`q` 等）。`ELF` 只在启用 FTrace 时必需。

### 1.5 DiffTest

先构建 NEMU 参考库（首次或 NEMU 改动后）：

```bash
make -C npc ref          # 生成 nemu/build/riscv32-nemu-interpreter-so
```

然后在 `make -C npc menuconfig` 中打开 `CONFIG_NPC_DIFFTEST`，再按 1.1/1.3 的方式运行即可。默认配置下 DiffTest 关闭。

### 1.6 文本 trace

在 `make -C npc menuconfig` 中按需打开：

- `CONFIG_NPC_ITRACE`：指令执行记录
- `CONFIG_NPC_MTRACE`：内存读写记录
- `CONFIG_NPC_FTRACE`：函数调用/返回记录，**必须同时提供 `ELF=...`**

### 1.7 波形与周期上限

```bash
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  WAVEFORM=build/waveform.vcd \
  MAX_CYCLES=200000 BATCH=1
gtkwave npc/build/waveform.vcd
```

- `WAVEFORM=<vcd>` 覆盖菜单里配置的波形路径
- `MAX_CYCLES=<N>` 覆盖周期上限，`0` 表示不限制
- `PROGRESS_INTERVAL=<N>` 每 N 个周期打印一次进度

### 1.8 只构建 / 生成 RTL / lint

```bash
make -C npc sim        # 只构建仿真器
make -C npc verilog    # 只生成 SystemVerilog 到 npc/build/rtl
make -C npc lint       # verilator --lint-only
make -C npc reformat   # scalafmt
```

## 2. Direct NPC + Icarus Verilog（四值，检查 X 传播）

```bash
make -C npc iverilog_defconfig
make -C npc sim
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-npc batch
# 或单跑
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  MAX_CYCLES=200000 WAVEFORM=build/waveform.vcd BATCH=1
```

- Icarus 下没有 SDB、DiffTest 和文本 trace，`BATCH`/`ELF` 只对 Verilator 有效。
- `HIT GOOD TRAP` / `HIT BAD TRAP` 由 CPU 内的 `NpcCommitDpi` 在 `ebreak` 时打印。
- iverilog 会输出 `sorry: constant selects …` 之类的提示，属正常，只看有没有 `error:`。

跑完切回：

```bash
make -C npc default_defconfig
```

## 3. Verilator 门级网表

网表由 ECC 从 Direct NPC 的 RTL 综合得到，顶层就是严格的 `NPC`（只有
`cpu-interface.md` 的端口）。综合会去掉 CPU 内部的仿真专用 `NpcCommitDpi`，所以网表是纯标准
单元、不含任何 DPI-C/VPI，也没有 trap 输出；网表仿真用周期上限 + 波形检查，不再判定
`HIT GOOD/BAD TRAP`。

```bash
# 1) 生成网表（会先 make -C npc verilog，再调用 ecc）
make -C npc default_defconfig
make -C ecc/npc netlist

# 2) 用网表替换 RTL 重新构建
make -C npc netlist_defconfig
make -C npc sim

# 3) 运行并抓波形；达到 MAX_CYCLES 时以 cycle limit reached 结束，属预期
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  MAX_CYCLES=200000 WAVEFORM=build/netlist.vcd BATCH=1
gtkwave npc/build/netlist.vcd
```

- 网表会被 gunzip 到 `npc/build/netlist/npc_Synthesis_sim.v`，并使用独立的 `npc/build/obj_dir-netlist`。
- 网表顶层应为 `NPC` 且端口与文档一致；`gunzip -c ecc/npc/runs/netlist-sim/Synthesis_yosys/output/npc_Synthesis_sim.v.gz | grep -c NpcCommitDpi` 应为 `0`。
- 找不到网表时先执行 `make -C ecc/npc netlist`；找不到标准单元模型时检查 `CONFIG_NPC_PDK_ROOT` 指向 `icsprout55-pdk`。

跑完切回：

```bash
make -C npc default_defconfig
```

## 4. Icarus 门级网表（四值）

```bash
make -C npc iverilog_netlist_defconfig
make -C npc sim
make -C npc run \
  IMAGE="$ROOT/am-kernels/tests/cpu-tests/build/dummy-riscv32e-npc.bin" \
  MAX_CYCLES=200000 WAVEFORM=build/netlist.vcd BATCH=1
```

同样不判定 trap：仿真到 `MAX_CYCLES` 会打印 `NPC TIMEOUT` 并结束，属预期。网表里的触发器是
无复位端的 `DFFQX0P5H7R`，仿真时刻 0 全为 `X`，复位结束后功能信号应变为确定值；若某触发器
漏了复位，`X` 会传播并可在波形中观察到。

跑完切回：

```bash
make -C npc default_defconfig
```

## 5. ysyxSoC 平台

一次运行包含：AM 程序编译 → 生成 `ysyxSoCFull.v` → Verilator 构建 → 运行。AM 侧负责把镜像和 SoC 参数传给 `npc`：

```bash
# MROM 启动（128 MiB SDRAM）
make -C npc ysyxsoc_mrom_defconfig
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-ysyxsoc YSYX_ROM=mrom batch

# Flash XIP 启动（128 MiB SDRAM）
make -C npc ysyxsoc_flash_128_defconfig
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-ysyxsoc YSYX_ROM=flash batch
```

交互模式（需要 NVBoard）：

```bash
make -C npc ysyxsoc_mrom_defconfig
make -C am-kernels/tests/cpu-tests ARCH=riscv32e-ysyxsoc YSYX_ROM=mrom run
```

要点：

- `YSYX_ROM` 必须等于所选 Kconfig 的启动介质（`mrom` 或 `flash`），`YSYX_SDRAM_SIZE` 必须与所选容量一致（`0x02000000`/`0x04000000`/`0x08000000`），否则 `npc/Makefile` 会直接报错。
- AM 的 `ARCH=riscv32e-ysyxsoc` 默认 `YSYX_ROM=mrom`、`YSYX_RAM=sram`、`YSYX_XIP=0`、`YSYX_SDRAM_SIZE=0x08000000`，与 `ysyxsoc_mrom_defconfig` / `ysyxsoc_flash_128_defconfig` 匹配。
- 该平台只支持 Verilator，不支持 Icarus 和网表。
- 首次运行会调用 ysyxSoC 的 Makefile 生成 `ysyxSoCFull.v`（`build/soc/…`），比较慢；之后会复用。

跑完切回：

```bash
make -C npc default_defconfig
```

## 6. 常用变量速查

| 变量 | 作用 | 默认 |
| --- | --- | --- |
| `IMAGE` | 待加载的原始二进制镜像，`run` 时必填 | 无 |
| `ELF` | 符号信息，仅 FTrace 需要 | 无 |
| `MROM_IMAGE` / `FLASH_IMAGE` | ysyxSoC 的启动介质镜像 | 无 |
| `BATCH` | `1` 批处理，`0` 进 SDB | `0` |
| `MAX_CYCLES` | 周期上限，`0` 为不限 | Kconfig 值 |
| `WAVEFORM` | VCD 输出路径 | Kconfig 值或空 |
| `PROGRESS_INTERVAL` | 进度打印间隔（周期） | `1000000` |
| `NVBOARD` | `1` 打开 NVBoard | `0` |

> `DIFF`、`ITRACE`、`MTRACE`、`FTRACE`、`YSYX_HAS_CHIPLINK`、`YSYX_SDRAM_PARTICLES` 等旧变量已废弃，传入会报错，请改用 `make menuconfig` 和 Kconfig。

## 7. 清理

```bash
make -C npc clean          # 删除 npc/build
make -C npc config-clean   # 删除 .config 和生成的 autoconf.h
make -C npc distclean      # 两者都删
```
