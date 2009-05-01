/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_CHECKFRIENDS_H__
#define __LIVEJOURNAL_CHECKFRIENDS_H__

#include <livejournal/verb.h>

typedef struct {
	LJVerb verb;
	char *lastupdate;
	int interval;
	int newposts;
} LJCheckFriends;

LJCheckFriends* lj_checkfriends_new(LJUser *user, const char *lastupdate);
void            lj_checkfriends_set_mask(LJCheckFriends *cf, guint32 mask);
void            lj_checkfriends_free(LJCheckFriends *checkfriends);

#endif /* __LIVEJOURNAL_CHECKFRIENDS_H__ */

