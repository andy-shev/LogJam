/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <stdio.h>
#include <livejournal/livejournal.h>
#include <livejournal/login.h>

#include "curl.h"

#define DEFAULT_URL "http://www.livejournal.com"

int main(int argc, char *argv[]) {
	LJServer *server;
	LJUser   *user;
	LJLogin  *login;
	GError   *err = NULL;

	server = lj_server_new(DEFAULT_URL);

	user = lj_user_new(server);
	user->username = g_strdup("test");
	user->password = g_strdup("test");

	printf("logging into %s...\n", DEFAULT_URL);
	login = lj_login_new(user, "liblivejournal-test");
	if (run_verb((LJVerb*)login, &err)) {
		printf("login succeeded.  some properties learned from logging in:\n");
		printf("\tmessage: %s\n", login->message);
		printf("\tfull name: %s\n", user->fullname ? user->fullname : "");
	} else {
		printf("login failed: %s.\n", err->message);
		g_error_free(err);
	}
	lj_login_free(login);

	return 0;
}
