/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_GETEVENTS_H__
#define __LIVEJOURNAL_GETEVENTS_H__

#include <livejournal/verb.h>
#include <livejournal/entry.h>

typedef enum {
	LJ_GETEVENTS_MODE_SINGLE,
	LJ_GETEVENTS_MODE_RECENT,
} LJGetEventsMode;

typedef struct _LJGetEvents {
	LJVerb verb;
	GList *warnings;
} LJGetEvents;

typedef struct _LJGetEventsSingle {
	LJGetEvents getevents;
	LJEntry *entry;
} LJGetEventsSingle;

LJGetEventsSingle* lj_getevents_single_new(LJUser *user, const char *usejournal, int itemid);
void               lj_getevents_single_free(LJGetEventsSingle *getevents, gboolean includeevent);

LJGetEvents* lj_getevents_new_recent(LJUser *user, const char *usejournal, gboolean summary, int count);

typedef struct _LJGetEventsSync {
	LJGetEvents getevents;
	int entry_count;
	LJEntry **entries;
	GSList *warnings;
} LJGetEventsSync;

LJGetEventsSync* lj_getevents_sync_new(LJUser *user, const char *usejournal,
                                       const char *lastsync);
void             lj_getevents_sync_free(LJGetEventsSync *getevents,
                                        gboolean includeentries);

typedef struct _LJGetEventsRecent {
	LJGetEvents getevents;
	int entry_count;
	LJEntry **entries;
} LJGetEventsRecent;

LJGetEventsRecent*
lj_getevents_recent_new(LJUser *user, const char *usejournal, int howmany,
                        const char *beforedate, gboolean summary, int truncate);
void
lj_getevents_recent_free(LJGetEventsRecent *getevents, gboolean includeentries);

#endif /* __LIVEJOURNAL_GETEVENTS_H__ */

