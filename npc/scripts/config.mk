KCONFIG_PATH := $(NEMU_HOME)/tools/kconfig
FIXDEP_PATH := $(NEMU_HOME)/tools/fixdep
KCONFIG := Kconfig
CONFIG_FILE := .config
CONFIG_HEADER := include/generated/autoconf.h

CONF := $(KCONFIG_PATH)/build/conf
MCONF := $(KCONFIG_PATH)/build/mconf
FIXDEP := $(FIXDEP_PATH)/build/fixdep

$(CONF):
	$(MAKE) -s -C $(KCONFIG_PATH) NAME=conf

$(MCONF):
	$(MAKE) -s -C $(KCONFIG_PATH) NAME=mconf

$(FIXDEP):
	$(MAKE) -s -C $(FIXDEP_PATH)

$(CONFIG_HEADER): $(CONFIG_FILE) $(KCONFIG) $(CONF) $(FIXDEP)
	$(CONF) -s --syncconfig $(KCONFIG)

menuconfig: $(MCONF) $(CONF) $(FIXDEP)
	$(MCONF) $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

default_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/default_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

iverilog_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/iverilog_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

ysyxsoc_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/ysyxsoc_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

ysyxsoc_mrom_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/ysyxsoc_mrom_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

ysyxsoc_flash_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/ysyxsoc_flash_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

ysyxsoc_flash_32_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/ysyxsoc_flash_32_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

ysyxsoc_flash_64_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/ysyxsoc_flash_64_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

ysyxsoc_flash_128_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/ysyxsoc_flash_128_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

ysyxsoc_chiplink_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/ysyxsoc_chiplink_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

ysyxsoc_mrom_difftest_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/ysyxsoc_mrom_difftest_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

ysyxsoc_flash_difftest_defconfig: $(CONF) $(FIXDEP)
	$(CONF) -s --defconfig=configs/ysyxsoc_flash_difftest_defconfig $(KCONFIG)
	$(CONF) -s --syncconfig $(KCONFIG)

savedefconfig: $(CONF)
	$(CONF) -s --savedefconfig=configs/defconfig $(KCONFIG)

config-clean:
	rm -rf include/generated include/config .config .config.old

distclean: clean config-clean

.PHONY: menuconfig default_defconfig iverilog_defconfig ysyxsoc_defconfig \
	ysyxsoc_mrom_defconfig ysyxsoc_flash_defconfig \
	ysyxsoc_flash_32_defconfig ysyxsoc_flash_64_defconfig \
	ysyxsoc_flash_128_defconfig ysyxsoc_chiplink_defconfig \
	ysyxsoc_mrom_difftest_defconfig ysyxsoc_flash_difftest_defconfig \
	savedefconfig config-clean distclean
