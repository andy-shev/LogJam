/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>

#include "consolecommand.h"

static void
parse_result(LJVerb *verb) {
	LJConsoleCommand *cc = (LJConsoleCommand*)verb;
	int i;
	char *linetext, *linetype;
	
	cc->linecount = lj_result_get_int(verb->result, "cmd_line_count");
	cc->lines = g_new0(LJConsoleLine, cc->linecount);
	for (i = 0; i < cc->linecount; i++) {
		linetext = lj_result_getf(verb->result, "cmd_line_%d", i+1);
		linetype = lj_result_getf(verb->result, "cmd_line_%d_type", i+1);
		if (linetype == NULL)
			linetype = "info";

		if (g_ascii_strcasecmp(linetype, "error") == 0)
			cc->lines[i].type = LJ_CONSOLE_LINE_TYPE_ERROR;
		else if (g_ascii_strcasecmp(linetype, "info") == 0)
			cc->lines[i].type = LJ_CONSOLE_LINE_TYPE_INFO;
		else 
			cc->lines[i].type = LJ_CONSOLE_LINE_TYPE_INFO;

		if (linetext)
			cc->lines[i].text = g_strdup(linetext);
	}
}

LJConsoleCommand*
lj_consolecommand_new(LJUser *user, const char *command) {
	LJConsoleCommand *consolecommand = g_new0(LJConsoleCommand, 1);
	LJVerb *verb = (LJVerb*)consolecommand;

	lj_verb_init(verb, user, "consolecommand", FALSE, parse_result);
	lj_request_add(verb->request, "command", command);

	return consolecommand;
}

void
lj_consolecommand_free(LJConsoleCommand *cc) {
	lj_verb_free_contents((LJVerb*)cc);
	if (cc->lines) {
		int i;
		for (i = 0; i < cc->linecount; i++)
			g_free(cc->lines[i].text);
		g_free(cc->lines);
	}
	g_free(cc);
}

