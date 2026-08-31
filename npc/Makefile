TOPNAME := top

NVBOARD_HOME ?= $(abspath ../nvboard)

BUILD_DIR := ./build
OBJ_DIR := $(BUILD_DIR)/obj_dir
TARGET := $(BUILD_DIR)/V$(TOPNAME)

NXDC_FILES := constr/top.nxdc
SRC_AUTO_BIND := $(abspath $(BUILD_DIR)/auto_bind.cpp)

VERILATOR ?= verilator
VERILATOR_FLAGS += --cc --exe --build -j 0 -Wall --trace

$(shell mkdir -p $(BUILD_DIR))

# Generate the NVBoard pin binding source from the board constraints.
$(SRC_AUTO_BIND): $(NXDC_FILES)
	python3 $(NVBOARD_HOME)/scripts/auto_pin_bind.py $< $@

VSRCS := $(shell find ./vsrc -type f \( -name '*.v' -o -name '*.sv' \) | sort)
CSRCS := $(shell find ./csrc -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' \) | sort)
CSRCS += $(SRC_AUTO_BIND)

# NVBoard archive and SDL include/link flags.
include $(NVBOARD_HOME)/scripts/nvboard.mk

INCFLAGS := $(addprefix -I,$(INC_PATH))
CXXFLAGS += $(INCFLAGS) -DTOP_NAME="\"V$(TOPNAME)\""

$(TARGET): $(VSRCS) $(CSRCS) $(NVBOARD_ARCHIVE)
	@rm -rf $(OBJ_DIR)
	$(VERILATOR) $(VERILATOR_FLAGS) \
		--top-module $(TOPNAME) $^ \
		$(addprefix -CFLAGS , $(CXXFLAGS)) \
		$(addprefix -LDFLAGS , $(LDFLAGS)) \
		--Mdir $(OBJ_DIR) --exe -o $(abspath $(TARGET))

all: $(TARGET)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!

run: $(TARGET)
	@$(TARGET)

clean:
	rm -rf $(BUILD_DIR) waveform.vcd

include ../Makefile

.PHONY: all run clean
