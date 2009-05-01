/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>
#include <stdlib.h> /* atoi */

#include "getchallenge.h"

static void
parse_result(LJVerb *verb) {
	LJGetChallenge *getchallenge = (LJGetChallenge*)verb;
	char *authscheme;

	authscheme = lj_result_get(verb->result, "auth_scheme");
	if (!authscheme) {
		getchallenge->authscheme = LJ_AUTH_SCHEME_NONE;
		return;
	}

	if (g_ascii_strcasecmp(authscheme, "c0") == 0) {
		getchallenge->authscheme = LJ_AUTH_SCHEME_C0;
		getchallenge->challenge = g_strdup(
				lj_result_get(verb->result, "challenge"));
	} else {
		getchallenge->authscheme = LJ_AUTH_SCHEME_OTHER;
		getchallenge->authschemestr = g_strdup(authscheme);
	}
}

LJGetChallenge*
lj_getchallenge_new(LJUser *user) {
	LJGetChallenge *getchallenge = g_new0(LJGetChallenge, 1);
	lj_verb_init((LJVerb*)getchallenge, user, "getchallenge", TRUE, parse_result);
	return getchallenge;
}

void
lj_getchallenge_free(LJGetChallenge *getchallenge) {
	lj_verb_free_contents((LJVerb*)getchallenge);
	g_free(getchallenge->authschemestr);
	g_free(getchallenge->challenge);
	g_free(getchallenge);
}

