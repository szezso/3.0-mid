#
# Makefile for the graphics hardware.  This driver provides support for the
# Power VR accelerated 2D and 3D cores


PVR_BUILD_DIR ?= $(PWD)


# Services module name
PVRSRV_MODNAME = pvrsrvkm

# FIXME do we need these
# -DSUPPORT_SGX_HWPERF

#-DPVR_LINUX_USING_WORKQUEUES \
#-DPVR_LINUX_MISR_USING_PRIVATE_WORKQUEUE \
#-DPVR_LINUX_TIMERS_USING_WORKQUEUES \
#-DSYS_CUSTOM_POWERLOCK_WRAP


INCDIR=drivers/staging/ice4100/sgx535

include_dirs := -I$(INCDIR)/ \
	-I$(INCDIR)/include \
	-I$(INCDIR)/include/env/linux \
	-I$(INCDIR)/bridged \
	-I$(INCDIR)/bridged/sgx \
	-I$(INCDIR)/devices/sgx \
	-I$(INCDIR)/system \
	-I$(INCDIR)/system/include \
	-I$(INCDIR)/system/sgx_intel_ce \
	-I$(INCDIR)/hwdefs \
	$(ALL_CFLAGS_kbuild)

ccflags-y := $(include_dirs)

# X86_INTEL_CE device information
ccflags-y += -DSGX535 -DSUPPORT_SGX535 -DSGX_CORE_REV=121

ccflags-y += \
	-Werror \
	-DLINUX \
	-DPVR_BUILD_DATE="\"$(DATE)\"" \
	-DPVR_BUILD_DIR="\"$(PVR_BUILD_DIR)\"" \
	-DSERVICES4 \
	-D_XOPEN_SOURCE=600 \
	-DPVR2D_VALIDATE_INPUT_PARAMS \
	-UDEBUG_LOG_PATH_TRUNCATE \
	-DSUPPORT_SRVINIT \
	-DSUPPORT_SGX \
	-DSUPPORT_PERCONTEXT_PB \
	-DSUPPORT_LINUX_X86_WRITECOMBINE \
	-DTRANSFER_QUEUE \
	-DSUPPORT_DRI_DRM \
	-DSYS_USING_INTERRUPTS \
	-DSUPPORT_HW_RECOVERY \
	-DPVR_SECURE_HANDLES \
	-DUSE_PTHREADS \
	-DSUPPORT_SGX_EVENT_OBJECT \
	-DSUPPORT_SGX_LOW_LATENCY_SCHEDULING \
	-DSUPPORT_LINUX_X86_PAT \
	-DPVR_PROC_USE_SEQ_FILE


# Defaults for useful things on the Intel CE platform.
ccflags-$(CONFIG_X86_INTEL_CE) += -DPVRSRV_MODNAME="\"$(PVRSRV_MODNAME)"\"

ccflags-$(CONFIG_X86_INTEL_CE) += -DINTEL_D3_CHANGES=1
ccflags-$(CONFIG_X86_INTEL_CE) += -DINTEL_D3_PAD=1
ccflags-$(CONFIG_X86_INTEL_CE) += -DINTEL_D3_FLUSH=1
ccflags-$(CONFIG_X86_INTEL_CE) += -DINTEL_D3_DISABLE_TEXTURE_STREAM=1
#ccflags-$(CONFIG_X86_INTEL_CE) += -DINTEL_D3_GDL=1
#ccflags-$(CONFIG_X86_INTEL_CE) += -DINTEL_D3_NO_PCI_ENUM=1
ccflags-$(CONFIG_X86_INTEL_CE) += -DINTEL_D3_P_CHANGES=1
ccflags-$(CONFIG_X86_INTEL_CE) += -DINTEL_D3_CACHED_CBUF=1
ccflags-$(CONFIG_X86_INTEL_CE) += -DINTEL_D3_MEEGO=1

# FIXME not sure if we need these
#ccflags-$(CONFIG_CE_RELEASE) += -DUSE_FBDEV
#ccflags-$(CONFIG_CE_RELEASE) += -DFBDEV_NAME="\"$(FBDEV_NAME)\""


ccflags-$(CONFIG_CE_RELEASE) += -DBUILD="\"release\"" -DPVR_BUILD_TYPE="\"release\"" -DRELEASE

ccflags-$(CONFIG_CE_DEBUG) += -DBUILD="\"debug\"" -DPVR_BUILD_TYPE="\"debug\"" -DDEBUG
ccflags-$(CONFIG_CE_DEBUG) += -DDEBUG_LINUX_MEMORY_ALLOCATIONS
ccflags-$(CONFIG_CE_DEBUG) += -DDEBUG_LINUX_MEM_AREAS
ccflags-$(CONFIG_CE_DEBUG) += -DDEBUG_LINUX_MMAP_AREAS
#ccflags-$(CONFIG_CE_DEBUG) += -DDEBUG_LINUX_XML_PROC_FILES
#ccflags-$(CONFIG_CE_DEBUG) += -DDEBUG_LINUX_SLAB_ALLOCATIONS
ccflags-$(CONFIG_CE_DEBUG) += -DDEBUG_BRIDGE_KM
#ccflags-$(CONFIG_CE_DEBUG) += -DDEBUG_TRACE_BRIDGE_KM
#ccflags-$(CONFIG_CE_DEBUG) += -DDEBUG_BRIDGE_KM_DISPATCH_TABLE
#ccflags-$(CONFIG_CE_DEBUG) += -DSUPPORT_HW_RECOVERY

COMMONDIR = common
BRIDGEDIR = bridged
SGXDIR = devices/sgx
SYSDIR = system/sgx_intel_ce
SYSCONFIGDIR = system/sgx_intel_ce

pvrsrvkm-y := osfunc.o \
	 mmap.o \
	 module.o \
	 pdump.o \
	 proc.o \
	 pvr_bridge_k.o \
	 pvr_debug.o \
	 pvr_drm.o \
	 mm.o \
	 mutils.o \
	 event.o \
	 osperproc.o

pvrsrvkm-y += $(COMMONDIR)/buffer_manager.o \
	$(COMMONDIR)/devicemem.o \
	$(COMMONDIR)/deviceclass.o \
	$(COMMONDIR)/handle.o \
	$(COMMONDIR)/hash.o \
	$(COMMONDIR)/metrics.o \
	$(COMMONDIR)/pvrsrv.o \
	$(COMMONDIR)/queue.o \
	$(COMMONDIR)/ra.o \
	$(COMMONDIR)/resman.o \
	$(COMMONDIR)/power.o \
	$(COMMONDIR)/mem.o \
	$(COMMONDIR)/pdump_common.o \
	$(COMMONDIR)/perproc.o \
	$(COMMONDIR)/lists.o

pvrsrvkm-y += $(BRIDGEDIR)/bridged_support.o \
	$(BRIDGEDIR)/bridged_pvr_bridge.o \
	$(BRIDGEDIR)/sgx/bridged_sgx_bridge.o

pvrsrvkm-y += $(SGXDIR)/sgxinit.o \
	$(SGXDIR)/sgxpower.o \
	$(SGXDIR)/sgxreset.o \
	$(SGXDIR)/sgxutils.o \
	$(SGXDIR)/sgxkick.o \
	$(SGXDIR)/sgxtransfer.o \
	$(SGXDIR)/mmu.o \
	$(SGXDIR)/pb.o

pvrsrvkm-y += $(SYSCONFIGDIR)/sysconfig.o \
	$(SYSCONFIGDIR)/sysutils.o

obj-$(CONFIG_CE_SGX_GFX) += pvrsrvkm.o
