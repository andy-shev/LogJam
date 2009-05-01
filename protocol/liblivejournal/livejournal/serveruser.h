/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_SERVERUSER_H__
#define __LIVEJOURNAL_SERVERUSER_H__

#include <livejournal/types.h>

typedef struct _LJServer LJServer;
typedef struct _LJUser   LJUser;

/* --- server --- */

typedef enum {
	LJ_AUTH_SCHEME_UNKNOWN,
	LJ_AUTH_SCHEME_NONE, /* we use md5 passwords by default.
	                        they're replayable, but at least you're not sending
	                        in plaintext. */
	LJ_AUTH_SCHEME_C0, /* challenge/response.
	                      http://www.livejournal.com/~lj_dev/599743.html. */
	LJ_AUTH_SCHEME_OTHER /* for authschemes we don't know about yet. */
} LJAuthScheme;

struct _LJServer {
	char *url;

	GSList *moods;

	int protocolversion;
	LJAuthScheme authscheme;
};

LJServer* lj_server_new(const char *url);
int       lj_server_get_last_cached_moodid(LJServer *server);
#define lj_server_unicode(s) ((s)->protocolversion > 0)

/* --- user --- */

struct _LJUser {
	LJServer *server;

	char *username;
	char *password;
	char *fullname;

	gboolean checkfriends;

	GSList *pickws;
	GSList *friendgroups;
	GSList *usejournals;
	GSList *webmenu;
};

LJUser*  lj_user_new(LJServer *server);
gint     lj_user_compare(gconstpointer a, gconstpointer b);


/* --- moods --- */

#define lj_mood_new() g_new0(LJMood, 1)
#define lj_mood_id_from_name(server, name) _lj_nid_by_name(server->moods, name)
#define lj_mood_name_from_id(server, id) _lj_nid_by_id(server->moods, id)
#define lj_mood_compare _lj_nid_compare_alpha
typedef struct _LJMood {
	NID_CONTENT
	int parentid;
} LJMood;

/* --- friend group --- */

#define lj_friendgroup_new() g_new0(LJFriendGroup, 1)
#define lj_friendgroup_from_id(user, id) _lj_nid_by_id(user->friendgroups, id)
#define lj_friend_group_free _lj_nid_free

/* --- web menu --- */

typedef struct {
	char *text;
	char *url;
	GSList *subitems;
} LJWebMenuItem;

#define lj_webmenuitem_new() g_new0(LJWebMenuItem, 1)
void   lj_webmenu_free(GSList *menu);

#endif /* __LIVEJOURNAL_SERVERUSER_H__ */
