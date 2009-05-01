/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_EDITFRIENDGROUPS_H__
#define __LIVEJOURNAL_EDITFRIENDGROUPS_H__

#include <livejournal/friends.h>
#include <livejournal/verb.h>

typedef struct {
	LJVerb verb;
	int addcount;
	LJFriend *added;
} LJEditFriendGroups;

LJEditFriendGroups* lj_editfriendgroups_new(LJUser *user);

void lj_editfriendgroups_add_delete(LJEditFriendGroups *ef, int id);
void lj_editfriendgroups_add_friend(LJEditFriendGroups *ef,
                                    const char *username,
                                    const char *fg, const char *bg);
void lj_editfriendgroups_add_groupmask(LJEditFriendGroups *ef,
                                       const char *username,
                                       guint32 groupmask);
void lj_editfriendgroups_add_edit(LJEditFriendGroups *efg,
                                  int id, const char *name, gboolean ispublic);
void lj_editfriendgroups_free(LJEditFriendGroups *editfriendgroups);

#endif /* __LIVEJOURNAL_EDITFRIENDGROUPS_H__ */

