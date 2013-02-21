PREFIX := $(TARGET_KERNEL_SOURCE)/arch/x86/configs

ifndef TARGET_KERNEL_CONFIG_OVERRIDE
    TARGET_KERNEL_CONFIG_OVERRIDE := $(PREFIX)/i386_mfld_android_defconfig

    ifneq ($(filter user userdebug,$(TARGET_BUILD_VARIANT)),)
        TARGET_KERNEL_CONFIG_OVERRIDE += $(PREFIX)/i386_mfld_android_defconfig_production_overlay
    endif
endif

# This rule only takes effect for products which have defined
# TARGET_KERNEL_CONFIG=$(TARGET_OUT_INTERMEDIATES)/kernel.config in their
# BoardConfig.mk file (i.e. products which want a dynamically generated config
# file). For products which set TARGET_KERNEL_CONFIG to a pre-existing config
# file, this rule has no effect.
$(TARGET_OUT_INTERMEDIATES)/kernel.config: $(TARGET_KERNEL_CONFIG_OVERRIDE)
	$(hide) mkdir -p $(dir $@)
	vendor/intel/support/build-defconfig.py $^ > $@
