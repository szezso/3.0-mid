########################################################################
#
# This file is provided under a dual BSD/GPLv2 license.  When using or
# redistributing this file, you may do so under either license.
#
# GPL LICENSE SUMMARY
#
# Copyright(c) 2011 Intel Corporation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
# The full GNU General Public License is included in this distribution
# in the file called LICENSE.GPL.
#
# Contact Information:
# Intel Corporation
# 2200 Mission College Blvd.
# Santa Clara, CA  97052
#
# BSD LICENSE
#
# Copyright(c) 2011 Intel Corporation. All rights reserved.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
########################################################################

TARGET = otm_hdmi

OTM_HDMI_INCDIR = drv/otm_hdmi

ccflags-y += \
	-I$(srctree)/$(src)/drv/otm_hdmi/os/android/include \
	-I$(srctree)/$(src)/drv/otm_hdmi/pil/include \
	-I$(srctree)/$(src)/drv/otm_hdmi/pil/common \
	-I$(srctree)/$(src)/drv/otm_hdmi/pil/specific/include \
	-I$(srctree)/$(src)/drv/otm_hdmi/ipil/include \
	-I$(srctree)/$(src)/drv/otm_hdmi/ipil/common \
	-I$(srctree)/$(src)/drv/otm_hdmi/ipil/specific/include \
	-I$(srctree)/$(src)/drv/otm_hdmi/ipil/specific/mfld \
	-I$(srctree)/$(src)/drv/otm_hdmi/pil/specific/mfld \

#uncomment for debug prints
#ccflags-y += -DDEBUG

#only for internal testing
ccflags-y += -DOTM_HDMI_UNIT_TEST

#enable HDCP
ccflags-y += -DOTM_HDMI_HDCP_ENABLE

ifeq ($(CONFIG_MDFD_HDMI_GREENRIDGE),y)
ccflags-y += \
	-I$(srctree)/$(src)/drv/otm_hdmi/pil/specific/oktl \
	-I$(srctree)/$(src)/drv/otm_hdmi/ipil/specific/oktl-sdv
else ifeq ($(CONFIG_MDFD_HDMI_REDRIDGE),y)
ccflags-y += \
	-DMFLD_HDMI_DV1 \
	-I$(srctree)/$(src)/drv/otm_hdmi/pil/specific/mfld \
	-I$(srctree)/$(src)/drv/otm_hdmi/ipil/specific/mfld
else ifeq ($(CONFIG_MDFD_HDMI_PR2),y)
ccflags-y += \
	-DMFLD_HDMI_PR3 \
	-I$(srctree)/$(src)/drv/otm_hdmi/pil/specific/mfld \
	-I$(srctree)/$(src)/drv/otm_hdmi/ipil/specific/mfld
endif

# Platform independent library
$(TARGET)-y += \
	$(OTM_HDMI_INCDIR)/pil/common/otm_hdmi.o \
	$(OTM_HDMI_INCDIR)/pil/common/mode_info.o \
	$(OTM_HDMI_INCDIR)/pil/common/hdcp.o \
	$(OTM_HDMI_INCDIR)/pil/common/edid.o \
	$(OTM_HDMI_INCDIR)/pil/common/edid_print.o \
	$(OTM_HDMI_INCDIR)/pil/common/infoframes.o

# IP independent library
$(TARGET)-y += \
	$(OTM_HDMI_INCDIR)/ipil/common/otm_ipil_main.o \
	$(OTM_HDMI_INCDIR)/ipil/common/ipil_hdcp.o

# OS specific library
$(TARGET)-y += \
	$(OTM_HDMI_INCDIR)/os/android/android_hdmi.o
ifneq ($(CONFIG_MDFD_HDMI_REDRIDGE),y)
$(TARGET)-y += \
	$(OTM_HDMI_INCDIR)/os/android/android_hdmi_sw_scale.o
endif

ifeq ($(CONFIG_MDFD_HDMI_GREENRIDGE),y)
$(TARGET)-y += \
	$(OTM_HDMI_INCDIR)/ipil/specific/oktl-sdv/ips_hdmi.o \
	$(OTM_HDMI_INCDIR)/ipil/specific/oktl-sdv/ips_hdcp.o \
	$(OTM_HDMI_INCDIR)/pil/specific/oktl/ps_hdmi.o
else ifeq ($(CONFIG_MDFD_HDMI_REDRIDGE),y)
$(TARGET)-y += \
	$(OTM_HDMI_INCDIR)/ipil/specific/mfld/ips_hdmi.o \
	$(OTM_HDMI_INCDIR)/ipil/specific/mfld/ips_hdcp.o \
	$(OTM_HDMI_INCDIR)/pil/specific/mfld/ps_hdmi.o \
	$(OTM_HDMI_INCDIR)/pil/specific/mfld/ps_hdmi_tablet.o
else ifeq ($(CONFIG_MDFD_HDMI_PR2),y)
$(TARGET)-y += \
	$(OTM_HDMI_INCDIR)/ipil/specific/mfld/ips_hdmi.o \
	$(OTM_HDMI_INCDIR)/ipil/specific/mfld/ips_hdcp.o \
	$(OTM_HDMI_INCDIR)/pil/specific/mfld/ps_hdmi.o \
	$(OTM_HDMI_INCDIR)/pil/specific/mfld/ps_hdmi_phone.o
endif
obj-$(CONFIG_MDFD_HDMI) += $(TARGET).o
