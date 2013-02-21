#ifndef _PVR_DEBUGFS_H_
#define _PVR_DEBUGFS_H_ 1

#ifdef CONFIG_DEBUG_FS
void pvr_debugfs_hwrec_create_snapshot(PVRSRV_DEVICE_NODE *dev_node);
int pvr_debugfs_init(void);
void pvr_debugfs_cleanup(void);
#else
static inline void pvr_debugfs_hwrec_create_snapshot(
		PVRSRV_DEVICE_NODE *dev_node)
{
}
static inline int pvr_debugfs_init(void)
{
	return 0;
}
static inline void pvr_debugfs_cleanup(void)
{
}
#endif

#endif /* _PVR_DEBUGFS_H_ */
