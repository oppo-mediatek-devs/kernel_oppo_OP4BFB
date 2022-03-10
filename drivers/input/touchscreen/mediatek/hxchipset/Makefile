# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/hxchipset/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/i2c/$(MTK_PLATFORM)/

# Makefile for the Himax touchscreen drivers.

obj-$(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)  += himax_debug.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INSPECT) += himax_inspection.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INCELL) += himax_ic_incell_core.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_ONCELL) += himax_ic_oncell_core.o

obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xG) += himax_ic_HX852xG.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xH) += himax_ic_HX852xH.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102) += himax_ic_HX83102.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83103) += himax_ic_HX83103.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83106) += himax_ic_HX83106.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83111) += himax_ic_HX83111.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112) += himax_ic_HX83112.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83113) += himax_ic_HX83113.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83192) += himax_ic_HX83192.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83191) += himax_ic_HX83191.o

obj-$(CONFIG_TOUCHSCREEN_HIMAX_COMMON)+= himax_common.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_COMMON)+= himax_platform.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)= Himax_firmware.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE) += himax_mmi.o

