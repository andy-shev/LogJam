/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>

#include "checkfriends.h"

static void
parse_result(LJVerb *verb) {
	LJCheckFriends *cf = (LJCheckFriends*)verb;

	cf->lastupdate = g_strdup(lj_result_get(verb->result, "lastupdate"));
	cf->interval = lj_result_get_int(verb->result, "interval");
	cf->newposts = lj_result_get_int(verb->result, "new");
}

LJCheckFriends*
lj_checkfriends_new(LJUser *user, const char *lastupdate) {
	LJCheckFriends *checkfriends = g_new0(LJCheckFriends, 1);
	LJVerb *verb = (LJVerb*)checkfriends;

	lj_verb_init(verb, user, "checkfriends", FALSE, parse_result);
	lj_request_add(verb->request, "lastupdate", lastupdate);

	return checkfriends;
}

void
lj_checkfriends_set_mask(LJCheckFriends *cf, guint32 mask) {
	lj_request_add_int(((LJVerb*)cf)->request, "mask", mask);
}

void
lj_checkfriends_free(LJCheckFriends *cf) {
	lj_verb_free_contents((LJVerb*)cf);
	g_free(cf->lastupdate);
	g_free(cf);
}

