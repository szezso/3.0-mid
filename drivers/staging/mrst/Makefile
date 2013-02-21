ccflags-y += \
	-I$(srctree)/$(src)/bc_video \
	-I$(srctree)/$(src)/drv \
	-I$(srctree)/$(src)/imgv \
	-I$(srctree)/$(src)/pvr \
	-I$(srctree)/$(src)/pvr/include4 \
	-I$(srctree)/$(src)/pvr/services4/3rdparty/linux_framebuffer_mrst \
	-I$(srctree)/$(src)/pvr/services4/include \
	-I$(srctree)/$(src)/pvr/services4/include/env/linux \
	-I$(srctree)/$(src)/pvr/services4/srvkm/bridged \
	-I$(srctree)/$(src)/pvr/services4/srvkm/bridged/sgx \
	-I$(srctree)/$(src)/pvr/services4/srvkm/common \
	-I$(srctree)/$(src)/pvr/services4/srvkm/devices/sgx \
	-I$(srctree)/$(src)/pvr/services4/srvkm/env/linux \
	-I$(srctree)/$(src)/pvr/services4/srvkm/hwdefs \
	-I$(srctree)/$(src)/pvr/services4/srvkm/include \
	-I$(srctree)/$(src)/pvr/services4/system/include \
	-I$(srctree)/$(src)/pvr/services4/system/unified \
	-I$(srctree)/include/drm \
	-Iinclude/linux

ccflags-y += -DSGX540 -DSUPPORT_SGX540 -DSGX_CORE_REV=121

ccflags-y += \
	-DDEBUG_MESA_OGL_TRACE \
	-DDISABLE_PM \
	-DDISPLAY_CONTROLLER=mrstlfb \
	-DLINUX \
	-DPVR2D_VALIDATE_INPUT_PARAMS \
	-DPVR_BUILD_DIR="\"pc_i686_moorestown_linux\"" \
	-DPVR_LINUX_MISR_USING_PRIVATE_WORKQUEUE \
	-DPVR_LINUX_MISR_USING_WORKQUEUE \
	-DPVR_LINUX_TIMERS_USING_WORKQUEUES \
	-DPVR_LINUX_USING_WORKQUEUES \
	-DPVR_PROC_USE_SEQ_FILE \
	-DPVR_SECURE_HANDLES \
	-DSERVICES4 \
	-DSUPPORT_ACTIVE_POWER_MANAGEMENT \
	-DSUPPORT_CACHE_LINE_FLUSH \
	-DSUPPORT_CPU_CACHED_BUFFERS \
	-DSUPPORT_DRI_DRM \
	-DSUPPORT_DRI_DRM_EXT \
	-DSUPPORT_HW_RECOVERY \
	-DSUPPORT_LINUX_X86_PAT \
	-DSUPPORT_LINUX_X86_WRITECOMBINE \
	-DSUPPORT_MEMINFO_IDS \
	-DSUPPORT_PERCONTEXT_PB \
	-DSUPPORT_SGX \
	-DSUPPORT_SGX_EVENT_OBJECT \
	-DSUPPORT_SGX_HWPERF \
	-DSUPPORT_SGX_LOW_LATENCY_SCHEDULING \
	-DSUPPORT_SGX_NEW_STATUS_VALS \
	-DSUPPORT_SRVINIT \
	-DSYS_USING_INTERRUPTS \
	-DTRANSFER_QUEUE \
	-DUSE_PTHREADS \
	-D_XOPEN_SOURCE=600 \
	-UDEBUG_LOG_PATH_TRUNCATE

ccflags-$(CONFIG_PVR_ABI_RELEASE) += \
	-DBUILD="\"release\"" \
	-DPVR_BUILD_TYPE="\"release\"" \
	-DRELEASE

ccflags-$(CONFIG_PVR_ABI_DEBUG) += \
	-DBUILD="\"debug\"" \
	-DPVR_BUILD_TYPE="\"debug\"" \
	-DPVR_DEBUG_EXT \
	-DDEBUG_BRIDGE_KM \
	-DDEBUG_LINUX_MEMORY_ALLOCATIONS \
	-DDEBUG_LINUX_MEM_AREAS \
	-DDEBUG_LINUX_MMAP_AREAS

ccflags-$(CONFIG_PVR_DEBUG) += -DDEBUG
ccflags-$(CONFIG_PCI_MSI) += -DCONFIG_PCI_MSI
ccflags-$(CONFIG_MDFD_GL3) += -DSUPPORT_EXTERNAL_SYSTEM_CACHE

ccflags-$(CONFIG_MDFD_HDMI) += -DHDMI_MODE_SETTING

PVRSRV_RESOURCE_PROFILING =
PERPROC_LIST =
RES_MAN_EXTEND =
PVRSRV_OS_MEM_INFO =

ccflags-$(PVRSRV_RESOURCE_PROFILING) += -DPVRSRV_RESOURCE_PROFILING
ccflags-$(PERPROC_LIST) += -DPERPROC_LIST
ccflags-$(RES_MAN_EXTEND) += -DRES_MAN_EXTEND
ccflags-$(PVRSRV_OS_MEM_INFO) += -DPVRSRV_OS_MEM_INFO

PVRDIR = pvr
ENVDIR = pvr/services4/srvkm/env/linux
COMMONDIR = pvr/services4/srvkm/common
BRIDGEDDIR = pvr/services4/srvkm/bridged
SGXDIR = pvr/services4/srvkm/devices/sgx
FBDEVDIR = pvr/services4/3rdparty/linux_framebuffer_mrst
DRMDRVDIR = drv
SYSCONFIGDIR = pvr/services4/system/unified
IMGVDIR = imgv
BUFFER_CLASS_DIR = bc_video

medfield_gfx-y += \
	$(PVRDIR)/pvr_debug_core.o

medfield_gfx-y += \
	$(ENVDIR)/event.o \
	$(ENVDIR)/mm.o \
	$(ENVDIR)/mmap.o \
	$(ENVDIR)/module.o \
	$(ENVDIR)/mutex.o \
	$(ENVDIR)/mutils.o \
	$(ENVDIR)/osfunc.o \
	$(ENVDIR)/osperproc.o \
	$(ENVDIR)/ossync.o \
	$(ENVDIR)/pdump.o \
	$(ENVDIR)/proc.o \
	$(ENVDIR)/pvr_bridge_k.o \
	$(ENVDIR)/pvr_debug.o \
	$(ENVDIR)/pvr_drm.o

medfield_gfx-y += \
	$(COMMONDIR)/buffer_manager.o \
	$(COMMONDIR)/deviceclass.o \
	$(COMMONDIR)/devicemem.o \
	$(COMMONDIR)/handle.o \
	$(COMMONDIR)/hash.o \
	$(COMMONDIR)/lists.o \
	$(COMMONDIR)/mem.o \
	$(COMMONDIR)/mem_debug.o \
	$(COMMONDIR)/metrics.o \
	$(COMMONDIR)/osfunc_common.o \
	$(COMMONDIR)/pdump_common.o \
	$(COMMONDIR)/perproc.o \
	$(COMMONDIR)/power.o \
	$(COMMONDIR)/pvrsrv.o \
	$(COMMONDIR)/queue.o \
	$(COMMONDIR)/ra.o \
	$(COMMONDIR)/resman.o

medfield_gfx-y += \
	$(BRIDGEDDIR)/bridged_pvr_bridge.o \
	$(BRIDGEDDIR)/bridged_support.o \
	$(BRIDGEDDIR)/sgx/bridged_sgx_bridge.o

medfield_gfx-y += \
	$(SYSCONFIGDIR)/sys_pvr_drm_export.o \
	$(SYSCONFIGDIR)/sysconfig.o \
	$(SYSCONFIGDIR)/sysutils.o

medfield_gfx-y += \
	$(SGXDIR)/mmu.o \
	$(SGXDIR)/pb.o \
	$(SGXDIR)/sgxinit.o \
	$(SGXDIR)/sgxkick.o \
	$(SGXDIR)/sgxpower.o \
	$(SGXDIR)/sgxreset.o \
	$(SGXDIR)/sgxtransfer.o \
	$(SGXDIR)/sgxutils.o

medfield_gfx-y += \
	$(FBDEVDIR)/mrstlfb_displayclass.o \
	$(FBDEVDIR)/mrstlfb_linux.o

medfield_gfx-y += \
	$(DRMDRVDIR)/drm_flip.o \
	$(DRMDRVDIR)/fp_trig.o \
	$(DRMDRVDIR)/mdfld_dsi_dbi.o \
	$(DRMDRVDIR)/mdfld_dsi_dpi.o \
	$(DRMDRVDIR)/mdfld_dsi_output.o \
	$(DRMDRVDIR)/mdfld_dsi_pkg_sender.o \
	$(DRMDRVDIR)/mdfld_hdmi_audio.o \
	$(DRMDRVDIR)/mdfld_intel_hdcp.o \
	$(DRMDRVDIR)/mdfld_msic.o \
	$(DRMDRVDIR)/mdfld_output.o \
	$(DRMDRVDIR)/mdfld_overlay.o \
	$(DRMDRVDIR)/psb_bl.o \
	$(DRMDRVDIR)/psb_drv.o \
	$(DRMDRVDIR)/psb_fb.o \
	$(DRMDRVDIR)/psb_gtt.o \
	$(DRMDRVDIR)/psb_hotplug.o \
	$(DRMDRVDIR)/psb_intel_display.o \
	$(DRMDRVDIR)/psb_intel_hdmi.o \
	$(DRMDRVDIR)/psb_intel_hdmi_i2c.o \
	$(DRMDRVDIR)/psb_irq.o \
	$(DRMDRVDIR)/psb_page_flip.o \
	$(DRMDRVDIR)/psb_powermgmt.o \
	$(DRMDRVDIR)/psb_pvr_glue.o \
	$(DRMDRVDIR)/psb_reset.o \
	$(DRMDRVDIR)/psb_schedule.o \
	$(DRMDRVDIR)/psb_sgx.o \
	$(DRMDRVDIR)/psb_socket.o \
	$(DRMDRVDIR)/psb_umevents.o \
	$(DRMDRVDIR)/tc35876x-dsi-lvds.o \
	$(DRMDRVDIR)/tmd_vid.o \
	$(DRMDRVDIR)/tpo_cmd.o \
	$(DRMDRVDIR)/tpo_vid.o

medfield_gfx-y += \
	$(IMGVDIR)/msvdx_power.o \
	$(IMGVDIR)/pnw_topaz.o \
	$(IMGVDIR)/pnw_topazinit.o \
	$(IMGVDIR)/psb_buffer.o \
	$(IMGVDIR)/psb_fence.o \
	$(IMGVDIR)/psb_mmu.o \
	$(IMGVDIR)/psb_msvdx.o \
	$(IMGVDIR)/psb_msvdxinit.o \
	$(IMGVDIR)/psb_ttm_fence.o \
	$(IMGVDIR)/psb_ttm_fence_user.o \
	$(IMGVDIR)/psb_ttm_glue.o \
	$(IMGVDIR)/psb_ttm_placement_user.o \
	$(IMGVDIR)/topaz_power.o

medfield_gfx-y += \
	$(IMGVDIR)/psb_video_bind.o

medfield_gfx-y += \
	$(BUFFER_CLASS_DIR)/bufferclass_video.o \
	$(BUFFER_CLASS_DIR)/bufferclass_video_linux.o


medfield_gfx-$(CONFIG_MDFLD_DSI_DPU) += $(DRMDRVDIR)/mdfld_dsi_dbi_dpu.o
medfield_gfx-$(CONFIG_MDFD_GL3) += $(DRMDRVDIR)/mdfld_gl3.o

medfield_gfx-$(CONFIG_DEBUG_FS) += $(PVRDIR)/pvr_debugfs.o
medfield_gfx-$(CONFIG_PVR_TRACE_CMD) += $(PVRDIR)/pvr_trace_cmd.o

include $(srctree)/$(src)/drv/otm_hdmi/Makefile
obj-$(CONFIG_DRM_MDFLD) += medfield_gfx.o
