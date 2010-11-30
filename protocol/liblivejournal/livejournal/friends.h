/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_FRIENDS_H__
#define __LIVEJOURNAL_FRIENDS_H__

#define LJ_FRIEND_GROUP_ALLFRIENDS 1
typedef struct _LJFriendGroup {
	int id; char *name;
	gboolean ispublic;
} LJFriendGroup;

typedef enum {
	LJ_FRIEND_TYPE_USER=1,
	LJ_FRIEND_TYPE_COMMUNITY
} LJFriendType;

#define LJ_FRIEND_CONN_MY   (1 << 0)
#define LJ_FRIEND_CONN_OF   (1 << 1)
#define LJ_FRIEND_CONN_BOTH (LJ_FRIEND_CONN_MY | LJ_FRIEND_CONN_OF)

typedef struct _LJFriend {
	char *username;
	char *fullname;
	guint32 foreground, background; /* colors. */
	int conn; /* LJ_FRIEND_CONN_MY, etc */
	LJFriendType type;
	guint32 groupmask;
} LJFriend;

LJFriend *lj_friend_new(void);
LJFriend *lj_friend_new_with(char *username, char *name, char *fg, char *bg, int conn, guint32 mask, char *type);
void      lj_friend_free(LJFriend *f);
gint      lj_friend_compare_username(gconstpointer a, gconstpointer b);

LJFriendType lj_friend_type_from_str(char *str);

#endif /* __LIVEJOURNAL_FRIENDS_H__ */
