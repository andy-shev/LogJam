/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <glib.h>

#include "getevents.h"
#include "syncitems.h"
#include "sync.h"

typedef struct _Sync {
	LJUser *user;
	const char *usejournal;

	char *lastsync;
	int synced;
	int total;

	gboolean sync_complete;
	GHashTable *syncitems;

	GError **err;

	LJPutLastSyncCallback put_lastsync;
	LJRunVerbCallback run_verb;
	LJPutEntriesCallback put_entries;
	LJSyncProgressCallback sync_progress;
	gpointer user_data;
} Sync;

static int
entry_date_compare_func(const void *a, const void *b, void *unused) {
	/* struct tm* cast needed to un-constify the times passed to mktime. */
	const LJEntry *entrya = *(LJEntry **)a;
	const LJEntry *entryb = *(LJEntry **)b;
	time_t timea = mktime((struct tm*)&entrya->time);
	time_t timeb = mktime((struct tm*)&entryb->time);
	/* mktime actually converts the times to local time, which isn't
	 * quite correct, but since we're comparing times directly like this
	 * it should still sort the same way and timegm is potentially slower. */
	if (timea < timeb) return -1;
	if (timea > timeb) return 1;
	return 0;
}

static void
sync_report_progress(Sync *sync, LJSyncProgress progress) {
	sync->sync_progress(sync->user_data, progress,
	                    sync->synced, sync->total, sync->lastsync);
}

static time_t
run_one_getevents(Sync *sync, GSList **warnings) {
	LJGetEventsSync *getevents;
	int i;
	time_t lastsync = 0;

	getevents = lj_getevents_sync_new(sync->user, sync->usejournal,
	                                  sync->lastsync);
	if (!sync->run_verb(sync->user_data, (LJVerb*)getevents, sync->err)) {
		lj_getevents_sync_free(getevents, TRUE);
		return 0;
	}
	if (getevents->entry_count == 0) {
		g_warning("getevents returned no entries!?\n");
		lj_getevents_sync_free(getevents, TRUE);
		return 0;
	}

	/* g_print("lastsync %s : got %d entries\n",
			sync->lastsync ? sync->lastsync : "(none)", getevents->entry_count);
	*/

	/* remove off all the syncitems for the entries we have now received,
	 * also scanning for the latest time we managed to sync. */
	for (i = 0; i < getevents->entry_count; i++) {
		int id = getevents->entries[i]->itemid;
		time_t itemtime = GPOINTER_TO_INT(
				g_hash_table_lookup(sync->syncitems, GINT_TO_POINTER(id)));

		if (itemtime) {
			sync->synced++;

			/* update latest time. */
			if (itemtime > lastsync)
				lastsync = itemtime;

			/* pop this off the syncitems list. */
			g_hash_table_remove(sync->syncitems, GINT_TO_POINTER(id));
		}
	}

	/* a sync returns a list in update order,
	 * and we don't want to depend on that order anyway.
	 * we want them ordered by entry time. */
	g_qsort_with_data(getevents->entries, getevents->entry_count,
	                  sizeof(LJEntry*), entry_date_compare_func, NULL);
	/* and finally, put these to disk. */
	if (!sync->put_entries(sync->user_data,
				getevents->entries, getevents->entry_count, sync->err)) {
		lastsync = 0;  /* signal to caller that we failed. */
	}

	lj_getevents_sync_free(getevents, TRUE);

	return lastsync;
}

static gboolean
run_one_syncitems(Sync *sync) {
	LJSyncItems *syncitems;

	syncitems = lj_syncitems_new(sync->user, sync->usejournal,
	                             sync->lastsync, sync->syncitems);
	if (!sync->run_verb(sync->user_data, (LJVerb*)syncitems, sync->err)) {
		lj_syncitems_free(syncitems, TRUE);
		sync->syncitems = NULL;
		return FALSE;
	}

	if (!sync->total)
		sync->total = syncitems->total;
	sync->synced = sync->total - (syncitems->total - syncitems->count);

	g_free(sync->lastsync);
	sync->lastsync = g_strdup(syncitems->lastsync);

	sync->syncitems = syncitems->items;
	lj_syncitems_free(syncitems, FALSE);

	return TRUE;
}

static gboolean
run_syncitems(Sync *sync) {
	do {
		if (!run_one_syncitems(sync))
			return FALSE;
		sync_report_progress(sync, LJ_SYNC_PROGRESS_ITEMS);
	} while (sync->synced < sync->total);

	return TRUE;
}

static gboolean
run_getevents(Sync *sync, GSList **warnings) {
	time_t lastsync;

	sync->synced = 0;
	sync->total = g_hash_table_size(sync->syncitems);

	sync_report_progress(sync, LJ_SYNC_PROGRESS_ENTRIES);

	while (g_hash_table_size(sync->syncitems) > 0) {
		lastsync = run_one_getevents(sync, warnings);
		if (lastsync <= 0)
			return FALSE;

		/* now that we've successfully synced this batch of entries,
		 * let the caller know; they should write it out to disk,
		 * so if we're cancelled before we complete,
		 * we can resume at this point instead of starting over. */
		g_free(sync->lastsync);
		sync->lastsync = lj_tm_to_ljdate(gmtime(&lastsync));
		if (!sync->put_lastsync(sync->user_data, sync->lastsync, sync->err))
			return FALSE;

		sync_report_progress(sync, LJ_SYNC_PROGRESS_ENTRIES);
	}

	return TRUE;
}

gboolean
lj_sync_run(LJUser *user, const char *usejournal,
            const char *lastsync, LJPutLastSyncCallback put_lastsync_cb,
            LJRunVerbCallback run_verb_cb, LJPutEntriesCallback put_entries_cb,
            LJSyncProgressCallback sync_progress_cb,
			gpointer user_data, GSList **warnings, GError **err) {
	Sync st = {0}, *sync = &st;
	gboolean success = FALSE;

	sync->user = user;
	sync->usejournal = usejournal;
	sync->lastsync = lastsync ? g_strdup(lastsync) : NULL;
	sync->put_lastsync = put_lastsync_cb;
	sync->run_verb = run_verb_cb;
	sync->put_entries = put_entries_cb;
	sync->sync_progress = sync_progress_cb;
	sync->user_data = user_data;
	sync->err = err;

	if (run_syncitems(sync)) {
		g_free(sync->lastsync);
		sync->lastsync = lastsync ? g_strdup(lastsync) : NULL;
		if (run_getevents(sync, warnings))
			success = TRUE;
	}

	g_free(sync->lastsync);
	if (sync->syncitems)
		g_hash_table_destroy(sync->syncitems);
	return success;
}

