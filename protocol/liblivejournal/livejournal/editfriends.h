/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_EDITFRIENDS_H__
#define __LIVEJOURNAL_EDITFRIENDS_H__

#include <livejournal/friends.h>
#include <livejournal/verb.h>

typedef struct {
	LJVerb verb;
	int addcount;
	LJFriend *added;
} LJEditFriends;

LJEditFriends* lj_editfriends_new(LJUser *user);
void           lj_editfriends_add_delete(LJEditFriends *ef,
                                         const char *username);
void           lj_editfriends_add_friend(LJEditFriends *ef,
                                         const char *username,
										 const char *fg, const char *bg);
void           lj_editfriends_free(LJEditFriends *editfriends);

#endif /* __LIVEJOURNAL_EDITFRIENDS_H__ */

