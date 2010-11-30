/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#ifdef HAVE_GTK
#include "gtk-all.h"
#else
#include "glib-all.h"
#endif

#ifdef HAVE_REGEX_H
#include <sys/types.h>
#include <regex.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>

#include <livejournal/consolecommand.h>
#include <livejournal/login.h>

#include "getopt.h"

#include "conf_xml.h"
#include "journalstore.h"
#include "network.h"
#include "checkfriends.h"
#include "sync.h"

#include "jamdoc.h"
#include "cmdline.h"
#include "cmdline_data.h"


typedef struct _Command Command;

typedef struct {
	JamDoc *doc;
	char *username, *password, *postas;
	char *filename;

	Command *curcmd;

	gboolean use_editor;
} Cmdline;

struct _Command {
	const char *cmdname; const char *desc;
	gboolean requireuser, existinguser, needpw;
	void (*action)(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]);
};

static void command_dispatch(Cmdline *cmdline, Command *commands, const char *help, int argc, gchar *argv[]);

#ifndef HAVE_UNISTD_H
/* this is a lame function. it isn't available on windows. */
static gchar*
getpass(const gchar* prompt) {
	static gchar buf[64];
	gint i;
	g_print("%s", prompt);
	for (i = 0; i < 64; i++) {
		gint ch = getchar(); /* FIXME: this echoes */
		if (ch == EOF || ch == (gint)'\n' || ch == (gint)'\r' /* win32 */)
			break;
		buf[i] = (gchar)ch;
	}
	buf[63] = '\0';
	return buf;
}
#endif /* HAVE_UNISTD_H */

static void
print_header(void) {
	g_print(_("LogJam %s\nCopyright (C) 2000-2004 Evan Martin\n\n"), PACKAGE_VERSION);
}

static void
list_commands(Command *commands) {
	int maxlen = 0, len;
	Command *c;
	char *fmt;

	for (c = commands; c->cmdname; c++) {
		len = strlen(c->cmdname);
		if (len > maxlen)
			maxlen = len;
	}
	fmt = g_strdup_printf("  %%-%ds  %%s\n", maxlen);
	for (c = commands; c->cmdname; c++)
		g_print(fmt, _(c->cmdname), _(c->desc));
	g_free(fmt);
}

static void
help_commands(Command *commands) {
	g_print(_("Subcommands:\n"));
	list_commands(commands);
}

static void
print_help(char *argv0, Command *commands) {
	print_header();

	g_print(_("Use:  %s [options] [command [commandargs]]\n"), argv0);
	g_print("\n");

#ifdef HAVE_GTK
	g_print(_("If no subcommand is provided, the default behavior is to load the GUI.\n"));
	g_print("\n");
#endif

	g_print(_(LOGJAM_HELP_TEXT));

	help_commands(commands);

#ifdef HAVE_GTK
	g_print("\n");
	g_print(_("Also, GTK+ command line options (such as --display) can be used.\n"));
#endif
}

static void
print_command_help(Command *cmd, const char *helptext, Command *subcmds) {
	g_print("%s -- %s", cmd->cmdname, helptext);
	if (subcmds)
		help_commands(subcmds);
	exit(EXIT_SUCCESS);
}

#define OPT_LIST_SUBCOMMANDS 1

#define _GETOPT_LOOP(name)                                                  \
{                                                                           \
	const char *short_options = name ## _SHORT_OPTIONS;                     \
	struct option long_options[] = name ## _LONG_OPTIONS;                   \
	optind = 0;                                                             \
	for (;;) {                                                              \
		int c = getopt_long(argc, argv, short_options, long_options, NULL); \
		if (c == -1) break;                                                 \
																			\
		switch (c) {                                                        \
		case '?': /* unknown option character. */                           \
		case ':': /* missing option character. */                           \
			g_printerr(_("Unknown or missing option character\n"));         \
			exit(EXIT_FAILURE);

#define GETOPT_LOOP_END \
		} \
	} \
}

#define _GETOPT_LOOP_SUBCOMMANDS(name)                                      \
	Command commands[] = name ## _SUBCOMMANDS;                              \
	_GETOPT_LOOP(name)                                                      \
		case OPT_LIST_SUBCOMMANDS:                                          \
			list_commands(commands);                                        \
			exit(EXIT_SUCCESS);

#define GETOPT_LOOP_SUBCOMMANDS_END(name)                                   \
	GETOPT_LOOP_END                                                         \
	command_dispatch(cmdline, commands, name ## _HELP_TEXT, argc, argv);

#define GETOPT_LOOP(name)                                                   \
	_GETOPT_LOOP(name)                                                      \
		case 'h':                                                           \
			print_command_help(cmdline->curcmd,                             \
					_(name ## _HELP_TEXT), NULL);

#define GETOPT_LOOP_SUBCOMMANDS(name)                                       \
	_GETOPT_LOOP_SUBCOMMANDS(name)                                          \
		case 'h':                                                           \
			print_command_help(cmdline->curcmd,                             \
					_(name ## _HELP_TEXT), commands);


static void
do_console(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	GString *command = g_string_new(NULL);
	LJConsoleCommand *cc;
	GError *err = NULL;
	int i;

	GETOPT_LOOP(CONSOLE)
	GETOPT_LOOP_END

	if (!JAM_ACCOUNT_IS_LJ(acc)) {
		g_printerr(_("Blogger accounts do not have a console interface.\n"));
		exit(EXIT_FAILURE);
	}

	argc--; argv++; /* skip 'console' */
	if (argc <= 0) {
		g_printerr(_("Must specify a console command.  Try \"help\".\n"));
		exit(EXIT_FAILURE);
	}
	while (argc--)
		if (argc)
			g_string_append_printf(command, "%s ", argv++[0]);
		else
			g_string_append(command, argv++[0]);

	cc = lj_consolecommand_new(jam_account_lj_get_user(JAM_ACCOUNT_LJ(acc)),
			command->str);
	g_string_free(command, TRUE);

	if (!net_run_verb_ctx((LJVerb*)cc, network_ctx_cmdline, &err)) {
		g_print("Error: %s\n", err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < cc->linecount; i++) {
		g_print("%s\n", cc->lines[i].text);
	}

	lj_consolecommand_free(cc);

	exit(EXIT_SUCCESS);
}

static void
do_post(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	gboolean use_editor = FALSE;

	GETOPT_LOOP(POST)
		case 'e':
			use_editor = TRUE;
			break;
	GETOPT_LOOP_END

	if (!jam_host_do_post(jam_account_get_host(acc),
			network_ctx_cmdline, cmdline->doc, NULL)) {
		exit(EXIT_FAILURE);
	}
	if (!app.quiet)
		g_print(_("Success.\n"));
	exit(EXIT_SUCCESS);
}

static void
do_checkfriends(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	JamAccountLJ *acclj;
	char *subcommand = argc > 1 ? argv[1] : NULL;

	app.cli = TRUE;

	GETOPT_LOOP(CHECKFRIENDS)
	GETOPT_LOOP_END

	if (!JAM_ACCOUNT_IS_LJ(acc)) {
		g_printerr(_("Checkfriends is only supported by LiveJournal accounts.\n"));
		exit(EXIT_FAILURE);
	}
	acclj = JAM_ACCOUNT_LJ(acc);

	if (!subcommand) {
		if (checkfriends_cli(acclj)) {
			g_print("1\n"); exit(EXIT_SUCCESS); /* NEW */
		} else {
			g_print("0\n"); exit(EXIT_FAILURE); /* no new entries, or some error */
		}
	} else if (!g_ascii_strcasecmp("purge", subcommand)) {
		char *id = jam_account_id_strdup(acc);
		checkfriends_cli_purge(acclj);
		g_print(_("Checkfriends information for %s purged.\n"), id);
		g_free(id);
		exit(EXIT_SUCCESS);
	} else {
		g_printerr(_("Unknown argument %s to --checkfriends\n"), subcommand);
		exit(EXIT_FAILURE);
	}
}

static void
do_offline_sync(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	GETOPT_LOOP(OFFLINE_SYNC)
	GETOPT_LOOP_END

	app.cli = TRUE;
	if (JAM_ACCOUNT_IS_LJ(acc)) {
		if (sync_run(JAM_ACCOUNT_LJ(acc), NULL))
			exit(EXIT_SUCCESS);
	} else {
		g_printerr(_("Sync is only supported by LiveJournal accounts.\n"));
	}
	exit(EXIT_FAILURE);
}

static void
do_offline_cat(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	LJEntryFileType output_type = LJ_ENTRY_FILE_RFC822;
	JournalStore *js = NULL;
	GError *err = NULL;
	GString *text;
	gint counter = 0; /* how many entries written. for rfc822 separator. */
	gboolean mailbox = FALSE;

	GETOPT_LOOP(OFFLINE_CAT)
		case 'm':
			mailbox = TRUE;
			break;
		case 't':
			output_type = LJ_ENTRY_FILE_RFC822;
			break;
		case 'x':
			output_type = LJ_ENTRY_FILE_XML;
			break;
	GETOPT_LOOP_END

	if (optind >= argc) {
		fprintf(stderr,
				_("Specify at least one itemid or 'latest'.\n"));
		exit(EXIT_FAILURE);
	}
	if (!(js = journal_store_open(acc, FALSE, &err))) {
		fprintf(stderr,
				"%s", err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}
	while (optind < argc) {
		gchar snum[16];
		gint  num = atoi(argv[optind]);
		LJEntry *entry;

		g_snprintf(snum, 16, "%d", num);
		if (strncmp(snum, argv[optind], 16)) {
			if (strncmp(argv[optind], "latest", 7)) {
				fprintf(stderr,
						_("expected itemid, got %s\n"), argv[optind]);
				exit(EXIT_FAILURE);
			} else {
				num = journal_store_get_latest_id(js);
			}
		}
		if (!(entry = journal_store_get_entry(js, num))) {
			fprintf(stderr,
					_("entry %d not found\n"), num);
			exit(EXIT_FAILURE);
		}
		switch (output_type) {
			case LJ_ENTRY_FILE_RFC822:
				if (counter++)
					/* entry separator lead-in: *two* newlines unlike RFC822
					 * email, so that later on in reading a entrybox we can
					 * tell an entry doesn't end with a newline. */
					g_print("\n\n");
				text = lj_entry_to_rfc822(entry, FALSE);
				if (mailbox) {
					time_t now = time(NULL);
					g_print("From %s@%s %s"
					        "X-LogJam-Itemid: %d\n"
					        "%s",
					        jam_account_get_username(acc),
					        jam_account_get_host(acc)->name,
							ctime(&now),
					        num, text->str);
				} else {
					g_print("X-LogJam-Itemid: %d\n"
							"X-LogJam-User: %s\n"
							"X-LogJam-Server: %s\n"
							"%s",
							num, jam_account_get_username(acc),
							jam_account_get_host(acc)->name, text->str);
				}
				g_string_free(text, TRUE);
				break;
			case LJ_ENTRY_FILE_XML:
			default:
				fprintf(stderr, "(xml cat not yet implemented.)\n");
				exit(EXIT_FAILURE);
		}
		lj_entry_free(entry);
		optind++;
	}
	exit(EXIT_SUCCESS);
}

#ifdef HAVE_REGEX_H
/* from regexp documentation in glibc */
static char *get_regerror (int errcode, regex_t *compiled)
{
	size_t length = regerror (errcode, compiled, NULL, 0);
	char *buffer = g_new (char, length);
	(void) regerror (errcode, compiled, buffer, length);
	return buffer;
}
#endif /* HAVE_REGEX_H */

static void
do_offline_grep(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
#ifdef HAVE_REGEX_H
	LJEntryFileType output_type = LJ_ENTRY_FILE_RFC822;
	JournalStore *js = NULL;
	GError *err = NULL;
	GString *text;
	gint counter = 0; /* how many entries written. for rfc822 separator. */
	gint cflags = 0;
	gint res;
	gint latest, itemid;
	regex_t regexp;

	GETOPT_LOOP(OFFLINE_GREP)
		case 't':
			output_type = LJ_ENTRY_FILE_RFC822;
			break;
		case 'x':
			output_type = LJ_ENTRY_FILE_XML;
			break;
		case 'e':
			cflags |= REG_EXTENDED;
			break;
		case 'i':
			cflags |= REG_ICASE;
			break;
	GETOPT_LOOP_END

	if (optind >= argc) {
		g_printerr(_("Specify a regexp to match\n"));
		exit(EXIT_FAILURE);
	}
	if (!(js = journal_store_open(acc, FALSE, &err))) {
		g_printerr("%s", err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}
	if ((res = regcomp(&regexp, argv[optind], cflags | REG_NOSUB))) {
		gchar *re_error = get_regerror(res, &regexp);
		g_printerr(_("Regexp error: %s.\n"), re_error);
		g_free(re_error);
		exit(EXIT_FAILURE);
	}

	latest = journal_store_get_latest_id(js);
	for (itemid = 1; itemid <= latest; itemid++) {
		LJEntry *entry;

		if (!(entry = journal_store_get_entry(js, itemid)))
			continue; /* deleted item */

		// XXX body only
		switch (regexec(&regexp, entry->event, 0, NULL, 0)) {
			case REG_NOMATCH:
				continue; /* item does not match */
			case REG_ESPACE:
				g_printerr(_("Regexp error: out of memory.\n"));
				exit(EXIT_FAILURE);
			default:
				; /* proceed */
		}

		switch (output_type) {
			case LJ_ENTRY_FILE_RFC822:
				if (counter++)
					/* entry separator lead-in: *two* newlines unlike RFC822
					 * email, so that later on in reading a entrybox we can
					 * tell an entry doesn't end with a newline. */
					g_print("\n\n");
				text = lj_entry_to_rfc822(entry, FALSE);
				g_print("X-LogJam-Itemid: %d\n"
						"X-LogJam-User: %s\n"
						"X-LogJam-Server: %s\n"
						"%s",
						itemid, jam_account_get_username(acc),
						jam_account_get_host(acc)->name, text->str);
				g_string_free(text, TRUE);
				break;
			case LJ_ENTRY_FILE_XML:
			default:
				g_printerr("Not yet implemented.\n");
				exit(EXIT_FAILURE);
		}
		lj_entry_free(entry);
	}
	exit(EXIT_SUCCESS);
#else /* HAVE_REGEX_H */
	g_printerr(_("LogJam was compiled without regular expression support.\n"));
	exit(EXIT_FAILURE);
#endif /* HAVE_REGEX_H */
}

static void
do_offline_summary(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	GError *err = NULL;
	JournalStore *js = NULL;
	char *id, *lastsync;

	GETOPT_LOOP(OFFLINE_SUMMARY)
	GETOPT_LOOP_END
	command_dispatch(cmdline, NULL, _(OFFLINE_SUMMARY_HELP_TEXT), argc, argv);

	if (!(js = journal_store_open(acc, FALSE, &err))) {
		fprintf(stderr,
				"%s", err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	}

	id = jam_account_id_strdup(acc);
	g_print(_("Offline journal for '%s':\n"), id);
	g_free(id);

	lastsync = journal_store_get_lastsync(js);
	g_print(_("  Last synced: %s.\n"), lastsync);
	g_free(lastsync);

	g_print(_("  Entry count: %d.\n"), journal_store_get_count(js));
	g_print(_("  Latest itemid: %d.\n"), journal_store_get_latest_id(js));
	exit(EXIT_SUCCESS);
}

static void
do_offline_reindex(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	GError *err = NULL;

	GETOPT_LOOP(OFFLINE_REINDEX)
	GETOPT_LOOP_END
	command_dispatch(cmdline, NULL, _(OFFLINE_REINDEX_HELP_TEXT), argc, argv);

	g_print(_("Rebuilding index..."));
	fflush(stdout);
	if (!journal_store_reindex(acc, &err)) {
		g_print(_(" ERROR: %s.\n"), err->message);
		g_error_free(err);
		exit(EXIT_FAILURE);
	} else {
		g_print(_("done.\n"));
		exit(EXIT_SUCCESS);
	}
}

static void
do_offline(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	GETOPT_LOOP_SUBCOMMANDS(OFFLINE)
	GETOPT_LOOP_SUBCOMMANDS_END(OFFLINE)
	print_command_help(cmdline->curcmd, _(OFFLINE_HELP_TEXT), commands);
}


static void
do_user_add(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	const char *username, *password = NULL;
	gboolean remember_password = TRUE;
	JamHost *host;

	GETOPT_LOOP(USER_ADD)
		case 'p':
			remember_password = FALSE;
			break;
	GETOPT_LOOP_END

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		g_print(_("Error: specify a user.\n"));
		g_print(_(USER_ADD_HELP_TEXT));
		exit(EXIT_FAILURE);
	}
	username = argv[0];

	if (remember_password) {
		if (argc < 2) {
			password = getpass("Password: ");
		} else {
			password = argv[1];
		}
	}

	conf_verify_a_host_exists();
	if (conf.lasthost) {
		host = conf.lasthost;
	} else {
		host = conf.hosts->data;
		conf.lasthost = host;
	}

	acc = jam_host_get_account_by_username(host, username, /*create = */TRUE);
	if (password)
		jam_account_set_password(acc, password);
	jam_account_set_remember(acc, TRUE, remember_password);
	conf_write(&conf, app.conf_dir);
	exit(EXIT_SUCCESS);
}

static void
do_user(Cmdline *cmdline, JamAccount *acc, gint argc, gchar *argv[]) {
	GETOPT_LOOP_SUBCOMMANDS(USER)
	GETOPT_LOOP_SUBCOMMANDS_END(USER)
	print_command_help(cmdline->curcmd, _(USER_HELP_TEXT), commands);
}

JamAccount*
cmdline_load_account(Cmdline *cmdline, gboolean existinguser, gboolean needpw) {
	JamHost *host;
	JamAccount *acc;

	if (!cmdline->username &&
			conf.lasthost && conf.lasthost->lastaccount &&
			conf.lasthost->lastaccount->remember_password) {
		acc = conf.lasthost->lastaccount;
	} else {
		if (!cmdline->username)
			return NULL;

		if (conf.lasthost)
			host = conf.lasthost;
		else if (conf.hosts)
			host = conf.hosts->data;
		else
			return NULL;

		acc = jam_host_get_account_by_username(host, cmdline->username,
				!existinguser);
	}

	if (!acc) {
		g_printerr(_("Unknown account '%s'.\n"), cmdline->username);
		return NULL;
	}

	if (cmdline->password)
		jam_account_set_password(acc, cmdline->password);
	if (!jam_account_get_password(acc) && needpw) {
		char *password;
		if ((password = getpass(_("Password: "))) != NULL)
			jam_account_set_password(acc, password);
	}

	// XXX usejournal
	// if (cmdline->postas)
	//	string_replace(&conf.usejournal, g_strdup(cmdline->postas));

	/* if they're running a console command, we exit before we
	 * get a chance to write this new conf out.  so we do it here. */
	conf_write(&conf, app.conf_dir);

	return acc;
}

gboolean
cmdline_load_file(JamDoc *doc, char *filename, GError **err) {
	if (strcmp(filename, "-") == 0) {
		LJEntry *entry;
		entry = lj_entry_new_from_file(stdin,
				LJ_ENTRY_FILE_AUTODETECT, NULL, err);
		if (!entry) return FALSE;
		jam_doc_load_entry(doc, entry);
		lj_entry_free(entry);
		return TRUE;
	} else {
		return jam_doc_load_file(doc, filename, LJ_ENTRY_FILE_AUTODETECT, err);
	}
}

static void
command_dispatch(Cmdline *cmdline, Command *commands, const char *help, int argc, gchar *argv[]) {
	JamAccount *acc = NULL;
	Command *command = NULL;
	char *cmdname;
	int i;

	argc -= optind;
	argv += optind;
	cmdname = argv[0];

	if (argc <= 0)
		return;

	if (g_ascii_strcasecmp(cmdname, "help") == 0) {
		g_print(help);
		exit(EXIT_SUCCESS);
	}
	for (i = 0; commands && commands[i].cmdname; i++) {
		if (g_ascii_strcasecmp(cmdname, commands[i].cmdname) == 0) {
			command = &commands[i];
			break;
		}
	}
	if (!command) {
		g_printerr(_("Error: Unknown action '%s'.\n"), cmdname);
		exit(EXIT_FAILURE);
	}
	cmdline->curcmd = command;
	if (command->requireuser) {
		acc = cmdline_load_account(cmdline,
		                           command->existinguser, command->needpw);
		if (!acc) {
			g_printerr(_("Error:  Must specify account.\n"));
			exit(EXIT_FAILURE);
		}
		jam_doc_set_account(cmdline->doc, acc);
	}
	command->action(cmdline, acc, argc, argv);
	/* should terminate. */
	g_error("not reached");
}

void
cmdline_parse(JamDoc *doc, int argc, char *argv[]) {
	Cmdline cmdline_real = { .doc = doc };
	Cmdline *cmdline = &cmdline_real;

	_GETOPT_LOOP_SUBCOMMANDS(LOGJAM)
		case 'h':
			print_help(argv[0], commands);
			exit(EXIT_SUCCESS);
		case 'v':
			print_header();
			exit(EXIT_SUCCESS);
		case 'u':
			cmdline->username = optarg;
			break;
		case 'a':
			cmdline->postas = optarg;
			break;
		case 'p':
			cmdline->password = optarg;
			break;
		case 'e':
			cmdline->use_editor = TRUE;
			break;
		case 'q':
			app.quiet = TRUE;
			break;
		case 'f':
			cmdline_load_file(doc, optarg, NULL); /* XXX error */
			break;
	GETOPT_LOOP_SUBCOMMANDS_END(LOGJAM)

	/* if we get here, there wasn't a command to run. */
	if (cmdline->username) {
		JamAccount *acc;
		acc = cmdline_load_account(cmdline, FALSE, TRUE);
		jam_doc_set_account(doc, acc);
	}
	if (cmdline->use_editor) {
		GError *err = NULL;
		LJEntry *entry = jam_doc_get_entry(doc);
		if (!lj_entry_edit_with_usereditor(entry, app.conf_dir, &err)) {
			g_printerr("%s", err->message);
			exit(EXIT_FAILURE);
		}
		jam_doc_load_entry(doc, entry);
		lj_entry_free(entry);
	}
}
