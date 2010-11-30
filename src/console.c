/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <livejournal/consolecommand.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "util-gtk.h"
#include "network.h"
#include "console.h"
#include "conf.h"

typedef struct {
	GtkWidget *win;
	GtkWidget *entry, *display;
	GtkTextMark *mark_end;
	JamAccountLJ *account;
} ConsoleUI;

static char *
name_from_type(LJConsoleLineType type) {
	switch (type) {
	case LJ_CONSOLE_LINE_TYPE_INFO:
		return "info";
	case LJ_CONSOLE_LINE_TYPE_ERROR:
		return "error";
	case LJ_CONSOLE_LINE_TYPE_UNKNOWN:
	default:
		return NULL;
	}
}

void
console_print(ConsoleUI *cui, char *type, const char *text, ...) {
	char buf[4096];
	va_list ap;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	va_start(ap, text);
	g_vsnprintf(buf, 4096, text, ap);
	va_end(ap);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(cui->display));
	gtk_text_buffer_get_end_iter(buffer, &iter);
	if (type) {
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
				buf, -1, type, NULL);
	} else {
		gtk_text_buffer_insert(buffer, &iter, buf, -1);
	}
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(cui->display),
			cui->mark_end, 0.0, FALSE, 0, 0);
}

gboolean
run_console_command(ConsoleUI *cui, const char *command) {
	NetContext *ctx;
	LJConsoleCommand *cc;
	int i;

	console_print(cui, NULL, _("Running command: '"));
	console_print(cui, "command", command);
	console_print(cui, NULL, "'.\n");

	cc = lj_consolecommand_new(jam_account_lj_get_user(cui->account), command);

	ctx = net_ctx_gtk_new(GTK_WINDOW(cui->win), _("Running Command"));
	if (!net_run_verb_ctx((LJVerb*)cc, ctx, NULL)) {
		console_print(cui, "error", _("Error running command.\n"));
		lj_consolecommand_free(cc);
		net_ctx_gtk_free(ctx);
		return FALSE;
	}

	for (i = 0; i < cc->linecount; i++) {
		char *typename = name_from_type(cc->lines[i].type);
		console_print(cui, typename, cc->lines[i].text);
		console_print(cui, typename, "\n");
	}

	lj_consolecommand_free(cc);
	net_ctx_gtk_free(ctx);

	return TRUE;
}

static void
submit_cb(GtkWidget *w, ConsoleUI *cui) {
	char *command;

	command = gtk_editable_get_chars(GTK_EDITABLE(cui->entry), 0, -1);
	if (run_console_command(cui, command))
		gtk_editable_delete_text(GTK_EDITABLE(cui->entry), 0, -1);
	g_free(command);
}

static void
console_help(GtkWidget *w, ConsoleUI *cui) {
	run_console_command(cui, "help");
}

void
console_dialog_run(GtkWindow *parent, JamAccountLJ *acc) {
	ConsoleUI *cui;
	GtkWidget *vbox, *hbox, *button;
	GtkTextBuffer *buffer;
	GtkTextIter end;

	cui = g_new0(ConsoleUI, 1);
	cui->account = acc;
	cui->win = gtk_dialog_new_with_buttons(_("LiveJournal Console"),
			parent, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	jam_win_set_size(GTK_WINDOW(cui->win), 500, 400);
	g_signal_connect_swapped(G_OBJECT(cui->win), "response",
			G_CALLBACK(gtk_widget_destroy), cui->win);
	g_signal_connect_swapped(G_OBJECT(cui->win), "destroy",
			G_CALLBACK(g_free), cui);
	geometry_tie(cui->win, GEOM_CONSOLE);

	vbox = gtk_vbox_new(FALSE, 5);

	hbox = gtk_hbox_new(FALSE, 5);
	cui->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), cui->entry, TRUE, TRUE, 0);

	button = gtk_button_new_with_mnemonic(_(" _Submit "));
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(cui->entry), "activate",
			G_CALLBACK(submit_cb), cui);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(submit_cb), cui);

	button = gtk_button_new_from_stock(GTK_STOCK_HELP);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(console_help), cui);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	cui->display = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(cui->display), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), scroll_wrap(cui->display),
			TRUE, TRUE, 0);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(cui->display));
	gtk_text_buffer_get_end_iter(buffer, &end);
	cui->mark_end = gtk_text_buffer_create_mark(buffer, NULL, &end, FALSE);
	gtk_text_buffer_create_tag(buffer, "command",
			"family", "monospace",
			 NULL);
	gtk_text_buffer_create_tag(buffer, "info",
			"family", "monospace",
			"foreground", "darkgreen",
			NULL);
	gtk_text_buffer_create_tag(buffer, "error",
			"family", "monospace",
			"weight", PANGO_WEIGHT_BOLD,
			"foreground", "red",
			NULL);

	jam_dialog_set_contents(GTK_DIALOG(cui->win), vbox);

	console_print(cui, NULL, _("LiveJournal console ready.\n"));

	gtk_widget_show(cui->win);
}
