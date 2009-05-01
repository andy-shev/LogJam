/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_GETCHALLENGE_H__
#define __LIVEJOURNAL_GETCHALLENGE_H__

#include <livejournal/verb.h>

typedef enum {
	LJ_GETCHALLENGE_MODE_SINGLE,
	LJ_GETCHALLENGE_MODE_RECENT,
} LJGetChallengeMode;

typedef struct _LJGetChallenge {
	LJVerb verb;
	LJAuthScheme authscheme;
	char *authschemestr; /* only if authscheme == LJ_AUTH_SCHEME_OTHER. */
	LJChallenge challenge;
} LJGetChallenge;

LJGetChallenge* lj_getchallenge_new(LJUser *user);
void lj_getchallenge_free(LJGetChallenge *getchallenge);

#endif /* __LIVEJOURNAL_GETCHALLENGE_H__ */

