BUILD_DIR := build
RTL_DIR := $(BUILD_DIR)/rtl
OBJ_DIR := $(BUILD_DIR)/obj_dir
C_OBJ_DIR := $(BUILD_DIR)/cobj
TOP_MODULE := NpcTop
SIM := $(OBJ_DIR)/V$(TOP_MODULE)
VERILATOR_LOG := $(BUILD_DIR)/.verilator.log

NEMU_HOME ?= $(abspath ../nemu)
MILL ?= /usr/bin/mill
VERILATOR ?= verilator

include scripts/config.mk

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

ifeq ($(wildcard $(CONFIG_FILE)),)
ifeq ($(strip $(MAKECMDGOALS)),)
$(error NPC configuration is missing; run 'make menuconfig' or 'make default_defconfig' first)
endif
ifneq ($(filter all sim run,$(MAKECMDGOALS)),)
$(error NPC configuration is missing; run 'make menuconfig' or 'make default_defconfig' first)
endif
endif

SCALA_SRCS := $(sort $(shell find src -type f -name '*.scala'))
C_SRCS := $(sort $(shell find csrc -type f -name '*.c'))
CXX_SRCS := $(sort $(shell find csrc -type f \( -name '*.cc' -o -name '*.cpp' \)))
C_HEADERS := $(sort $(shell find csrc -type f -name '*.h'))
C_OBJS := $(patsubst csrc/%.c,$(C_OBJ_DIR)/%.o,$(C_SRCS))
C_DEPS := $(C_OBJS:.o=.d)
RTL_TOP := $(RTL_DIR)/NPC.sv
RTL_FILELIST := $(RTL_DIR)/filelist.f
RTL_SV = $(addprefix $(RTL_DIR)/,$(shell sed -e '/^[[:space:]]*$$/d' -e '/^[[:space:]]*#/d' $(RTL_FILELIST) 2>/dev/null))
VSRC := vsrc/NpcTop.sv

CAPSTONE_CFLAGS ?= $(shell pkg-config --cflags capstone 2>/dev/null)
CAPSTONE_LIBS ?= $(shell pkg-config --libs capstone 2>/dev/null)
CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wpedantic
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra

IMAGE ?=
ELF ?=
BATCH ?= 1

RUN_ARGS := --image "$(IMAGE)"
ifneq ($(strip $(ELF)),)
RUN_ARGS += --elf "$(ELF)"
endif
ifneq ($(filter 1 y yes true,$(BATCH)),)
RUN_ARGS += --batch
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

$(C_OBJ_DIR)/%.o: csrc/%.c $(C_HEADERS) $(CONFIG_HEADER) | $(C_OBJ_DIR)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(CAPSTONE_CFLAGS) -I$(abspath include) -I$(abspath csrc/include) \
		-DNPC_CONFIG_ROOT=\"$(abspath .)\" -MMD -MP -c $< -o $@

$(RTL_TOP): $(SCALA_SRCS) build.mill .mill-version | $(RTL_DIR)
	$(MILL) -i runMain Elaborate --target-dir $(RTL_DIR)
	@test -f $@
	@test -f $(RTL_FILELIST)

$(RTL_FILELIST): $(RTL_TOP)
	@test -f $@

verilog: $(RTL_TOP)
	$(call git_commit, "generate verilog")

$(SIM): Makefile $(RTL_FILELIST) $(VSRC) $(CXX_SRCS) $(C_OBJS) $(C_HEADERS) $(CONFIG_HEADER) | $(OBJ_DIR)
	@$(foreach source,$(RTL_SV) $(VSRC),printf '+V %s\n' '$(notdir $(source))';)
	@$(foreach source,$(C_SRCS),printf '+C %s\n' '$(notdir $(source))';)
	@$(foreach source,$(CXX_SRCS),printf '+CC %s\n' '$(notdir $(source))';)
	@# C objects are linker inputs, not prerequisites in Verilator's generated Makefile.
	@rm -f $(SIM)
	@$(VERILATOR) --cc --exe --build --trace --Wno-fatal \
		--top-module $(TOP_MODULE) --Mdir $(OBJ_DIR) \
		-CFLAGS "$(CXXFLAGS) $(CAPSTONE_CFLAGS) -I$(abspath include) -I$(abspath csrc/include) -DNPC_CONFIG_ROOT=\\\"$(abspath .)\\\"" \
		-LDFLAGS "$(abspath $(C_OBJS)) $(CAPSTONE_LIBS) -ldl" \
		$(RTL_SV) $(VSRC) $(CXX_SRCS) >$(VERILATOR_LOG) 2>&1 || { cat $(VERILATOR_LOG); exit 1; }
	@grep -E '^- V e r i l a t i o n +R e p o r t:|^- Verilator:' $(VERILATOR_LOG) || true
	@rm -f $(VERILATOR_LOG)

sim: $(SIM)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!

run: sim
	@test -n "$(IMAGE)" || { echo "IMAGE=<program.bin> is required" >&2; exit 2; }
	$(SIM) $(RUN_ARGS)

help:
	@echo '  menuconfig      - Update NPC debugging configuration'
	@echo '  default_defconfig - Load the default NPC configuration'
	@echo '  savedefconfig   - Save current configuration to configs/defconfig'
	@echo '  config-clean    - Remove generated NPC configuration files'
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

.PHONY: all ref verilog sim run help reformat checkformat bsp idea clean

-include $(C_DEPS)

-include ../Makefile
