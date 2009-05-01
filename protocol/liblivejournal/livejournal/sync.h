/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_SYNC_H__
#define __LIVEJOURNAL_SYNC_H__

#include <livejournal/verb.h>
#include <livejournal/entry.h>

typedef enum {
	LJ_SYNC_PROGRESS_ITEMS,
	LJ_SYNC_PROGRESS_ENTRIES,
} LJSyncProgress;

typedef gboolean (*LJPutLastSyncCallback) (gpointer, const char *, GError **);
typedef gboolean (*LJRunVerbCallback)     (gpointer, LJVerb *, GError **);
typedef gboolean (*LJPutEntriesCallback)  (gpointer, LJEntry **, int c, GError **);
typedef void     (*LJSyncProgressCallback)(gpointer, LJSyncProgress, int cur, int max, const char *date);

gboolean
lj_sync_run(LJUser *user, const char *usejournal,
            const char *lastsync, LJPutLastSyncCallback put_lastsync_cb,
            LJRunVerbCallback run_verb_cb, LJPutEntriesCallback put_entries_cb,
            LJSyncProgressCallback sync_progress_cb,
			gpointer user_data, GSList **warnings, GError **err);

#endif /* __LIVEJOURNAL_SYNC_H__ */
