/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <stdlib.h>
#include <string.h>

#include <livejournal/editfriends.h>

#include "friends.h"
#include "network.h"
#include "conf.h"
#include "util-gtk.h"
#include "account.h"
#include "util.h"

#include "friendedit.h"

typedef struct {
	GtkWidget *win;
	GtkWidget *eusername, *ebgcolor, *efgcolor;

	LJFriend *editfriend;
	JamAccountLJ *account;
} FriendEditUI;

static void
update_preview(FriendEditUI *feui) {
	GdkColor fg, bg;

	gdk_color_parse(gtk_entry_get_text(GTK_ENTRY(feui->efgcolor)), &fg);
	gdk_color_parse(gtk_entry_get_text(GTK_ENTRY(feui->ebgcolor)), &bg);

	gtk_widget_modify_text(feui->eusername, GTK_STATE_NORMAL, &fg);
	gtk_widget_modify_base(feui->eusername, GTK_STATE_NORMAL, &bg);
}

static void
color_entry_changed(GtkEntry *e, FriendEditUI *feui) {
	if (strlen(gtk_entry_get_text(e)) == 7)
		update_preview(feui);
}

static gint
change_entry_color_dlg(FriendEditUI *feui, GtkWidget *toedit, const char *title) {
	GtkWidget *dlg;
	GtkColorSelection *csel;
	const char *curcolor;
	char new_hex[10];
	GdkColor color;

	dlg = gtk_color_selection_dialog_new(title);
	gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(feui->win));
	gtk_widget_hide(GTK_COLOR_SELECTION_DIALOG(dlg)->help_button);

	csel = GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(dlg)->colorsel);

	curcolor = gtk_entry_get_text(GTK_ENTRY(toedit));

	/* convert existing hex color to the color selection's color */
	if (strlen(curcolor) == 7 && curcolor[0] == '#') {
		gdk_color_parse(curcolor, &color);
		gtk_color_selection_set_current_color(csel, &color);
	}

	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
		gtk_color_selection_get_current_color(csel, &color);

		gdkcolor_to_hex(&color, new_hex);

		gtk_entry_set_text(GTK_ENTRY(toedit), new_hex);
	}
	gtk_widget_destroy(dlg);

	return 0;
}

static void
change_col_bg(GtkWidget *w, FriendEditUI *feui) {
	change_entry_color_dlg(feui, feui->ebgcolor, _("Select Background Color"));
}

static void
change_col_fg(GtkWidget *w, FriendEditUI *feui) {
	change_entry_color_dlg(feui, feui->efgcolor, _("Select Foreground Color"));
}

static gboolean
add_the_friend(FriendEditUI *feui) {
	NetContext *ctx;
	LJEditFriends *ef;
	gchar *username, *name;

	ctx = net_ctx_gtk_new(GTK_WINDOW(feui->win), _("Adding Friend"));

	ef = lj_editfriends_new(jam_account_lj_get_user(feui->account));
	lj_editfriends_add_friend(ef,
			gtk_entry_get_text(GTK_ENTRY(feui->eusername)),
			gtk_entry_get_text(GTK_ENTRY(feui->efgcolor)),
			gtk_entry_get_text(GTK_ENTRY(feui->ebgcolor)));

	if (!net_run_verb_ctx((LJVerb*)ef, ctx, NULL) || ef->addcount != 1) {
		lj_editfriends_free(ef);
		net_ctx_gtk_free(ctx);
		return FALSE;
	}

	name = ef->added[0].fullname;
	username = ef->added[0].username;

	if (feui->editfriend == NULL ||
			strcmp(feui->editfriend->username, username) != 0) {
		/* we must create a new friend */
		feui->editfriend = lj_friend_new();
		feui->editfriend->conn = LJ_FRIEND_CONN_MY;
	}

	string_replace(&feui->editfriend->username, g_strdup(username));
	string_replace(&feui->editfriend->fullname, g_strdup(name));
	feui->editfriend->foreground = lj_color_to_int(
			gtk_entry_get_text(GTK_ENTRY(feui->efgcolor)));
	feui->editfriend->background = lj_color_to_int(
			gtk_entry_get_text(GTK_ENTRY(feui->ebgcolor)));

	lj_editfriends_free(ef);
	net_ctx_gtk_free(ctx);
	return TRUE;
}

static void
entry_changed(GtkEntry *entry, FriendEditUI* feui) {
	gtk_dialog_set_response_sensitive(GTK_DIALOG(feui->win),
			GTK_RESPONSE_OK,
			(strlen(gtk_entry_get_text(entry)) > 0));
}

LJFriend*
friend_edit_dlg_run(GtkWindow *parent, JamAccountLJ *acc, gboolean edit, LJFriend *f) {
	FriendEditUI feui_actual = { 0 };
	FriendEditUI *feui = &feui_actual;
	GtkWidget *button;
	GtkWidget *table;

	feui->account = acc;
	feui->editfriend = f;

	feui->win = gtk_dialog_new_with_buttons(
			edit ?  _("Edit Friend") :
			(f ? _("Add This Friend") : _("Add a Friend")),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			edit ? _("Change") : _("Add"), GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(feui->win), GTK_RESPONSE_OK);

	table = jam_table_new(3, 3);

	/* make the labels/entries */
	feui->eusername = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(feui->eusername), TRUE);
	gtk_entry_set_max_length(GTK_ENTRY(feui->eusername), 18);
	if (edit)
		gtk_editable_set_editable(GTK_EDITABLE(feui->eusername), FALSE);
	else
		/* enable/disable the button based on name text */
		g_signal_connect(G_OBJECT(feui->eusername), "changed",
			G_CALLBACK(entry_changed), feui);

	gtk_widget_set_size_request(feui->eusername, 100, -1);
	jam_table_label_content(GTK_TABLE(table), 0,
			_("Friend's _username:"), feui->eusername);

	feui->efgcolor = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(feui->efgcolor), TRUE);
	gtk_entry_set_max_length(GTK_ENTRY(feui->efgcolor), 7);
	g_signal_connect(G_OBJECT(feui->efgcolor), "changed",
			G_CALLBACK(color_entry_changed), feui);
	gtk_widget_set_size_request(feui->efgcolor, 100, -1);
	jam_table_label_content(GTK_TABLE(table), 1,
			_("_Text color:"), feui->efgcolor);

	feui->ebgcolor = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(feui->ebgcolor), TRUE);
	gtk_entry_set_max_length(GTK_ENTRY(feui->ebgcolor), 7);
	g_signal_connect(G_OBJECT(feui->ebgcolor), "changed",
			G_CALLBACK(color_entry_changed), feui);
	gtk_widget_set_size_request(feui->ebgcolor, 100, -1);
	jam_table_label_content(GTK_TABLE(table), 2,
			_("_Background color:"), feui->ebgcolor);

	/* make the color selector buttons */
	button = gtk_button_new_with_label(" ... ");
	gtk_table_attach(GTK_TABLE(table), button, 2, 3, 1, 2, GTK_FILL, 0, 2, 2);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(change_col_fg), feui);
	button = gtk_button_new_with_label(" ... ");
	gtk_table_attach(GTK_TABLE(table), button, 2, 3, 2, 3, GTK_FILL, 0, 2, 2);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(change_col_bg), feui);

	jam_dialog_set_contents(GTK_DIALOG(feui->win), table);

	/* fill in default values. */
	if (f) {
		gtk_entry_set_text(GTK_ENTRY(feui->eusername), f->username);
	} else {
		/* emit the "changed" signal, in any case. */
		g_signal_emit_by_name(G_OBJECT(feui->eusername), "changed");
	}

	if (!edit) {
		gtk_entry_set_text(GTK_ENTRY(feui->efgcolor), "#000000");
		gtk_entry_set_text(GTK_ENTRY(feui->ebgcolor), "#FFFFFF");
	} else {
		char color[10];
		lj_int_to_color(f->foreground, color);
		gtk_entry_set_text(GTK_ENTRY(feui->efgcolor), color);
		lj_int_to_color(f->background, color);
		gtk_entry_set_text(GTK_ENTRY(feui->ebgcolor), color);
	}

	while (gtk_dialog_run(GTK_DIALOG(feui->win)) == GTK_RESPONSE_OK) {
		if (add_the_friend(feui)) {
			gtk_widget_destroy(feui->win);
			return feui->editfriend;
		}
	}
	gtk_widget_destroy(feui->win);
	return NULL;
}


