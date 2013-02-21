#EXTRA_CFLAGS += -DDEBUG
#
# Makefile for Intel MID Audio drivers
#
SOC_SRCS := soc_audio
LINUXINCLUDE += -I$(srctree)/drivers/staging/intel_sst/soc_audio/ -I$(srctree)/drivers/staging/intel_sst/
snd-intel-sst-y := intel_sst.o intel_sst_ipc.o intel_sst_drv_interface.o  intel_sst_dsp.o intel_sst_stream_encoded.o intel_sst_app_interface.o intel_sst_loopback.o

snd-intel-sst-y += $(SOC_SRCS)/soc_audio_processor.o $(SOC_SRCS)/soc_ipc.o soc_audio_pipeline_specific.o $(SOC_SRCS)/soc_audio_pipeline.o $(SOC_SRCS)/soc_audio_inputs.o $(SOC_SRCS)/soc_audio_outputs.o $(SOC_SRCS)/soc_audio_core.o $(SOC_SRCS)/soc_audio_mpeg_config.o $(SOC_SRCS)/soc_audio_aac_config.o $(SOC_SRCS)/soc_audio_mixer.o
obj-$(CONFIG_SND_INTEL_SST) += snd-intel-sst.o
