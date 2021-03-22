/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <stdlib.h>
#include <string.h>

#include <livejournal/editfriendgroups.h>

#include "util-gtk.h"

#include "friends.h"
#include "conf.h"
#include "friendgroupedit.h"
#include "network.h"
#include "util.h"

typedef struct {
	GtkWidget *win;
	GtkWidget *egroupname, *cpublic;

	JamAccountLJ *account;
	LJFriendGroup *editgroup;

	int freegroup; /* index of first free group number. */
} friend_group_edit_dlg;

static gboolean
editgroup_run(friend_group_edit_dlg *fged) {
	NetContext *ctx;
	LJEditFriendGroups *efg;
	int groupid;

	if (fged->editgroup) {
		groupid = fged->editgroup->id;
	} else {
		groupid = fged->freegroup;
	}

	efg = lj_editfriendgroups_new(jam_account_lj_get_user(fged->account));
	lj_editfriendgroups_add_edit(efg, groupid,
			gtk_entry_get_text(GTK_ENTRY(fged->egroupname)),
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(fged->cpublic)));

	ctx = net_ctx_gtk_new(GTK_WINDOW(fged->win), _("Modifying Friend Group"));
	if (!net_run_verb_ctx((LJVerb*)efg, ctx, NULL)) {
		lj_editfriendgroups_free(efg);
		net_ctx_gtk_free(ctx);
		return FALSE;
	}

	if (fged->editgroup == NULL) {
		/* we must create a new group */
		fged->editgroup = lj_friendgroup_new();
		fged->editgroup->id = fged->freegroup;
	}
	string_replace(&fged->editgroup->name,
			g_strdup(gtk_entry_get_text(GTK_ENTRY(fged->egroupname))));
	fged->editgroup->ispublic =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fged->cpublic));

	lj_editfriendgroups_free(efg);
	net_ctx_gtk_free(ctx);
	return TRUE;
}

static void
entry_changed(GtkEntry *entry, GtkWidget* button) {
	gtk_widget_set_sensitive(button,
			(strlen(gtk_entry_get_text(entry)) > 0));
}

LJFriendGroup*
friend_group_edit_dlg_run(GtkWindow *parent, JamAccountLJ *acc, LJFriendGroup *fg, int freegroup) {
	friend_group_edit_dlg fged_actual = {0};
	friend_group_edit_dlg *fged = &fged_actual;
	GtkWidget *table, *button, *label;
	char *idstr;
	int row = 0;

	fged->account = acc;
	fged->editgroup = fg;
	fged->freegroup = freegroup;

	fged->win = gtk_dialog_new_with_buttons(
			fg ? _("Edit Friend Group") : _("New Friend Group"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			NULL);
	jam_win_set_size(GTK_WINDOW(fged->win), 200, 1);

	table = jam_table_new(fg ? 3 : 2, 2);

	if (fg) {
		idstr = g_strdup_printf("%d", fg->id);
		label = gtk_label_new(idstr);
		g_free(idstr);
		gtk_label_set_selectable(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		jam_table_label_content(GTK_TABLE(table), row++, _("Group ID:"), label);
	}

	fged->egroupname = gtk_entry_new();
	jam_table_label_content(GTK_TABLE(table), row++, _("_Group Name:"), fged->egroupname);

	fged->cpublic = gtk_check_button_new_with_mnemonic("_Public");
	jam_table_fillrow(GTK_TABLE(table), row++, fged->cpublic);

	jam_dialog_set_contents(GTK_DIALOG(fged->win), table);

	button = gtk_dialog_add_button(GTK_DIALOG(fged->win),
			fg ? "  Change  " : "  Create  ", GTK_RESPONSE_OK);
	/* enable/disable the button based on name text */
	g_signal_connect(G_OBJECT(fged->egroupname), "changed",
		G_CALLBACK(entry_changed), button);

	/* fill in default values. */
	if (fg) {
		gtk_entry_set_text(GTK_ENTRY(fged->egroupname), fg->name);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fged->cpublic), fg->ispublic);
	} else {
		gtk_entry_set_text(GTK_ENTRY(fged->egroupname), "");
		/* emit the "changed" signal, anyway. */
	}

	while (gtk_dialog_run(GTK_DIALOG(fged->win)) == GTK_RESPONSE_OK) {
		if (editgroup_run(fged)) {
			gtk_widget_destroy(fged->win);
			return fged->editgroup;
		}
	}
	gtk_widget_destroy(fged->win);
	return NULL;
}

