/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"
#include <stdio.h>

#ifndef G_OS_WIN32
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
#endif
#include <signal.h>

#include "conf.h"
#include "util-gtk.h"
#include "spawn.h"

#ifdef G_OS_WIN32
#include <windows.h>
void
spawn_url(GtkWindow *parent, const char *url) {
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}
#else

/* http://lxr.mozilla.org/seamonkey/source/xpfe/bootstrap/nsAppRunner.cpp#1413
 * when sending remote commands,
 * mozilla returns 0 on success and nonzero on failure. */
const CommandList spawn_commands[] = {
	{ "Freedesktop URL opener",
	  "xdg-open '%s'" },
	{ "GNOME Browser",
	  "gnome-open '%s'" },
	{ "Mozilla Firefox",
	  "firefox '%s'" },
	{ "Google Chrome",
	  "google-chrome '%s'" },
	{ "Chromium",
	  "chromium-browser '%s'" },
	{ "Debian (sensible-browser)",
	  "sensible-browser '%s'" },
	{ "Galeon",
	  "galeon '%s'" },
	{ "Mozilla",
	  "mozilla -remote 'openURL(%s, new-window)' || mozilla '%s'" },
	{ "Opera",
	  "opera -remote 'openURL(%s,new-window)' || " /* note no space */
	      "opera '%s'" },
	{ "Konqueror",
	  "kfmclient exec '%s'" },
	{ "Netscape",
	  "netscape -remote 'openURL(%s, new-window)' || netscape '%s'" },
	{ 0, 0 }
};

void
spawn_url(GtkWindow *parent, const char *url) {
	char *cmd;
	GError *err = NULL;
	char *argv[4] = { "/bin/sh", "-c", NULL, NULL };

	/* and now, a hack because I don't know how many %s's
	 * are in spawn_command. */
#define ARGS_HACK url,url,url,url,url,url
	cmd = g_strdup_printf(conf.spawn_command, ARGS_HACK);

	argv[2] = cmd;
	if (!g_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL, &err)) {
		jam_warning(parent, _("Error spawning URL '%s': %s\n"),
				url, err->message);
		g_error_free(err);
	}
	g_free(cmd);
}
#endif /* G_OS_WIN32 */
