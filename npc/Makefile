TOP_NAME = top

BUILD_DIR = ./build
OBJ_DIR = $(BUILD_DIR)/obj_dir
TARGET = $(OBJ_DIR)/V$(TOP_NAME)

VERILATOR_FLAGS += --cc --exe --build -j 0 -Wall --trace --Mdir $(OBJ_DIR)
CSRC += $(shell find ./csrc -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' \) | sort)
VSRC += $(shell find ./vsrc -type f \( -name '*.v' -o -name '*.sv' \) | sort)

all: build-dir
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	verilator $(VERILATOR_FLAGS) $(CSRC) $(VSRC)

build-dir:
	mkdir -p build

sim: all
	./$(TARGET)

run: sim

clean:
	rm -rf build/ waveform.vcd

include ../Makefile

.PHONY: all sim run clean build-dir
