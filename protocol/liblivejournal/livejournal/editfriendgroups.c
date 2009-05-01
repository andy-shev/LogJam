/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>

#include "editfriendgroups.h"

static void
parse_result(LJVerb *verb) {
	LJEditFriendGroups *ef = (LJEditFriendGroups*)verb;
	int i;

	ef->addcount = lj_result_get_int(verb->result, "friends_added");
	if (ef->addcount > 0) {
		ef->added = g_new0(LJFriend, ef->addcount);
		for (i = 0; i < ef->addcount; i++) {
			ef->added[i].username = lj_result_getf(verb->result,
					"friend_%d_user", i+1);
			ef->added[i].fullname = lj_result_getf(verb->result,
					"friend_%d_name", i+1);
		}
	}
}

LJEditFriendGroups*
lj_editfriendgroups_new(LJUser *user) {
	LJEditFriendGroups *editfriendgroups = g_new0(LJEditFriendGroups, 1);
	LJVerb *verb = (LJVerb*)editfriendgroups;

	lj_verb_init(verb, user, "editfriendgroups", FALSE, parse_result);

	return editfriendgroups;
}

void
lj_editfriendgroups_add_delete(LJEditFriendGroups *efg, int id) {
	char *key = g_strdup_printf("efg_delete_%d", id);
	lj_request_add(((LJVerb*)efg)->request, key, "1");
	g_free(key);
}

void
lj_editfriendgroups_add_groupmask(LJEditFriendGroups *efg,
                                  const char *username, guint32 groupmask) {
	char *key;
	key = g_strdup_printf("editfriend_groupmask_%s", username);
	lj_request_add_int(((LJVerb*)efg)->request, key, groupmask);
	g_free(key);
}

void
lj_editfriendgroups_add_edit(LJEditFriendGroups *efg,
                             int id, const char *name, gboolean ispublic) {
	char *key;
	key = g_strdup_printf("efg_set_%d_name", id);
	lj_request_add(((LJVerb*)efg)->request, key, name);
	g_free(key);
	key = g_strdup_printf("efg_set_%d_public", id);
	lj_request_add(((LJVerb*)efg)->request, key, ispublic ? "1" : "0");
	g_free(key);
}

void
lj_editfriendgroups_free(LJEditFriendGroups *ef) {
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

