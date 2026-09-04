BUILD_DIR := build
RTL_DIR := $(BUILD_DIR)/rtl
OBJ_DIR := $(BUILD_DIR)/obj_dir
TOP_MODULE := NpcTop
SIM := $(OBJ_DIR)/V$(TOP_MODULE)
VERILATOR_LOG := $(BUILD_DIR)/.verilator.log

NEMU_HOME ?= $(abspath ../nemu)
MILL ?= /usr/bin/mill
VERILATOR ?= verilator

SCALA_SRCS := $(sort $(shell find src -type f -name '*.scala'))
CSRC := $(sort $(shell find csrc -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' \)))
RTL_SV := $(RTL_DIR)/NPC.sv
VSRC := vsrc/NpcTop.sv

CAPSTONE_CFLAGS ?= $(shell pkg-config --cflags capstone 2>/dev/null)
CAPSTONE_LIBS ?= $(shell pkg-config --libs capstone 2>/dev/null)
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra

IMAGE ?=
ELF ?=
DIFF ?=
BATCH ?= 1
MAX_CYCLES ?= 10000000
ITRACE ?= 0
MTRACE ?= 0
FTRACE ?= 0

RUN_ARGS := --image "$(IMAGE)" --max-cycles $(MAX_CYCLES)
ifneq ($(strip $(ELF)),)
RUN_ARGS += --elf "$(ELF)"
endif
ifneq ($(strip $(DIFF)),)
RUN_ARGS += --diff "$(DIFF)"
endif
ifneq ($(filter 1 y yes true,$(BATCH)),)
RUN_ARGS += --batch
endif
ifneq ($(filter 1 y yes true,$(ITRACE)),)
RUN_ARGS += --itrace
endif
ifneq ($(filter 1 y yes true,$(MTRACE)),)
RUN_ARGS += --mtrace
endif
ifneq ($(filter 1 y yes true,$(FTRACE)),)
RUN_ARGS += --ftrace
endif

.DEFAULT_GOAL := all

all: sim

test:
	$(MILL) -i test

ref:
	@bash ./scripts/build-nemu-ref.sh

$(RTL_DIR):
	@mkdir -p $@

$(OBJ_DIR):
	@mkdir -p $@

$(RTL_SV): $(SCALA_SRCS) build.mill .mill-version | $(RTL_DIR)
	$(MILL) -i runMain Elaborate --target-dir $(RTL_DIR)
	@test -f $@

verilog: $(RTL_SV)
	$(call git_commit, "generate verilog")

$(SIM): $(RTL_SV) $(VSRC) $(CSRC) | $(OBJ_DIR)
	@$(foreach source,$(RTL_SV) $(VSRC),printf '+V %s\n' '$(notdir $(source))';)
	@$(foreach source,$(CSRC),printf '+CC %s\n' '$(notdir $(source))';)
	@$(VERILATOR) --cc --exe --build --trace --Wno-fatal \
		--top-module $(TOP_MODULE) --Mdir $(OBJ_DIR) \
		-CFLAGS "$(CXXFLAGS) $(CAPSTONE_CFLAGS)" \
		-LDFLAGS "$(CAPSTONE_LIBS) -ldl" \
		$(RTL_SV) $(VSRC) $(CSRC) >$(VERILATOR_LOG) 2>&1 || { cat $(VERILATOR_LOG); exit 1; }
	@grep -E '^- V e r i l a t i o n +R e p o r t:|^- Verilator:' $(VERILATOR_LOG) || true
	@rm -f $(VERILATOR_LOG)

sim: $(SIM)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!

run: sim
	@test -n "$(IMAGE)" || { echo "IMAGE=<program.bin> is required" >&2; exit 2; }
	$(SIM) $(RUN_ARGS)

help:
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

.PHONY: all test ref verilog sim run help reformat checkformat bsp idea clean

-include ../Makefile
