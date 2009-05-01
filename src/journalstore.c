/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"
#include <errno.h>

#include "conf.h"
#include "journalstore.h"

gboolean
journal_store_find_relative(JournalStore *js, gint itemid,
		int *ritemid, int dir, GError *err) {
	time_t when = journal_store_lookup_entry_time(js, itemid);

	return journal_store_find_relative_by_time(js, when,
			ritemid, dir, err);
}

char*
journal_store_get_lastsync(JournalStore *js) {
	char *filename;
	char *sync = NULL;

	if (journal_store_get_invalid(js)) {
		/* the journalstore was invalid; we need to resync. */
		return NULL;
	}

	filename = conf_make_account_path(journal_store_get_account(js),
									  "lastsync");
	if (g_file_get_contents(filename, &sync, NULL, NULL)) {
		g_strchomp(sync);
	} else {
		sync = NULL;
	}
	g_free(filename);
	return sync;
}

gboolean
journal_store_put_lastsync(JournalStore *js, const char *lastsync, GError **err) {
	char *filename;
	FILE *f;

	filename = conf_make_account_path(journal_store_get_account(js),
									  "lastsync");
	f = fopen(filename, "w");
	if (f == NULL) {
		g_set_error(err, 0, 0, _("Can't open '%s': %s"), filename,
		            g_strerror(errno));
		g_free(filename);
		return FALSE;
	}
	fprintf(f, "%s\n", lastsync);
	fclose(f);

	g_free(filename);
	return TRUE;
}


