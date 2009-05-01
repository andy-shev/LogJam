/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_CONSOLECOMMAND_H__
#define __LIVEJOURNAL_CONSOLECOMMAND_H__

#include <livejournal/verb.h>

typedef enum {
	LJ_CONSOLE_LINE_TYPE_UNKNOWN,
	LJ_CONSOLE_LINE_TYPE_INFO,
	LJ_CONSOLE_LINE_TYPE_ERROR
} LJConsoleLineType;

typedef struct {
	LJConsoleLineType type;
	char *text;
} LJConsoleLine;

typedef struct {
	LJVerb verb;
	int linecount;
	LJConsoleLine *lines;
} LJConsoleCommand;

LJConsoleCommand* lj_consolecommand_new(LJUser *user, const char *command);
void              lj_consolecommand_free(LJConsoleCommand *consolecommand);

#endif /* __LIVEJOURNAL_CONSOLECOMMAND_H__ */

