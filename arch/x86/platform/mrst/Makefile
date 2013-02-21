ifeq ($(CONFIG_WL12XX_COMPAT_PLATFORM_DATA),y)
ccflags-y += -include $(srctree)/drivers/staging/wl12xx_compat/include/linux/wl12xx.h
endif

obj-$(CONFIG_X86_INTEL_MID)	+= mrst.o
obj-$(CONFIG_X86_INTEL_MID)	+= vrtc.o
obj-$(CONFIG_EARLY_PRINTK_INTEL_MID)	+= early_printk_mrst.o
