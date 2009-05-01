/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <string.h>

#include <glib.h>
#include <livejournal/protocol.h>
#include <livejournal/verb.h>

void
lj_verb_init(LJVerb *verb, LJUser *user, const char *mode, gboolean noauth,
		void (*parse_result)(LJVerb *verb)) {
	if (noauth)
		verb->request = lj_request_new_without_auth(user, mode);
	else
		verb->request = lj_request_new(user, mode);
	verb->parse_result = parse_result;
}

void
lj_verb_dump_request(LJVerb *verb) {
	lj_request_dump(verb->request);
}

void
lj_verb_parse_result(LJVerb *verb) {
	if (verb->parse_result)
		verb->parse_result(verb);
}

#define ERROR_CONTEXT 300
gboolean
lj_verb_handle_response(LJVerb *verb, const char *response, GError **err) {
	verb->result = lj_result_new_from_response(response);
	if (!lj_result_succeeded(verb->result)) {
		const char *msg = NULL;
		if (verb->result)
			msg = lj_result_get(verb->result, "errmsg");
		if (msg) {
			g_set_error(err, 0, 0, "%s", msg);
		} else {
			char buf[ERROR_CONTEXT];
			strncpy(buf, response, ERROR_CONTEXT);
			buf[ERROR_CONTEXT-1] = 0;
			g_set_error(err, 0, 0,
					"Unable to parse server response; it began with:\n%s",
					buf);
		}
		return FALSE;
	}
	lj_verb_parse_result(verb);
	return TRUE;
}

void
lj_verb_use_challenge(LJVerb *verb, LJChallenge challenge) {
	lj_request_use_challenge(verb->request, challenge);
}

void
lj_verb_free_contents(LJVerb *verb) {
	lj_request_free(verb->request);
	lj_result_free(verb->result);
}

