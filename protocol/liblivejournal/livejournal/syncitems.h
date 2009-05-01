/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_SYNCITEMS_H__
#define __LIVEJOURNAL_SYNCITEMS_H__

#include <livejournal/verb.h>
#include <time.h>

typedef struct _LJSyncItems {
	LJVerb verb;

	GHashTable *items;   /* itemid -> time */

	/* note that count and total are just the values returned by syncitems,
	 * so count includes dups and total changes with each new batch. */
	int count, total;
	char *lastsync;
} LJSyncItems;

LJSyncItems* lj_syncitems_new(LJUser *user, const char *usejournal,
                              const char *lastsync, GHashTable *items);
void         lj_syncitems_free(LJSyncItems *syncitems, gboolean free_items);

#endif /* __LIVEJOURNAL_SYNCITEMS_H__ */

