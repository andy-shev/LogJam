/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_GETFRIENDS_H__
#define __LIVEJOURNAL_GETFRIENDS_H__

#include <livejournal/verb.h>

typedef struct {
	LJVerb verb;
	GHashTable *friends;
} LJGetFriends;

LJGetFriends* lj_getfriends_new(LJUser *user);
void          lj_getfriends_free(LJGetFriends *getfriends, gboolean includefriends);

#endif /* __LIVEJOURNAL_GETFRIENDS_H__ */

