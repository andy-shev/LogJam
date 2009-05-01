/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_LOGIN_H__
#define __LIVEJOURNAL_LOGIN_H__

#include <livejournal/verb.h>

typedef struct _LJLogin {
	LJVerb verb;
	char *message;
} LJLogin;

LJLogin* lj_login_new(LJUser *user, const char *clientversion);
void     lj_login_free(LJLogin *login);

#endif /* __LIVEJOURNAL_LOGIN_H__ */

