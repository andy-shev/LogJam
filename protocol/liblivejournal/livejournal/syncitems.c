/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <glib.h>
#include <stdlib.h> /* atoi */

#include "syncitems.h"

static void
read_syncitems(LJSyncItems *syncitems, LJResult *result) {
	int i;
	time_t lastsync = 0;
	time_t itemtime;

	for (i = 1; i <= syncitems->count; i++) {
		char *item = lj_result_getf(result, "sync_%d_item", i);
		int id = atoi(item+2);
		struct tm tm;

		/* skip non-log items. */
		if (item[0] != 'L')
			continue;

		lj_ljdate_to_tm(lj_result_getf(result, "sync_%d_time", i), &tm);
		itemtime = lj_timegm(&tm);
		if (itemtime > lastsync)
			lastsync = itemtime;
		g_hash_table_insert(syncitems->items,
		                    GINT_TO_POINTER(id), GINT_TO_POINTER(itemtime));
	}
	if (lastsync > 0) {
		g_free(syncitems->lastsync);
		syncitems->lastsync = lj_tm_to_ljdate(gmtime(&lastsync));
	}
}


static void
parse_result(LJVerb *verb) {
	LJSyncItems *syncitems = (LJSyncItems*)verb;
	LJResult *result = verb->result;
	syncitems->count = lj_result_get_int(result, "sync_count");
	syncitems->total = lj_result_get_int(result, "sync_total");
	read_syncitems(syncitems, result);
}

LJSyncItems*
lj_syncitems_new(LJUser *user, const char *usejournal,
                 const char *lastsync, GHashTable *items) {
	LJSyncItems *syncitems = g_new0(LJSyncItems, 1);
	LJVerb *verb = (LJVerb*)syncitems;
	LJRequest *request;

	if (items)
		syncitems->items = items;
	else
		syncitems->items = g_hash_table_new(g_direct_hash, g_direct_equal);

	lj_verb_init(verb, user, "syncitems", FALSE, parse_result);
	request = verb->request;

	if (lastsync)
		lj_request_add(request, "lastsync", lastsync);

	return syncitems;
}

void
lj_syncitems_free(LJSyncItems *syncitems, gboolean free_items) {
	lj_verb_free_contents((LJVerb*)syncitems);
	g_free(syncitems->lastsync);
	if (free_items)
		g_hash_table_destroy(syncitems->items);
	g_free(syncitems);
}

