/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>
#include <stdlib.h> /* atoi */

#include "friends.h"
#include "getfriends.h"

static void
read_friends(LJResult *result, GHashTable *friends) {
	LJFriend *f;
	int i, count;
	guint32 mask;
	char *value;

	count = lj_result_get_int(result, "friend_count");
	for (i = 1; i <= count; i++) {
		f = lj_friend_new();
		f->username = g_strdup(lj_result_getf(result, "friend_%d_user", i));
		f->fullname = g_strdup(lj_result_getf(result, "friend_%d_name", i));

		value = lj_result_getf(result, "friend_%d_fg", i);
		if (value) f->foreground = lj_color_to_int(value);
		value = lj_result_getf(result, "friend_%d_bg", i);
		if (value) f->background = lj_color_to_int(value);

		value = lj_result_getf(result, "friend_%d_groupmask", i);
		if (value) {
			mask = atoi(value);
		} else {
			mask = LJ_FRIEND_GROUP_ALLFRIENDS;
		}
		f->groupmask = mask;

		f->conn = LJ_FRIEND_CONN_MY;
		f->type = lj_friend_type_from_str(
				lj_result_getf(result, "friend_%d_type", i));

		g_hash_table_insert(friends, f->username, f);
	}
}

static void
read_friendofs(LJResult *result, GHashTable *friends) {
	LJFriend *f;
	int i, count;
	char *username;

	count = lj_result_get_int(result, "friendof_count");
	for (i = 1; i <= count; i++) {
		username = lj_result_getf(result, "friendof_%d_user", i);

		f = g_hash_table_lookup(friends, username);
		if (f) {
			f->conn |= LJ_FRIEND_CONN_OF;
		} else {
			f = lj_friend_new();
			f->username = g_strdup(username);
			f->fullname = g_strdup(lj_result_getf(result, "friendof_%d_name", i));
			f->type = lj_friend_type_from_str(
					lj_result_getf(result, "friendof_%d_type", i));
			f->conn = LJ_FRIEND_CONN_OF;

			g_hash_table_insert(friends, f->username, f);
		}
	}
}

static void
parse_result(LJVerb *verb) {
	GHashTable *friends;
	
	friends = g_hash_table_new(g_str_hash, g_str_equal);

	read_friends(verb->result, friends);
	read_friendofs(verb->result, friends);
	((LJGetFriends*)verb)->friends = friends;
}

LJGetFriends*
lj_getfriends_new(LJUser *user) {
	LJGetFriends *getfriends = g_new0(LJGetFriends, 1);
	LJVerb *verb = (LJVerb*)getfriends;

	lj_verb_init(verb, user, "getfriends", FALSE, parse_result);
	lj_request_add_int(verb->request, "includefriendof", 1);

	return getfriends;
}

static void
hash_friend_free_cb(gpointer key, LJFriend *f, gpointer data) {
	lj_friend_free(f);
}

void
lj_getfriends_free(LJGetFriends *getfriends, gboolean includefriends) {
	if (getfriends->friends) {
		if (includefriends)
			g_hash_table_foreach(getfriends->friends,
					(GHFunc)hash_friend_free_cb, NULL);
		g_hash_table_destroy(getfriends->friends);
	}
	g_free(getfriends);
}

