/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include "jamdoc.h"
#include "util-gtk.h"

static GtkWidget*
add_menu_item(GtkMenuShell *ms, const gchar *id, const gchar *text) {
	GtkWidget *hbox;
	GtkWidget *item;

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox), 
			gtk_image_new_from_stock(id, GTK_ICON_SIZE_MENU), 
			FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			gtk_label_new(text), FALSE, FALSE, 0);

	item = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(item), hbox);
	gtk_menu_shell_append(ms, item);
	return item;
}

static GtkWidget*
make_usertype_omenu() {
	GtkWidget *omenu, *menu;

	menu = gtk_menu_new();
	add_menu_item(GTK_MENU_SHELL(menu), "logjam-ljuser", _("User"));
	add_menu_item(GTK_MENU_SHELL(menu), "logjam-ljcomm", _("Community"));

	omenu = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	return omenu;
}

void
link_journal_dialog_run(GtkWindow *win, JamDoc *doc) {
	GtkWidget *dlg;
	GtkWidget *vbox, *hbox, *entry, *omenu;
	GtkWidget *entry_name;
	GtkSizeGroup *sizegroup;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *username = NULL;
	char *usernick = NULL;
	int usertype;
	gboolean selection = FALSE;

	buffer = jam_doc_get_text_buffer(doc);
	if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
		username = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
		selection = TRUE;
	}

	dlg = gtk_dialog_new_with_buttons(_("Insert lj user / lj community Tag"),
			win, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	if (username) {
		gtk_entry_set_text(GTK_ENTRY(entry), username);
		g_free(username);
	}
	hbox = labelled_box_new_sg(_("_Username:"), entry, sizegroup);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	entry_name = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(entry_name), TRUE);
	hbox = labelled_box_new_sg(_("_Nickname:"), entry_name, sizegroup);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	omenu = make_usertype_omenu();
	hbox = labelled_box_new_sg(_("User _Type:"), omenu, sizegroup);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	jam_dialog_set_contents(GTK_DIALOG(dlg), vbox);

	if (gtk_dialog_run(GTK_DIALOG(dlg)) != GTK_RESPONSE_OK) {
		gtk_widget_destroy(dlg);
		return;
	}
	username = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	usernick = gtk_editable_get_chars(GTK_EDITABLE(entry_name), 0, -1);
	usertype = gtk_option_menu_get_history(GTK_OPTION_MENU(omenu));
	gtk_widget_destroy(dlg);
	if (username[0] == 0) {
		g_free(username);
		g_free(usernick);
		return;
	}

	if (selection)
		gtk_text_buffer_delete(buffer, &start, &end);
	else
		gtk_text_buffer_get_iter_at_mark(buffer, &start,
				gtk_text_buffer_get_insert(buffer));

	if (usernick && *usernick) {
		gchar *link;
		JamAccount *acc = jam_doc_get_account(doc);
		gchar *url = jam_account_lj_get_server(JAM_ACCOUNT_LJ(acc))->url;

		xml_escape(&username);
		xml_escape(&usernick);

		link = g_strdup_printf(
			"<a href='%s/userinfo.bml?user=%s'>"
				"<img src='%s/img/%s' alt='[info]' align='absmiddle' width='17' height='17' border='0' />"
			"</a>"
			"<a style='FONT-WEIGHT: 800' href='%s/users/%s'>%s</a>",
			url, username,
				url, usertype == 0 ? "userinfo.gif" : "community.gif",
			url, username, usernick);

		gtk_text_buffer_insert(buffer, &start, link, -1);

		g_free(link);
		g_free(username);
		g_free(usernick);
		return;
	}

	/* In case of empty string the buffer is allocated. We need to free it. */
	g_free(usernick);

	gtk_text_buffer_insert(buffer, &start, "<lj ", -1);
	if (usertype == 0)
		gtk_text_buffer_insert(buffer, &start, "user=\"", -1);
	else
		gtk_text_buffer_insert(buffer, &start, "comm=\"", -1);
	xml_escape(&username);
	gtk_text_buffer_insert(buffer, &start, username, -1);
	g_free(username);
	gtk_text_buffer_insert(buffer, &start, "\" />", -1);
}

