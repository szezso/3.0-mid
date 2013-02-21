/**********************************************************************
 *
 * Copyright (c) 2009-2010 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 *     Intel Corporation
 *     2200 Mission College Blvd.
 *     Santa Clara, CA  97052
 *
 ******************************************************************************/
#ifdef INTEL_D3_PM

#include "graphics_pm.h"
#include "intel_ce_pm.h"

#include "services.h"
#include "power.h"

#include "osal.h"

//#define GRAPHICS_PM_DEBUG_PRINT printk
#define GRAPHICS_PM_DEBUG_PRINT(x...)

#define GRAPHICS_PM_COMPONENT_NAME "graphics"

#define GRAPHICS_PM_STATE_UNINITIALIZED  (-1)
#define GRAPHICS_PM_STATE_SUSPENDED      ( 0)
#define GRAPHICS_PM_STATE_IDLE             ( 1)
#define GRAPHICS_PM_STATE_BUSY           ( 2)

/* graphics_pm_state is used to track the power management state.
 *
 * Expected state transitions are:
 * From:         To:           By:
 * UNINITIALIZED IDLE          graphics_pm_init()
 * SUSPENDED     UNINITIALIZED graphics_pm_deinit()
 * IDLE          UNINITIALIZED graphics_pm_deinit()
 * BUSY          UNINITIALIZED graphics_pm_deinit()
 * IDLE          SUSPENDED     graphics_pm_suspend()
 * SUSPENDED     IDLE          graphics_pm_resume()
 * IDLE          BUSY          graphics_pm_set_busy()
 * BUSY          IDLE          graphics_pm_set_idle()
 *
 * Other state transitions are not allowed.  In particular, direct transition
 * from BUSY to SUSPENDED by graphics_pm_suspend() is rejected and the only
 * valid transition from UNINITIALIZED is to IDLE by graphics_pm_init().
 *
 */

volatile static int graphics_pm_state = GRAPHICS_PM_STATE_UNINITIALIZED;

static os_sema_t graphics_pm_semaphore;

static int graphics_pm_suspend(struct pci_dev *dev, pm_message_t state)
{
	int rc = GRAPHICS_PM_OK;
	int ret_val = 0;
	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_suspend: Hello\n");

	assert(GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state);
	if (GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state) {
		os_sema_get(&graphics_pm_semaphore);
		if (GRAPHICS_PM_STATE_BUSY == graphics_pm_state) {
			rc = GRAPHICS_PM_ERR_FAILED;
		}
		if (GRAPHICS_PM_STATE_IDLE == graphics_pm_state) {
			if (PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_D3) !=
			    PVRSRV_OK) {
				rc = GRAPHICS_PM_ERR_FAILED;
			}

			if(GRAPHICS_PM_OK == rc)
			{
				icepm_ret_t icepm_rc = ICEPM_OK;
				icepm_rc = icepm_set_power_state(GRAPHICS_PM_COMPONENT_NAME,
							ICEPM_D3);
				if(ICEPM_OK != icepm_rc)
				{
					GRAPHICS_PM_DEBUG_PRINT ("graphics_pm_suspend: icepm_set_power_state %d\n",icepm_rc);
					rc = GRAPHICS_PM_ERR_FAILED;
					// Ignore error from icepm_set_power_state
					rc = GRAPHICS_PM_OK;
				}
			}

			if (GRAPHICS_PM_OK == rc) {
				graphics_pm_state = GRAPHICS_PM_STATE_SUSPENDED;
			}
		}
		os_sema_put(&graphics_pm_semaphore);
	} else {
		rc = GRAPHICS_PM_ERR_FAILED;
	}

	if (GRAPHICS_PM_OK == rc) {
		ret_val = 0;
	} else {
		ret_val = -EBUSY;
	}

	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_suspend: Goodbye\n");
	return ret_val;
}

static int graphics_pm_resume(struct pci_dev *dev)
{
	int rc = GRAPHICS_PM_OK;
	int ret_val = 0;
	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_resume: Hello\n");

	assert(GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state);
	if (GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state) {
		os_sema_get(&graphics_pm_semaphore);
		if (GRAPHICS_PM_STATE_SUSPENDED == graphics_pm_state) {
			{
				icepm_ret_t icepm_rc = ICEPM_OK;
				icepm_rc = icepm_set_power_state(GRAPHICS_PM_COMPONENT_NAME,.
						ICEPM_D0);
				if(ICEPM_OK != icepm_rc)
				{
					GRAPHICS_PM_DEBUG_PRINT ("graphics_pm_resume: icepm_set_power_state %d\n",icepm_rc);
					rc = GRAPHICS_PM_ERR_FAILED;
					// Ignore error from icepm_set_power_state
					rc = GRAPHICS_PM_OK;
				}
			}

			if(GRAPHICS_PM_OK == rc).
			{
				if(PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_D0) != PVRSRV_OK)
				{
					rc = GRAPHICS_PM_ERR_FAILED;
				}
			}

			if (GRAPHICS_PM_OK == rc) {
				graphics_pm_state = GRAPHICS_PM_STATE_IDLE;
			}
		}
		os_sema_put(&graphics_pm_semaphore);
	} else {
		rc = GRAPHICS_PM_ERR_FAILED;
	}

	if (GRAPHICS_PM_OK == rc) {
		ret_val = 0;
	} else {
		ret_val = -EBUSY;
	}

	GRAPHICS_PM_DEBUG_PRINT ("graphics_pm_resume: Goodbye %d\n", ret_val);
	return ret_val;
}

static icepm_functions_t graphics_pm_functions = {
	graphics_pm_suspend,
	graphics_pm_resume
};

int graphics_pm_init(void)
{
	int rc = GRAPHICS_PM_OK;
	icepm_ret_t icepm_rc = ICEPM_OK;
	osal_result osal_rc = OSAL_SUCCESS;
	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_init: Hello\n");

	assert(GRAPHICS_PM_STATE_UNINITIALIZED == graphics_pm_state);
	if (GRAPHICS_PM_STATE_UNINITIALIZED == graphics_pm_state) {
		osal_rc = os_sema_init(&graphics_pm_semaphore, 1);
		if (OSAL_SUCCESS != osal_rc) {
			rc = GRAPHICS_PM_ERR_FAILED;
		}

		if (GRAPHICS_PM_OK == rc) {
			os_sema_get(&graphics_pm_semaphore);

			icepm_rc =
			    icepm_device_register(GRAPHICS_PM_COMPONENT_NAME,
						  &graphics_pm_functions);
			if (ICEPM_OK != icepm_rc) {
				rc = GRAPHICS_PM_ERR_FAILED;
			}

			if (GRAPHICS_PM_OK == rc) {
				graphics_pm_state = GRAPHICS_PM_STATE_IDLE;
			}

			os_sema_put(&graphics_pm_semaphore);
		}
	} else {
		rc = GRAPHICS_PM_ERR_FAILED;
	}

	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_init: Goodbye\n");
	return rc;
}

int graphics_pm_deinit(void)
{
	int rc = GRAPHICS_PM_OK;
	icepm_ret_t icepm_rc = ICEPM_OK;
	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_deinit: Hello\n");

	assert(GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state);
	if (GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state) {
		os_sema_get(&graphics_pm_semaphore);

		icepm_rc = icepm_device_unregister(GRAPHICS_PM_COMPONENT_NAME);
		if (ICEPM_OK != icepm_rc) {
			rc = GRAPHICS_PM_ERR_FAILED;
		}

		if (GRAPHICS_PM_OK == rc) {
			graphics_pm_state = GRAPHICS_PM_STATE_UNINITIALIZED;
		}

		os_sema_put(&graphics_pm_semaphore);
		os_sema_destroy(&graphics_pm_semaphore);
	} else {
		rc = GRAPHICS_PM_ERR_FAILED;
	}

	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_deinit: Goodbye\n");
	return rc;
}

void graphics_pm_set_idle(void)
{
	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_set_idle: Hello\n");

	assert(GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state);
	if (GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state) {
		os_sema_get(&graphics_pm_semaphore);

		assert(GRAPHICS_PM_STATE_BUSY == graphics_pm_state);
		if (GRAPHICS_PM_STATE_BUSY == graphics_pm_state) {
			graphics_pm_state = GRAPHICS_PM_STATE_IDLE;
		}
		os_sema_put(&graphics_pm_semaphore);
	}

	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_set_idle: Goodbye\n");
	return;
}

void graphics_pm_set_busy(void)
{
	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_set_busy: Hello\n");

	assert(GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state);
	if (GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state) {
		os_sema_get(&graphics_pm_semaphore);

		assert(GRAPHICS_PM_STATE_IDLE == graphics_pm_state);
		if (GRAPHICS_PM_STATE_IDLE == graphics_pm_state) {
			graphics_pm_state = GRAPHICS_PM_STATE_BUSY;
		}
		os_sema_put(&graphics_pm_semaphore);
	}

	GRAPHICS_PM_DEBUG_PRINT("graphics_pm_set_busy: Goodbye\n");
	return;
}

void graphics_pm_wait_not_suspended(void)
{
	assert(GRAPHICS_PM_STATE_UNINITIALIZED != graphics_pm_state);

	while (GRAPHICS_PM_STATE_SUSPENDED == graphics_pm_state) {
		OS_SLEEP(1);
	}

	return;
}

#endif
