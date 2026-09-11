BUILD_DIR := build

NEMU_HOME ?= $(abspath ../nemu)
YSYXSOC_HOME ?= $(abspath ../ysyxSoC)
NVBOARD_HOME ?= $(abspath ../nvboard)
MILL ?= /usr/bin/mill
VERILATOR ?= verilator
IVERILOG ?= iverilog
VVP ?= vvp
IVERILOG_VPI ?= iverilog-vpi
CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra

include scripts/config.mk
-include $(CONFIG_FILE)

# The emitted CPU top module is ysyx_24100022 on every platform; it is also the
# Chisel class name, so the generated RTL file keeps that name. Only the reset
# vector, the address-space personality and the simulation wrapper differ
# between the Direct NPC and ysyxSoC platforms.
CHISEL_TOP := ysyx_24100022

ifeq ($(CONFIG_NPC_PLATFORM_YSYXSOC),y)
PLATFORM := ysyxsoc
TOP_MODULE := ysyxSoCFull
ifeq ($(CONFIG_NPC_YSYXSOC_BOOT_FLASH),y)
YSYXSOC_BOOT_MEDIUM := flash
RESET_PC := 0x30000000
else
YSYXSOC_BOOT_MEDIUM := mrom
RESET_PC := 0x20000000
endif
ifeq ($(CONFIG_NPC_YSYXSOC_SDRAM_32MB),y)
YSYXSOC_SDRAM_SIZE := 0x02000000
YSYXSOC_SDRAM_MB := 32
YSYXSOC_SDRAM_PARTICLES := 1
else ifeq ($(CONFIG_NPC_YSYXSOC_SDRAM_64MB),y)
YSYXSOC_SDRAM_SIZE := 0x04000000
YSYXSOC_SDRAM_MB := 64
YSYXSOC_SDRAM_PARTICLES := 2
else ifeq ($(CONFIG_NPC_YSYXSOC_SDRAM_128MB),y)
YSYXSOC_SDRAM_SIZE := 0x08000000
YSYXSOC_SDRAM_MB := 128
YSYXSOC_SDRAM_PARTICLES := 4
else
$(error ysyxSoC requires one persistent SDRAM capacity selection)
endif
ifeq ($(CONFIG_NPC_YSYXSOC_CHIPLINK),y)
YSYXSOC_HAS_CHIPLINK := 1
else
YSYXSOC_HAS_CHIPLINK := 0
endif
YSYXSOC_SOC_VARIANT := sdram$(YSYXSOC_SDRAM_MB)-chiplink$(YSYXSOC_HAS_CHIPLINK)
YSYXSOC_NPC_VARIANT := $(YSYXSOC_BOOT_MEDIUM)-$(YSYXSOC_SOC_VARIANT)
RTL_DIR := $(BUILD_DIR)/rtl-ysyxsoc-$(YSYXSOC_NPC_VARIANT)
C_OBJ_DIR := $(BUILD_DIR)/cobj-ysyxsoc-$(YSYXSOC_NPC_VARIANT)
OBJ_DIR := $(BUILD_DIR)/obj_dir-ysyxsoc-$(YSYXSOC_NPC_VARIANT)
ELABORATE_ARGS := --soc --narrow-addresses --reset-pc $(RESET_PC)
PLATFORM_CFLAGS := -DNPC_RESET_PC=$(RESET_PC)u
else
PLATFORM := direct
# The Direct RTL is the CPU itself; NpcTop.sv is only the simulation wrapper
# around it, so the Verilator top stays the wrapper.
TOP_MODULE := NpcTop
ELABORATE_ARGS := --reset-pc 0x80000000
RTL_DIR := $(BUILD_DIR)/rtl
C_OBJ_DIR := $(BUILD_DIR)/cobj
OBJ_DIR := $(BUILD_DIR)/obj_dir
RESET_PC := 0x80000000
PLATFORM_CFLAGS := -DNPC_RESET_PC=0x80000000u
endif

# Keep the gate-level build in its own Verilator directory so it never reuses
# object files generated from the RTL.
ifeq ($(CONFIG_NPC_NETLIST),y)
OBJ_DIR := $(OBJ_DIR)-netlist
endif

ifeq ($(CONFIG_NPC_SIM_IVERILOG),y)
SIMULATOR := iverilog
else
SIMULATOR := verilator
endif

ifeq ($(PLATFORM),ysyxsoc)
ifeq ($(SIMULATOR),iverilog)
$(error Icarus Verilog only supports the Direct NPC platform; select Direct NPC in menuconfig)
endif
endif

VERILATOR_SIM := $(OBJ_DIR)/V$(TOP_MODULE)
VERILATOR_LOG := $(OBJ_DIR)/.verilator.log

# Icarus Verilog outputs a compiled .vvp program plus a VPI module that
# provides the physical memory.
IVL_DIR := $(BUILD_DIR)/iverilog
IVL_TOP := iverilog_top
IVL_SIM := $(IVL_DIR)/$(IVL_TOP).vvp
VPI_NAME := npc_vpi
VPI_MODULE := $(IVL_DIR)/$(VPI_NAME).vpi

ifeq ($(SIMULATOR),iverilog)
SIM := $(IVL_SIM)
else
SIM := $(VERILATOR_SIM)
endif

# Gate-level netlist simulation.  When CONFIG_NPC_NETLIST is enabled the
# Chisel-generated RTL is replaced by the netlist ECC produced, and the
# ICsprout55 standard cell behavioral models are added to the build.
ifeq ($(CONFIG_NPC_NETLIST),y)
NETLIST_ENABLED := y
else
NETLIST_ENABLED := n
endif
PDK_ROOT ?= $(abspath $(patsubst "%",%,$(CONFIG_NPC_PDK_ROOT)))
STD_CELL_DIR := $(PDK_ROOT)/IP/STD_cell/ics55_LLSC_H7C_V1p10C100
STD_CELL_SRCS := $(STD_CELL_DIR)/ics55_LLSC_H7CH/verilog/ics55_LLSC_H7CH.v \
	$(STD_CELL_DIR)/ics55_LLSC_H7CR/verilog/ics55_LLSC_H7CR.v \
	$(STD_CELL_DIR)/ics55_LLSC_H7CL/verilog/ics55_LLSC_H7CL.v
NETLIST_DIR := $(BUILD_DIR)/netlist
NETLIST_GZ := $(patsubst "%",%,$(CONFIG_NPC_NETLIST_FILE))
NETLIST_V := $(NETLIST_DIR)/npc_Synthesis_sim.v

ifeq ($(NETLIST_ENABLED),y)
ifeq ($(PLATFORM),ysyxsoc)
$(error Gate-level netlist simulation only supports the Direct NPC platform)
endif
endif

ifneq ($(origin DIFF),undefined)
$(error DIFF=... is no longer supported; enable CONFIG_NPC_DIFFTEST with 'make menuconfig')
endif
ifneq ($(origin ITRACE),undefined)
$(error ITRACE=... is no longer supported; enable CONFIG_NPC_ITRACE with 'make menuconfig')
endif
ifneq ($(origin MTRACE),undefined)
$(error MTRACE=... is no longer supported; enable CONFIG_NPC_MTRACE with 'make menuconfig')
endif
ifneq ($(origin FTRACE),undefined)
$(error FTRACE=... is no longer supported; enable CONFIG_NPC_FTRACE with 'make menuconfig')
endif
ifneq ($(origin YSYX_HAS_CHIPLINK),undefined)
$(error YSYX_HAS_CHIPLINK=... is no longer supported; use CONFIG_NPC_YSYXSOC_CHIPLINK)
endif
ifneq ($(origin YSYX_SDRAM_PARTICLES),undefined)
$(error YSYX_SDRAM_PARTICLES=... is no longer supported; select the SDRAM capacity with Kconfig)
endif

ifeq ($(wildcard $(CONFIG_FILE)),)
ifeq ($(strip $(MAKECMDGOALS)),)
$(error NPC configuration is missing; run 'make menuconfig' or 'make default_defconfig' first)
endif
ifneq ($(filter all sim run batch verilog ysyxSoC-run ysyxSoC-batch,$(MAKECMDGOALS)),)
$(error NPC configuration is missing; run 'make menuconfig' or 'make default_defconfig' first)
endif
endif

CONFIG_ONLY_GOALS := menuconfig default_defconfig iverilog_defconfig \
	netlist_defconfig iverilog_netlist_defconfig \
	ysyxsoc_defconfig \
	ysyxsoc_mrom_defconfig ysyxsoc_flash_defconfig \
	ysyxsoc_flash_32_defconfig ysyxsoc_flash_64_defconfig \
	ysyxsoc_flash_128_defconfig ysyxsoc_chiplink_defconfig \
	ysyxsoc_mrom_difftest_defconfig ysyxsoc_flash_difftest_defconfig \
	savedefconfig config-clean distclean help
NON_CONFIG_GOALS := $(filter-out $(CONFIG_ONLY_GOALS),$(MAKECMDGOALS))
VALIDATE_RUNTIME_CONFIG := $(strip $(if $(strip $(MAKECMDGOALS)),\
	$(if $(strip $(NON_CONFIG_GOALS)),y,),y))

ifeq ($(VALIDATE_RUNTIME_CONFIG),y)
ifneq ($(origin YSYX_ROM),undefined)
ifeq ($(CONFIG_NPC_PLATFORM_YSYXSOC),y)
ifneq ($(YSYX_ROM),$(YSYXSOC_BOOT_MEDIUM))
$(error YSYX_ROM=$(YSYX_ROM) conflicts with Kconfig boot medium $(YSYXSOC_BOOT_MEDIUM))
endif
endif
endif

ifneq ($(strip $(YSYX_SDRAM_SIZE)),)
ifeq ($(CONFIG_NPC_PLATFORM_YSYXSOC),y)
ifneq ($(YSYX_SDRAM_SIZE),$(YSYXSOC_SDRAM_SIZE))
$(error YSYX_SDRAM_SIZE=$(YSYX_SDRAM_SIZE) conflicts with Kconfig SDRAM size $(YSYXSOC_SDRAM_SIZE))
endif
endif
endif

ifneq ($(origin YSYX_SOC),undefined)
ifneq ($(filter 1 y yes true,$(YSYX_SOC)),)
ifneq ($(CONFIG_NPC_PLATFORM_YSYXSOC),y)
$(error YSYX_SOC=1 requires CONFIG_NPC_PLATFORM_YSYXSOC=y; select ysyxSoC with 'make menuconfig')
endif
else
ifeq ($(CONFIG_NPC_PLATFORM_YSYXSOC),y)
$(error YSYX_SOC=$(YSYX_SOC) conflicts with the persistent ysyxSoC Kconfig selection)
endif
endif
endif
endif

SCALA_SRCS := $(sort $(shell find src -type f -name '*.scala'))
VPI_SRCS := csrc/vpi.c
ALL_C_SRCS := $(sort $(shell find csrc -type f -name '*.c'))
# csrc/vpi.c only makes sense inside the Icarus Verilog VPI module, so it is
# kept out of the object list that is linked into the Verilator executable.
C_SRCS := $(filter-out $(VPI_SRCS),$(ALL_C_SRCS))
CXX_SRCS := $(sort $(shell find csrc -type f \( -name '*.cc' -o -name '*.cpp' \)))
C_HEADERS := $(sort $(shell find csrc -type f -name '*.h'))
C_OBJS := $(patsubst csrc/%.c,$(C_OBJ_DIR)/%.o,$(C_SRCS))
C_DEPS := $(C_OBJS:.o=.d)
RTL_TOP := $(RTL_DIR)/$(CHISEL_TOP).sv
RTL_FILELIST := $(RTL_DIR)/filelist.f
RTL_SV = $(addprefix $(RTL_DIR)/,$(shell sed -e '/^[[:space:]]*$$/d' -e '/^[[:space:]]*#/d' $(RTL_FILELIST) 2>/dev/null))
DIRECT_VSRC := vsrc/NpcTop.sv

# In netlist mode the single gate-level CPU module takes the place of all the
# Chisel RTL modules, so the RTL file list must not be compiled as well.
ifeq ($(NETLIST_ENABLED),y)
RTL_SIM_SRCS = $(NETLIST_V) $(STD_CELL_SRCS)
RTL_SIM_DEPS = $(NETLIST_V) $(STD_CELL_SRCS)
# The standard cell models contain timing checks that only the functional
# models avoid; --no-timing drops them and the timescale is pinned so the
# netlist and the models agree. The netlist top must carry the same module name
# as the Chisel top, $(CHISEL_TOP), so a netlist synthesized before the top was
# renamed no longer matches NpcTop.sv and has to be resynthesized with
# `make -C ../ecc/npc netlist`; the simulation-only NpcCommitDpi tap is not
# synthesizable, so it is absent here and the netlist carries no DPI-C, VPI or
# black box at all.
VERILATOR_NETLIST_FLAGS := --timescale "1ns/1ns" --no-timing -D__VERILATOR__ -Dfunctional
# iverilog has no --no-timing, but the specify blocks are compiled away by the
# functional define.  __ICARUS__ is defined automatically by iverilog.
IVERILOG_NETLIST_FLAGS := -Dfunctional
else
# Recursively expanded on purpose: RTL_SV reads the generated filelist.f with
# $(shell ...), and the Chisel RTL may be regenerated during this same make
# run, so the list must be evaluated when the recipe runs, not at parse time.
RTL_SIM_SRCS = $(RTL_SV)
RTL_SIM_DEPS = $(RTL_FILELIST)
VERILATOR_NETLIST_FLAGS :=
IVERILOG_NETLIST_FLAGS :=
endif

# Sources compiled into the VPI module that implements $pmem_read/$pmem_write.
VPI_MODULE_SRCS := $(VPI_SRCS) csrc/common.c csrc/memory.c
VPI_INCLUDES := -I$(abspath csrc/include)

ifeq ($(PLATFORM),ysyxsoc)
YSYXSOC_SCALA_SRCS := $(sort $(shell find $(YSYXSOC_HOME)/src -type f -name '*.scala'))
SOC_RTL_DIR := $(BUILD_DIR)/soc/$(YSYXSOC_SOC_VARIANT)
SOC_RAW_RTL := $(SOC_RTL_DIR)/ysyxSoCSource.v
SOC_GENERATED_RTL := $(SOC_RTL_DIR)/ysyxSoCTop.sv
SOC_FULL_RTL := $(SOC_RTL_DIR)/ysyxSoCFull.v
SOC_PERIP_SRCS := $(sort $(shell find $(YSYXSOC_HOME)/perip -type f -name '*.v' 2>/dev/null))
VSRC := $(SOC_FULL_RTL) $(SOC_PERIP_SRCS)
VERILATOR_PLATFORM_FLAGS := --timescale "1ns/1ns" --no-timing \
	-I$(YSYXSOC_HOME)/perip/uart16550/rtl \
	-I$(YSYXSOC_HOME)/perip/spi/rtl
NVBOARD_NXDC := constr/ysyxSoC.nxdc
NVBOARD_AUTO_BIND := $(BUILD_DIR)/nvboard/auto_bind.cpp
include $(NVBOARD_HOME)/scripts/nvboard.mk
CXX_SRCS += $(NVBOARD_AUTO_BIND)
SIM_EXTRA_DEPS := $(NVBOARD_ARCHIVE)
NVBOARD_HOST_CFLAGS := -I$(NVBOARD_HOME)/usr/include
NVBOARD_HOST_LDFLAGS := $(NVBOARD_ARCHIVE) $(LDFLAGS)
else
ifeq ($(SIMULATOR),iverilog)
VSRC := $(DIRECT_VSRC) vsrc/IverilogTop.sv
else
VSRC := $(DIRECT_VSRC)
endif
VERILATOR_PLATFORM_FLAGS :=
SIM_EXTRA_DEPS :=
NVBOARD_HOST_CFLAGS :=
NVBOARD_HOST_LDFLAGS :=
endif

CAPSTONE_CFLAGS ?= $(shell pkg-config --cflags capstone 2>/dev/null)
CAPSTONE_LIBS ?= $(shell pkg-config --libs capstone 2>/dev/null)
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wpedantic

IMAGE ?=
ELF ?=
MROM_IMAGE ?=
FLASH_IMAGE ?=
BATCH ?= 0
NVBOARD ?= 0
ifeq ($(CONFIG_NPC_WAVEFORM),y)
KCONFIG_WAVEFORM := $(subst ",,$(CONFIG_NPC_WAVEFORM_FILE))
WAVEFORM ?= $(KCONFIG_WAVEFORM)
else
WAVEFORM ?=
endif
ifeq ($(CONFIG_NPC_MAX_CYCLES_UNLIMITED),y)
KCONFIG_MAX_CYCLES := 0
else ifneq ($(strip $(CONFIG_NPC_MAX_CYCLES)),)
KCONFIG_MAX_CYCLES := $(CONFIG_NPC_MAX_CYCLES)
else
KCONFIG_MAX_CYCLES := 100000000
endif
MAX_CYCLES ?= $(KCONFIG_MAX_CYCLES)
PROGRESS_INTERVAL ?= 1000000

EFFECTIVE_IMAGE = $(if $(strip $(IMAGE)),$(IMAGE),$(if $(strip $(MROM_IMAGE)),$(MROM_IMAGE),$(FLASH_IMAGE)))

RUN_ARGS = --image "$(EFFECTIVE_IMAGE)" --max-cycles "$(MAX_CYCLES)" \
	--progress-interval "$(PROGRESS_INTERVAL)"
ifneq ($(strip $(ELF)),)
RUN_ARGS += --elf "$(ELF)"
endif
ifneq ($(strip $(MROM_IMAGE)),)
RUN_ARGS += --mrom-image "$(MROM_IMAGE)"
endif
ifneq ($(strip $(FLASH_IMAGE)),)
RUN_ARGS += --flash-image "$(FLASH_IMAGE)"
endif
RUN_ARGS += $(if $(filter 1 y yes true,$(BATCH)),--batch)
RUN_ARGS += $(if $(filter 1 y yes true,$(NVBOARD)),--nvboard)
ifneq ($(strip $(WAVEFORM)),)
RUN_ARGS += --waveform "$(WAVEFORM)"
endif

# Icarus Verilog receives the image and the cycle limit as plusargs. The VPI
# module consumes +image= before the simulation starts, and iverilog_top
# consumes +max-cycles= and +wave=.
IVL_RUN_ARGS = +image="$(EFFECTIVE_IMAGE)" +max-cycles=$(MAX_CYCLES)
ifneq ($(strip $(WAVEFORM)),)
IVL_RUN_ARGS += +wave="$(WAVEFORM)"
endif

ifeq ($(SIMULATOR),iverilog)
# iverilog records the absolute VPI module path inside the .vvp file, so vvp
# loads npc_vpi on its own; passing -m here as well would register the system
# tasks and the start-of-simulation callback twice.
SIM_COMMAND = $(VVP) $(IVL_SIM) $(IVL_RUN_ARGS)
else
SIM_COMMAND = $(SIM) $(RUN_ARGS)
endif

.DEFAULT_GOAL := all

all: sim

ref:
	@bash ./scripts/build-nemu-ref.sh

$(RTL_DIR):
	@mkdir -p $@

$(OBJ_DIR):
	@mkdir -p $@

$(C_OBJ_DIR):
	@mkdir -p $@

$(IVL_DIR):
	@mkdir -p $@

$(NETLIST_DIR):
	@mkdir -p $@

# ECC stores the simulation netlist gzipped, but Verilator cannot read a
# compressed file, so it is expanded into the build directory first.
$(NETLIST_V): $(NETLIST_GZ) | $(NETLIST_DIR)
	@gunzip -c $< > $@
	@test -s $@

$(C_OBJ_DIR)/%.o: csrc/%.c $(C_HEADERS) $(CONFIG_HEADER) | $(C_OBJ_DIR)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(CAPSTONE_CFLAGS) -I$(abspath include) -I$(abspath csrc/include) \
		-DNPC_CONFIG_ROOT=\"$(abspath .)\" $(PLATFORM_CFLAGS) -MMD -MP -c $< -o $@

$(RTL_TOP): $(SCALA_SRCS) build.mill .mill-version $(CONFIG_FILE) | $(RTL_DIR)
	$(MILL) -i runMain Elaborate $(ELABORATE_ARGS) --target-dir $(RTL_DIR)
	@test -f $@
	@test -f $(RTL_FILELIST)
	@# The top module is ysyx_24100022 now, so a previously generated NPC.sv in
	@# the same directory would be a stale duplicate of the same design.
	@rm -f $(RTL_DIR)/NPC.sv

$(RTL_FILELIST): $(RTL_TOP)
	@test -f $@

verilog: $(RTL_TOP)
	$(call git_commit, "generate verilog")

ifeq ($(PLATFORM),ysyxsoc)
$(SOC_RTL_DIR):
	@mkdir -p $@

$(SOC_FULL_RTL): $(YSYXSOC_SCALA_SRCS) $(YSYXSOC_HOME)/Makefile \
	$(YSYXSOC_HOME)/build.sc $(YSYXSOC_HOME)/.mill-version \
	$(YSYXSOC_HOME)/patch/update-firtool.sh | $(SOC_RTL_DIR)
	@YSYX_HAS_CHIPLINK=$(YSYXSOC_HAS_CHIPLINK) \
		YSYX_SDRAM_PARTICLES=$(YSYXSOC_SDRAM_PARTICLES) \
		$(MAKE) -C $(YSYXSOC_HOME) -B verilog \
		V_FILE_GEN=$(abspath $(SOC_GENERATED_RTL)) \
		V_FILE_FINAL=$(abspath $(SOC_RAW_RTL))
	@test -f $(SOC_RAW_RTL)
	@sed 's/\<ysyx_00000000\>/ysyx_24100022/g' $(SOC_RAW_RTL) > $@
	@grep -q 'ysyx_24100022 cpu' $@

$(NVBOARD_AUTO_BIND): $(NVBOARD_NXDC)
	@mkdir -p $(dir $@)
	@NVBOARD_HOME=$(NVBOARD_HOME) python3 $(NVBOARD_HOME)/scripts/auto_pin_bind.py $< $@
endif

ifeq ($(SIMULATOR),iverilog)

# The VPI module is built first: iverilog loads it while compiling so that the
# compiler learns that $pmem_read is a 32-bit system function and $pmem_write
# is a system task. Without this, iverilog would treat $pmem_read as a task and
# reject calls that use its return value.
$(VPI_MODULE): $(VPI_MODULE_SRCS) $(C_HEADERS) | $(IVL_DIR)
	@cd $(IVL_DIR) && $(IVERILOG_VPI) --name=$(VPI_NAME) \
		$(VPI_INCLUDES) \
		$(abspath csrc/vpi.c) $(abspath csrc/common.c) $(abspath csrc/memory.c)

$(IVL_SIM): Makefile $(RTL_SIM_DEPS) $(VSRC) $(CONFIG_HEADER) $(VPI_MODULE) | $(IVL_DIR)
	@$(foreach source,$(RTL_SIM_SRCS) $(VSRC),printf '+V %s\n' '$(notdir $(source))';)
	@# -g2012 enables the SystemVerilog features firtool emits.
	@# The lecture notes call out that the "sorry: constant selects ..."
	@# diagnostics iverilog may print here are harmless and can be ignored.
	@$(IVERILOG) -g2012 -s $(IVL_TOP) \
		$(IVERILOG_NETLIST_FLAGS) \
		-L$(abspath $(IVL_DIR)) -m$(VPI_NAME) \
		-o $@ $(RTL_SIM_SRCS) $(VSRC)

else

$(SIM): Makefile $(RTL_SIM_DEPS) $(VSRC) $(CXX_SRCS) $(C_OBJS) $(C_HEADERS) \
	$(CONFIG_HEADER) $(SIM_EXTRA_DEPS) | $(OBJ_DIR)
	@$(foreach source,$(RTL_SIM_SRCS) $(VSRC),printf '+V %s\n' '$(notdir $(source))';)
	@$(foreach source,$(C_SRCS),printf '+C %s\n' '$(notdir $(source))';)
	@$(foreach source,$(CXX_SRCS),printf '+CC %s\n' '$(notdir $(source))';)
	@# C objects are linker inputs, not prerequisites in Verilator's generated Makefile.
	@rm -f $(SIM)
	@$(VERILATOR) --cc --exe --build --trace --autoflush --Wno-fatal \
		$(VERILATOR_PLATFORM_FLAGS) $(VERILATOR_NETLIST_FLAGS) \
		--top-module $(TOP_MODULE) --Mdir $(OBJ_DIR) \
		-CFLAGS "$(CXXFLAGS) $(CAPSTONE_CFLAGS) $(NVBOARD_HOST_CFLAGS) -I$(abspath include) -I$(abspath csrc/include) -DNPC_CONFIG_ROOT=\\\"$(abspath .)\\\" $(PLATFORM_CFLAGS)" \
		-LDFLAGS "$(abspath $(C_OBJS)) $(CAPSTONE_LIBS) -ldl $(NVBOARD_HOST_LDFLAGS)" \
		$(RTL_SIM_SRCS) $(VSRC) $(CXX_SRCS) >$(VERILATOR_LOG) 2>&1 || { cat $(VERILATOR_LOG); exit 1; }
	@grep -E '^- V e r i l a t i o n +R e p o r t:|^- Verilator:' $(VERILATOR_LOG) || true
	@rm -f $(VERILATOR_LOG)

endif

sim: $(SIM)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!

run: sim
	@test -n "$(EFFECTIVE_IMAGE)" || { echo "IMAGE, MROM_IMAGE, or FLASH_IMAGE is required" >&2; exit 2; }
	@if [ -n "$(strip $(WAVEFORM))" ]; then mkdir -p "$(dir $(WAVEFORM))"; fi
	$(SIM_COMMAND)

batch: BATCH = 1
batch: run

check-ysyxsoc-config:
	@test "$(CONFIG_NPC_PLATFORM_YSYXSOC)" = y || { \
		echo "ysyxSoC target requires CONFIG_NPC_PLATFORM_YSYXSOC=y; run 'make menuconfig'" >&2; \
		exit 2; \
	}

ysyxSoC-run: check-ysyxsoc-config
	@$(MAKE) run BATCH=0 NVBOARD=1 YSYX_SOC=1

ysyxSoC-batch: check-ysyxsoc-config
	@$(MAKE) run BATCH=1 NVBOARD=0 YSYX_SOC=1

ifeq ($(PLATFORM),ysyxsoc)
LINT_TOP := $(CHISEL_TOP)
else
LINT_TOP := NpcTop
endif

ifeq ($(SIMULATOR),iverilog)
# iverilog has no separate lint mode; a successful elaboration of the .vvp
# program is the closest equivalent.
lint: $(IVL_SIM)
	@echo "Icarus Verilog accepted the RTL, the testbench and the VPI module."
else ifeq ($(NETLIST_ENABLED),y)
lint: $(RTL_SIM_DEPS) $(VSRC)
	$(VERILATOR) --lint-only -Wall --Wno-fatal \
		$(VERILATOR_PLATFORM_FLAGS) $(VERILATOR_NETLIST_FLAGS) \
		--top-module $(LINT_TOP) \
		$(RTL_SIM_SRCS) $(VSRC)
else
lint: $(RTL_FILELIST)
	$(VERILATOR) --lint-only -Wall --Wno-fatal \
		--top-module $(LINT_TOP) \
		-I$(abspath $(RTL_DIR)) \
		-f $(RTL_FILELIST)
endif

help:
	@echo '  menuconfig      - Update NPC debugging configuration'
	@echo '  default_defconfig - Load the default NPC configuration'
	@echo '  iverilog_defconfig - Select the direct NPC with the Icarus Verilog simulator'
	@echo '  netlist_defconfig - Select the direct NPC with the ECC gate-level netlist'
	@echo '  iverilog_netlist_defconfig - Select the ECC gate-level netlist with Icarus Verilog'
	@echo '  ysyxsoc_mrom_defconfig  - Select ysyxSoC with reset PC in MROM'
	@echo '  ysyxsoc_flash_{32,64,128}_defconfig - Select flash XIP and SDRAM capacity'
	@echo '  ysyxsoc_chiplink_defconfig - Select the MROM/128 MiB ChipLink SoC'
	@echo '  ysyxsoc_{mrom,flash}_difftest_defconfig - Select a 128 MiB DiffTest SoC'
	@echo '  savedefconfig   - Save current configuration to configs/defconfig'
	@echo '  config-clean    - Remove generated NPC configuration files'
	@echo '  run             - Start the interactive simulator (BATCH=1 is also accepted)'
	@echo '  batch           - Run headless until trap or MAX_CYCLES'
	@echo '  ysyxSoC-run     - Interactive wrapper requiring the ysyxSoC Kconfig platform'
	@echo '  ysyxSoC-batch   - Headless wrapper requiring the ysyxSoC Kconfig platform'
	@echo '  MAX_CYCLES=<N>  - Override menuconfig cycle limit for one run (0 is unlimited)'
	@echo '  WAVEFORM=<vcd>  - Override the menuconfig waveform output path for one run'
	$(MILL) -i runMain Elaborate --help

reformat:
	$(MILL) -i __.reformat

checkformat:
	$(MILL) -i __.checkFormat

bsp:
	$(MILL) -i mill.bsp.BSP/install

idea:
	$(MILL) -i mill.idea.GenIdea/idea

clean:
	-rm -rf $(BUILD_DIR)

.PHONY: all ref verilog sim run batch lint check-ysyxsoc-config ysyxSoC-run ysyxSoC-batch \
	help reformat checkformat bsp idea clean

-include $(C_DEPS)

-include ../Makefile
