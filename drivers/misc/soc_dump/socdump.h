
#ifndef _SOCDUMP_H
#define _SOCDUMP_H


#include <linux/types.h>
#include "register_info.h"

struct dump_source {
	struct list_head	entry;
	spinlock_t		lock;
	int			count;
	void (*dump_regs) (void *buf);
};

extern struct dump_source *dump_source_create(void);
extern void dump_source_destroy(struct dump_source *ds);
extern void dump_source_add(struct dump_source *ds);
extern void dump_source_remove(struct dump_source *ds);

#endif /* _SOCDUMP_H */
