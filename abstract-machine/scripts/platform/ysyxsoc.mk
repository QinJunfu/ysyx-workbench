AM_SRCS := riscv/ysyxsoc/start.S \
           riscv/ysyxsoc/bootloader.c \
           riscv/ysyxsoc/trm.c \
           riscv/ysyxsoc/ioe.c \
           riscv/ysyxsoc/spi.c \
           riscv/npc/cte.c \
           riscv/npc/trap.S \
           platform/dummy/vme.c \
           platform/dummy/mpe.c

YSYX_ROM ?= mrom
YSYX_RAM ?= sram
YSYX_XIP ?= 0
YSYX_STACK_SIZE ?= 0x800
YSYX_SDRAM_SIZE ?= 0x08000000

ifeq ($(filter $(YSYX_ROM),mrom flash),)
$(error YSYX_ROM must be one of {mrom, flash}, got "$(YSYX_ROM)")
endif
ifeq ($(filter $(YSYX_RAM),sram psram sdram),)
$(error YSYX_RAM must be one of {sram, psram, sdram}, got "$(YSYX_RAM)")
endif
ifeq ($(filter $(YSYX_XIP),0 1),)
$(error YSYX_XIP must be 0 or 1, got "$(YSYX_XIP)")
endif
ifeq ($(filter $(YSYX_SDRAM_SIZE),0x02000000 0x04000000 0x08000000),)
$(error YSYX_SDRAM_SIZE must be 0x02000000, 0x04000000, or 0x08000000, got "$(YSYX_SDRAM_SIZE)")
endif

YSYX_LDSCRIPT_SOURCE := $(AM_HOME)/scripts/linker-ysyxsoc.ld
YSYX_LDSCRIPT := $(DST_DIR)/linker-ysyxsoc-$(YSYX_ROM)-$(YSYX_RAM)-xip$(YSYX_XIP)-sdram$(YSYX_SDRAM_SIZE)-stack$(YSYX_STACK_SIZE).ld

$(YSYX_LDSCRIPT): $(YSYX_LDSCRIPT_SOURCE)
	@mkdir -p $(dir $@)
	@echo + CPP "->" $(shell realpath $@ --relative-to .)
	@$(CC) -E -P -x c \
		-DYSYX_ROM_REGION=$(YSYX_ROM) \
		-DYSYX_RAM_REGION=$(YSYX_RAM) \
		-DYSYX_XIP=$(YSYX_XIP) \
		-DYSYX_STACK_SIZE=$(YSYX_STACK_SIZE) \
		-DYSYXSOC_SDRAM_SIZE=$(YSYX_SDRAM_SIZE) \
		-o $@ $<

CFLAGS += -fdata-sections -ffunction-sections -msmall-data-limit=0
CFLAGS += -DYSYXSOC_SDRAM_SIZE=$(YSYX_SDRAM_SIZE)
LDSCRIPTS += $(YSYX_LDSCRIPT)
LDFLAGS += --gc-sections --no-relax -Map=$(IMAGE).map -e _start_fsbl

MAINARGS_MAX_LEN = 64
MAINARGS_PLACEHOLDER = the_insert-arg_rule_in_Makefile_will_insert_mainargs_here
CFLAGS += -DMAINARGS_MAX_LEN=$(MAINARGS_MAX_LEN) \
          -DMAINARGS_PLACEHOLDER=$(MAINARGS_PLACEHOLDER)

$(IMAGE).elf: $(YSYX_LDSCRIPT) force

image: image-dep
	@$(OBJDUMP) -d $(IMAGE).elf > $(IMAGE).txt
	@echo + OBJCOPY "->" $(IMAGE_REL).bin
	@$(OBJCOPY) -S -O binary $(IMAGE).elf $(IMAGE).bin

insert-arg: image
	@python $(AM_HOME)/tools/insert-arg.py $(IMAGE).bin \
		$(MAINARGS_MAX_LEN) $(MAINARGS_PLACEHOLDER) "$(mainargs)"

NPC_HOME ?= $(abspath $(AM_HOME)/../npc)
YSYX_SOC_RUN_ARGS = IMAGE=$(IMAGE).bin ELF=$(IMAGE).elf \
                    YSYX_SOC=1 YSYX_ROM=$(YSYX_ROM) \
                    YSYX_RAM=$(YSYX_RAM) YSYX_XIP=$(YSYX_XIP) \
                    YSYX_SDRAM_SIZE=$(YSYX_SDRAM_SIZE)
ifeq ($(YSYX_ROM),mrom)
YSYX_SOC_RUN_ARGS += MROM_IMAGE=$(IMAGE).bin
ifneq ($(strip $(FLASH_DATA)),)
YSYX_SOC_RUN_ARGS += FLASH_IMAGE=$(abspath $(FLASH_DATA))
endif
else
YSYX_SOC_RUN_ARGS += FLASH_IMAGE=$(IMAGE).bin
endif

run: insert-arg
	$(MAKE) -C $(NPC_HOME) ysyxSoC-run $(YSYX_SOC_RUN_ARGS)

batch: insert-arg
	$(MAKE) -C $(NPC_HOME) ysyxSoC-batch $(YSYX_SOC_RUN_ARGS)

.PHONY: insert-arg run batch
