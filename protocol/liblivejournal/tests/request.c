/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <stdio.h>
#include <livejournal.h>

#include "curl.h"

static void
request_dump_string(LJRequest *request) {
	GString *str;
	str = lj_request_to_string(request);
	g_print("Request as string: %s\n", str->str);
	g_string_free(str, TRUE);
}

int main(int argc, char *argv[]) {
	LJServer  *server;
	LJUser    *user;
	LJRequest *request;

	server = lj_server_new("http://localhost");

	user = lj_user_new(server);
	user->username = g_strdup("test");
	user->password = g_strdup("test");

	request = lj_request_new(user, "login");
	lj_request_dump(request);
	request_dump_string(request);

	return 0;
}
