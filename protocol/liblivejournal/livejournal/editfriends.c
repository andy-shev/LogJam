/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>

#include "editfriends.h"

static void
parse_result(LJVerb *verb) {
	LJEditFriends *ef = (LJEditFriends*)verb;
	int i;

	ef->addcount = lj_result_get_int(verb->result, "friends_added");
	if (ef->addcount > 0) {
		ef->added = g_new0(LJFriend, ef->addcount);
		for (i = 0; i < ef->addcount; i++) {
			ef->added[i].username = g_strdup(lj_result_getf(verb->result,
					"friend_%d_user", i+1));
			ef->added[i].fullname = g_strdup(lj_result_getf(verb->result,
					"friend_%d_name", i+1));
		}
	}
}

LJEditFriends*
lj_editfriends_new(LJUser *user) {
	LJEditFriends *editfriends = g_new0(LJEditFriends, 1);
	LJVerb *verb = (LJVerb*)editfriends;

	lj_verb_init(verb, user, "editfriends", FALSE, parse_result);

	return editfriends;
}

void
lj_editfriends_add_delete(LJEditFriends *ef, const char *username) {
	char *key = g_strdup_printf("editfriend_delete_%s", username);
	lj_request_add(((LJVerb*)ef)->request, key, "1");
	g_free(key);
}

void
lj_editfriends_add_friend(LJEditFriends *ef, const char *username,
                          const char *fg, const char *bg) {
	char *key;
	ef->addcount++;
	key = g_strdup_printf("editfriend_add_%d_user", ef->addcount);
	lj_request_add(((LJVerb*)ef)->request, key, username);
	g_free(key);

	if (fg) {
		key = g_strdup_printf("editfriend_add_%d_fg", ef->addcount);
		lj_request_add(((LJVerb*)ef)->request, key, fg);
		g_free(key);
	}
	if (bg) {
		key = g_strdup_printf("editfriend_add_%d_bg", ef->addcount);
		lj_request_add(((LJVerb*)ef)->request, key, bg);
		g_free(key);
	}
}

void
lj_editfriends_free(LJEditFriends *ef) {
	lj_verb_free_contents((LJVerb*)ef);
	if (ef->added) {
		int i;
		for (i = 0; i < ef->addcount; i++) {
			g_free(ef->added[i].username);
			g_free(ef->added[i].fullname);
		}
		g_free(ef->added);
	}
	g_free(ef);
}

