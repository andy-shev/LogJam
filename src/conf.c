/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <livejournal/livejournal.h>

#include "conf_xml.h"
#include "conf.h"
#include "account.h"
#include "util.h"

Configuration conf;
Application app;

#define PATH_BUF_SIZE 1024

JamHost*
conf_host_by_name(Configuration *c, const char *hostname) {
	GSList *l;
	for (l = c->hosts; l != NULL; l = l->next) {
		if (strcmp(hostname, ((JamHost*)l->data)->name) == 0) {
			return l->data;
		}
	}
	return NULL;
}

gboolean
conf_verify_dir(void) {
	return verify_dir(app.conf_dir, NULL);
}

void
conf_verify_a_host_exists() {
	if (conf.hosts == NULL) {
		/* make a default host. */
		LJServer *s = lj_server_new("http://www.livejournal.com");
		JamHost *host = (JamHost*)jam_host_lj_new(s);
		host->name = g_strdup("LiveJournal.com");
		conf.hosts = g_slist_append(conf.hosts, host);
	}
}

void
conf_make_path(char *file, char *buf, int len) {
	char *path;
	path = g_build_filename(app.conf_dir, file, NULL);
	strncpy(buf, path, len);
	g_free(path);
}

char*
conf_make_account_path(JamAccount *acc, const char *path) {
	return g_build_filename(app.conf_dir,
			"servers", jam_account_get_host(acc)->name,
			"users",   jam_account_get_username(acc),
			path ? path : NULL,
			NULL);
}

gboolean
conf_rename_host(JamHost *host, const char *newname, GError **err) {
	char *oldpath, *newpath;

	/* disallow:
	 *   [empty string]
	 *   .
	 *   ..
	 *   ./../foo
	 *   /foo
	 * allow:
	 *   .lAmE.sErVeR.
	 */
	if ((newname[0] == 0) ||
			(newname[0] == '.' &&
				(newname[1] == '.' || newname[1] == '/' || newname[1] == 0)) ||
			(newname[0] == '/')) {
		g_set_error(err, 0, 0, _("new host name is invalid"));
		return FALSE;
	}
	oldpath = g_build_filename(app.conf_dir, "servers", host->name, NULL);
	if (!g_file_test(oldpath, G_FILE_TEST_EXISTS)) {
		string_replace(&host->name, g_strdup(newname));
		g_free(oldpath);
		return TRUE;
	}
	newpath = g_build_filename(app.conf_dir, "servers", newname, NULL);
	if (rename(oldpath, newpath) < 0) {
		g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
				_("renaming '%s' to '%s': %s"), oldpath, newpath,
				g_strerror(errno));
		g_free(oldpath);
		g_free(newpath);
		return FALSE;
	}
	string_replace(&host->name, g_strdup(newname));
	g_free(oldpath);
	g_free(newpath);
	return TRUE;
}

