/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>
#include <stdlib.h> /* atoi */

#include "entry.h"
#include "getevents.h"

static void
lj_getevents_init(LJGetEvents *getevents,
                  LJUser *user, const char *usejournal,
                  void (*parse_result)(LJVerb*)) {
	LJVerb *verb = (LJVerb*)getevents;

	lj_verb_init(verb, user, "getevents", FALSE, parse_result);
	if (usejournal)
		lj_request_add(verb->request, "usejournal", usejournal);
	lj_request_add(verb->request, "lineendings", "unix");
}

static void
single_parse_result(LJVerb *verb) {
	LJGetEventsSingle *getevents = (LJGetEventsSingle*)verb;
	getevents->entry = lj_entry_new_single_from_result(verb->result, NULL/*XXX*/);
}

LJGetEventsSingle*
lj_getevents_single_new(LJUser *user, const char *usejournal, int itemid) {
	LJGetEventsSingle *getevents = g_new0(LJGetEventsSingle, 1);
	lj_getevents_init((LJGetEvents*)getevents, user, usejournal,
			single_parse_result);
	lj_request_add(((LJVerb*)getevents)->request, "selecttype", "one");
	lj_request_add_int(((LJVerb*)getevents)->request, "itemid", itemid);
	return getevents;
}

void
lj_getevents_single_free(LJGetEventsSingle *getevents, gboolean includeentry) {
	lj_verb_free_contents((LJVerb*)getevents);
	if (includeentry && getevents->entry)
		lj_entry_free(getevents->entry);
	g_free(getevents);
}

static void
sync_parse_result(LJVerb *verb) {
	LJGetEventsSync *getevents = (LJGetEventsSync*)verb;

	getevents->entry_count = lj_result_get_int(verb->result, "events_count");
	getevents->entries =
		lj_entries_new_from_result(verb->result, getevents->entry_count,
		                           &getevents->warnings);
}

LJGetEventsSync*
lj_getevents_sync_new(LJUser *user, const char *usejournal,
                      const char *lastsync) {
	LJGetEventsSync *getevents = g_new0(LJGetEventsSync, 1);
	lj_getevents_init((LJGetEvents*)getevents, user, usejournal,
			sync_parse_result);
	lj_request_add(((LJVerb*)getevents)->request, "selecttype", "syncitems");
	if (lastsync)
		lj_request_add(((LJVerb*)getevents)->request, "lastsync", lastsync);
	return getevents;
}

void
lj_getevents_sync_free(LJGetEventsSync *getevents, gboolean includeentries) {
	lj_verb_free_contents((LJVerb*)getevents);
	g_free(getevents->entries);
	g_free(getevents);
}

static void
recent_parse_result(LJVerb *verb) {
	LJGetEventsSync *getevents = (LJGetEventsSync*)verb;

	getevents->entry_count = lj_result_get_int(verb->result, "events_count");
	getevents->entries =
		lj_entries_new_from_result(verb->result, getevents->entry_count,
		                           &getevents->warnings);
}

LJGetEventsRecent*
lj_getevents_recent_new(LJUser *user, const char *usejournal, int howmany,
                        const char *beforedate, gboolean summary, int truncate) {
	LJGetEventsRecent *getevents = g_new0(LJGetEventsRecent, 1);
	lj_getevents_init((LJGetEvents*)getevents, user, usejournal,
			recent_parse_result);
	lj_request_add(((LJVerb*)getevents)->request, "selecttype", "lastn");
	lj_request_add_int(((LJVerb*)getevents)->request, "howmany", howmany);
	if (beforedate)
		lj_request_add(((LJVerb*)getevents)->request, "beforedate", beforedate);
	if (summary) {
		if (truncate)
			lj_request_add_int(((LJVerb*)getevents)->request, "truncate", truncate);
		lj_request_add_int(((LJVerb*)getevents)->request, "prefersubject", 1);
		lj_request_add_int(((LJVerb*)getevents)->request, "noprops", 1);
	}
	return getevents;
}

void
lj_getevents_recent_free(LJGetEventsRecent *getevents, gboolean includeentries) {
	int i;
	if (includeentries) {
		for (i = 0; i < getevents->entry_count; i++)
			lj_entry_free(getevents->entries[i]);
	}
	lj_verb_free_contents((LJVerb*)getevents);
	g_free(getevents->entries);
	g_free(getevents);
}

