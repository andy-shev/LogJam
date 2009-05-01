/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_VERB_H__
#define __LIVEJOURNAL_VERB_H__

#include <livejournal/protocol.h>

/* an LJVerb is the (abstract) superclass of all of the LJ verbs,
 * such as login or postevent.  each verb provides a structured
 * method for using each protocol mode.
 * this allows the user's network code to operate with verbs,
 * while still using the specific verbs for each mode. */

typedef struct _LJVerb LJVerb;

struct _LJVerb {
	LJRequest *request;
	LJResult  *result;
	gboolean no_auth;
	void (*parse_result)(LJVerb *verb);
};

gboolean lj_verb_handle_response(LJVerb *verb, const char *response, GError **err);
void lj_verb_dump_request(LJVerb *verb);
void lj_verb_parse_result(LJVerb *verb);
void lj_verb_use_challenge(LJVerb *verb, LJChallenge challenge);

/* only used by subclasses. */
void lj_verb_init(LJVerb *verb, LJUser *user, const char *mode, gboolean no_auth,
		void (*parse_result)(LJVerb *verb));
void lj_verb_free_contents(LJVerb *verb);

#endif /* __LIVEJOURNAL_VERB_H__ */
