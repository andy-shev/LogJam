/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#ifdef HAVE_GTK
#include "gtk-all.h"
#else
#include "glib-all.h"
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

#include <stdlib.h>  /* for exit (and __argc and __argv on windows) */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>

#include "account.h"
#include "conf.h"
#include "conf_xml.h"
#include "jamdoc.h"
#include "cmdline.h"

#ifdef HAVE_GTK
#include "login.h"
#include "jam.h"
#include "icons.h"
#include "remote.h"
#include "spawn.h"
#endif

void set_defaults() {
#ifdef HAVE_GTK
#ifndef G_OS_WIN32
	if (conf.spawn_command == NULL)
		conf.spawn_command = g_strdup(spawn_commands[0].command);
#endif
#endif /* HAVE_GTK */

	conf_verify_a_host_exists();

	if (conf.cfuserinterval == 0)
		conf.cfuserinterval = 30;
	cf_threshold_normalize(&conf.cfthreshold);
}

#ifdef HAVE_GTK
static void
try_remote_command(JamAccount *acc) {
	const char *username = acc ? jam_account_get_username(acc) : NULL;
	GError *err = NULL;
	/* send the remote command.
	 * you'd think we'd want to conditionally do this on the configuration
	 * option, but you'd be wrong.  :)
	 * if the setting in the conf is "allow multiple instances", but then
	 * the user changes the setting to "only one instance" in the ui and then
	 * runs another copy of logjam, it's counterintuitive for it to ignore that
	 * changed ui setting, even if it hadn't got propogated to the
	 * configuration file yet.
	 * so we always try to use the socket.  the conf setting is only
	 * used when we consider *creating* the socket. */

	/* a null username just brings the window to the foreground. */
	if (remote_send_user(username, &err)) {
		/* we popped up another window, so we're done. */
		g_print(_("Bringing already-running LogJam instance to the "
				  "foreground...\n"));
		exit(EXIT_SUCCESS);
	} else if (err) {
		g_printerr(_("Error sending remote command: %s.\n"), err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}
}
#endif /* HAVE_GTK */

static void
init_app(int *argc, gchar *argv[]) {
	gint i, shift = 0;
	
	memset(&app, 0, sizeof(Application));
	app.programname      = argv[0]; /* not the same as PROGRAMNAME */

	/* treatment of configuration dir; must be done before read_conf */
	for (i = 1; i < *argc; i++) {                    /* look for arg */
		if (!strcmp(argv[i], "--conf-dir")) {
			app.conf_dir = argv[i+1];
			shift = 2;
			break;
		}
		if (g_strrstr(argv[i], "--conf-dir=")) {
			app.conf_dir = argv[i] + strlen("--conf-dir=");
			shift = 1;
			break;
		}
	}
	if (shift) {                           /* shift what comes after */
		*argc -= shift;
		for (; i < *argc; i++) {
			argv[i] = argv[i + shift];
		}
	}

	if (!app.conf_dir) {
		app.conf_dir = g_strdup_printf("%s/.logjam", g_get_home_dir());
	}
#ifdef HAVE_GTK
	app.tooltips = gtk_tooltips_new();
	gtk_tooltips_enable(app.tooltips);
#endif
}

#ifdef HAVE_GTK
static void
run_gtk(JamDoc *doc) {
	gchar *accelpath;

	app.remote = logjam_remote_new();
	
	accelpath = g_build_filename(app.conf_dir, "accels", NULL);
	gtk_accel_map_load(accelpath);

	icons_initialize();

	/* the 99% common case is the user has a username/password/host remembered
	 * and they just want to use those. */
	if (!jam_doc_get_account(doc)) {
		JamAccount *acc = NULL;
		if (conf.lasthost && conf.lasthost->lastaccount &&
				conf.lasthost->lastaccount->remember_password) {
			acc = conf.lasthost->lastaccount;
			if (JAM_ACCOUNT_IS_LJ(acc))
				login_check_lastupdate(NULL, JAM_ACCOUNT_LJ(acc));
		} else {
			acc = login_dlg_run(NULL, conf.lasthost, acc);
			if (!acc) return;
		}

		jam_doc_set_account(doc, acc);
	}

	jam_run(doc);

	g_object_unref(G_OBJECT(app.remote));

	gtk_accel_map_save(accelpath);
	g_free(accelpath);
	return;
}
#endif

static void
setup_locales() {
	/* This call actually initializes the locale settings,
	 * and doesn't just clear LC_ALL (which is sorta what
	 * it looks like it's doing). */
	setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	/* GTK always wants UTF-8 strings.
	 * gettext will try to translate to the current locale's codeset
	 * unless we tell it to leave it in UTF-8. */
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif
}

static void
setup_glibgdk() {
#if defined (HAVE_GTK) && defined (G_OS_WIN32)
	/* calls to these thread inits must happen before any other
	 * g* calls. */
	if (!g_thread_supported()) g_thread_init(NULL);
	gdk_threads_init();
	gdk_threads_enter();
#endif /* HAVE_GTK && G_OS_WIN32 */

	g_type_init();

	g_set_application_name(_("LogJam"));
}

int
main(int argc, char* argv[]) {
#ifdef HAVE_GTK
	gboolean has_gtk;
#endif
	JamDoc *doc;

	setup_locales();

	setup_glibgdk();

#ifdef HAVE_GTK
#ifdef G_OS_WIN32
	gtk_rc_add_default_file("gtkrc");
#endif
	has_gtk = gtk_init_check(&argc, &argv);
#endif

	init_app(&argc, argv); /* should be called before conf_read */
	conf_read(&conf, app.conf_dir);
	set_defaults();
	jam_account_logjam_init();

	doc = jam_doc_new();
	cmdline_parse(doc, argc, argv); /* may terminate. */

#ifdef HAVE_GTK
	if (!has_gtk) {
		g_printerr(_("GTK init failed -- are you in X?\n"));
		g_printerr(_("Run %s --help for command-line options.\n"), app.programname);
		exit(EXIT_FAILURE);
	}
	try_remote_command(jam_doc_get_account(doc));
	run_gtk(doc);
#else
	g_printerr(_("Error: No action given.\n"));
	g_printerr(_("Run %s --help for command-line options.\n"), app.programname);
	exit(EXIT_FAILURE);
#endif

	conf_write(&conf, app.conf_dir);

#ifdef G_OS_WIN32
	gdk_threads_leave();
#endif

	return 0;
}

#ifdef G_OS_WIN32
int _stdcall WinMain(void *hInst, void *hPrev, char* lpCmdLine, int nCmdShow) {
	return main(__argc, __argv);
}
#endif

