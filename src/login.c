/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <gtk/gtklist.h>
#include <stdlib.h> /* atoi */

#include <livejournal/login.h>

#include "conf.h"
#include "util-gtk.h"
#include "network.h"
#include "jam.h"
#include "menu.h"

#include "icons.h"
#include "login.h"

#define RESPONSE_UPDATE 1

typedef struct {
	GtkWidget *win;
	GtkWidget *mhost, *eusername, *epassword;
	GtkWidget *cusername;
	GtkWidget *cruser, *crpassword;
	GtkWidget *bupdate;

	JamHost *curhost;
} login_dlg;

static void populate_host_list(login_dlg *ldlg);
static void populate_user_list(login_dlg *ldlg);

static void
load_account(login_dlg *ldlg, JamAccount *acc) {
	if (acc) {
		const char *password = jam_account_get_password(acc);
		gtk_entry_set_text(GTK_ENTRY(ldlg->epassword), password ? password : "");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ldlg->cruser), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ldlg->crpassword),
				jam_account_get_remember_password(acc));
	} else {
		gtk_entry_set_text(GTK_ENTRY(ldlg->epassword), "");
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(ldlg->cruser), FALSE);
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(ldlg->crpassword), FALSE);
	}
}

static void
username_changed(GtkWidget *w, login_dlg *ldlg) {
	JamAccount *acc;
	acc = jam_host_get_account_by_username(ldlg->curhost,
			gtk_entry_get_text(GTK_ENTRY(ldlg->eusername)),
			FALSE);
	load_account(ldlg, acc);
}

static JamAccount*
get_selected_account(login_dlg *ldlg) {
	char *username, *password;
	JamAccount *acc;

	username = gtk_editable_get_chars(GTK_EDITABLE(ldlg->eusername), 0, -1);
	if (!username || !username[0]) {
		g_free(username);
		return NULL;
	}

	acc = jam_host_get_account_by_username(ldlg->curhost, username, TRUE);

	password = gtk_editable_get_chars(GTK_EDITABLE(ldlg->epassword), 0, -1);
	jam_account_set_password(acc, password);

	g_free(username);
	g_free(password);

	jam_account_set_remember(acc,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ldlg->cruser)),
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ldlg->crpassword)));

	return acc;
}

gboolean
login_run(GtkWindow *parent, JamAccountLJ *acc) {
	LJUser *u = jam_account_lj_get_user(acc);
	LJLogin *login;
	NetContext *ctx;

#ifndef G_OS_WIN32
	login = lj_login_new(u, "GTK2-LogJam/" PACKAGE_VERSION);
#else
	login = lj_login_new(u, "WinGTK2-LogJam/" PACKAGE_VERSION);
#endif

	ctx = net_ctx_gtk_new(parent, _("Logging In"));
	if (!net_run_verb_ctx((LJVerb*)login, ctx, NULL)) {
		lj_login_free(login);
		net_ctx_gtk_free(ctx);
		return FALSE;
	}

	if (login->message)
		jam_messagebox(parent, _("LiveJournal Message"), login->message);

	lj_login_free(login);
	net_ctx_gtk_free(ctx);

	acc->lastupdate = time(NULL);

	return TRUE;
}

static void
host_changed_cb(GtkOptionMenu *menu, login_dlg *ldlg) {
	GSList *l;

	l = g_slist_nth(conf.hosts, gtk_option_menu_get_history(menu));
	if (!l || !l->data) return;
	ldlg->curhost = l->data;
	populate_user_list(ldlg);
}

static void
addedit_host_cb(GtkWidget *w, login_dlg *ldlg) {
	void manager_dialog(GtkWindow *parent);
	manager_dialog(GTK_WINDOW(ldlg->win));
	ldlg->curhost = NULL;
	populate_host_list(ldlg);
}

static void
populate_host_list(login_dlg *ldlg) {
	GtkWidget *menu, *item, *box, *icon, *label;
	GSList *l;
	JamHost *h;
	int i = 0, history = -1;

	menu = gtk_menu_new();
	for (l = conf.hosts; l != NULL; l = l->next) {
		h = l->data;
		item = gtk_menu_item_new();
		box = gtk_hbox_new(FALSE, 3);
		icon = gtk_image_new_from_stock(jam_host_get_stock_icon(h), GTK_ICON_SIZE_MENU);
		label = gtk_label_new(h->name);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
		gtk_container_add(GTK_CONTAINER(item), box);
		gtk_widget_show_all(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		if (ldlg->curhost && (g_ascii_strcasecmp(h->name, ldlg->curhost->name) == 0))
			history = i;
		i++;
	}

	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("Add/Edit Servers..."));
	g_signal_connect(G_OBJECT(item), "activate",
			G_CALLBACK(addedit_host_cb), ldlg);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	gtk_widget_show_all(menu);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(ldlg->mhost), menu);
	if (history >= 0)
		gtk_option_menu_set_history(GTK_OPTION_MENU(ldlg->mhost), history);
}

static void
populate_user_list(login_dlg *ldlg) {
	GSList *l;
	GList *strings = NULL;
	JamHost *host = ldlg->curhost;
	JamAccount *acc;

	gtk_widget_set_sensitive(ldlg->bupdate, JAM_HOST_IS_LJ(ldlg->curhost));

	for (l = host->accounts; l != NULL; l = l->next) {
		strings = g_list_append(strings, (char*)jam_account_get_username(l->data));
	}
	if (strings)
		gtk_combo_set_popdown_strings(GTK_COMBO(ldlg->cusername), strings);
	else
		gtk_list_clear_items(GTK_LIST(GTK_COMBO(ldlg->cusername)->list), 0, -1);

	if (host->lastaccount) {
		acc = host->lastaccount;
	} else if (host->accounts) {
		acc = host->accounts->data;
	} else {
		acc = NULL;
	}

	gtk_entry_set_text(GTK_ENTRY(ldlg->eusername),
			acc ? jam_account_get_username(acc) : "");
	load_account(ldlg, acc);
}

static void
update_cb(GtkWidget *w, login_dlg *ldlg) {
	JamAccount *acc;
	acc = get_selected_account(ldlg);
	/* we know the account is an lj account because the update
	 * button is disabled for non-lj. */
	if (acc)
		login_run(GTK_WINDOW(ldlg->win), JAM_ACCOUNT_LJ(acc));
}

static GtkWidget*
make_login_table(login_dlg *ldlg) {
	GtkWidget *vbox, *hbox, *cvbox, *img;
	GtkSizeGroup *sg;

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = gtk_vbox_new(FALSE, 6);

	ldlg->mhost = gtk_option_menu_new();
	gtk_widget_set_size_request(ldlg->mhost, 50, -1);
	populate_host_list(ldlg);
	g_signal_connect(G_OBJECT(ldlg->mhost), "changed",
			G_CALLBACK(host_changed_cb), ldlg);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_Server:"), ldlg->mhost, sg),
			FALSE, FALSE, 0);

	ldlg->cusername = gtk_combo_new();
	gtk_widget_set_size_request(ldlg->cusername, 50, -1);
	ldlg->eusername = GTK_COMBO(ldlg->cusername)->entry;
	gtk_combo_disable_activate(GTK_COMBO(ldlg->cusername));
	gtk_entry_set_activates_default(GTK_ENTRY(ldlg->eusername), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_all(_("_User Name:"), ldlg->cusername, TRUE,
				sg, ldlg->eusername),
			FALSE, FALSE, 0);

	ldlg->epassword = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(ldlg->epassword), TRUE);
	gtk_widget_set_size_request(ldlg->epassword, 100, -1);
	gtk_entry_set_visibility(GTK_ENTRY(ldlg->epassword), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_Password:"), ldlg->epassword, sg),
			FALSE, FALSE, 0);

	ldlg->cruser = gtk_check_button_new_with_mnemonic(_("R_emember user"));
	ldlg->crpassword = gtk_check_button_new_with_mnemonic(_("Re_member password"));
	ldlg->bupdate = gtk_button_new_with_mnemonic(_("Update _Information"));
	g_signal_connect(G_OBJECT(ldlg->bupdate), "clicked",
			G_CALLBACK(update_cb), ldlg);

	cvbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(cvbox), ldlg->cruser, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(cvbox), ldlg->crpassword, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(cvbox), ldlg->bupdate, FALSE, FALSE, 0);

	img = gtk_image_new_from_stock("logjam-goat", GTK_ICON_SIZE_DIALOG),
	gtk_size_group_add_widget(sg, img);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cvbox, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	populate_user_list(ldlg);

	g_signal_connect(G_OBJECT(ldlg->eusername), "changed",
			G_CALLBACK(username_changed), ldlg);

	return vbox;
}

static void
login_shown(login_dlg *ldlg) {
	gtk_widget_grab_focus(ldlg->eusername);
}

gboolean
login_check_lastupdate(GtkWindow *parent, JamAccountLJ *acclj) {
	time_t deltat;
	GtkWidget *dlg;
	char *msg;
	gboolean ret = TRUE;
	JamAccount *acc = JAM_ACCOUNT(acclj);

	if (!conf.options.showloginhistory)
		return login_run(NULL, acclj);

	deltat = time(NULL) - acclj->lastupdate;
	if (deltat < 2 * 7 * 24 * 60 * 60) /* two weeks. */
		return TRUE;

	if (acclj->lastupdate == 0) {
		msg = g_strdup_printf(_("Account '%s' hasn't ever logged in.  "
								"Log in now?"), jam_account_get_username(acc));
	} else {
		msg = g_strdup_printf(_("Account '%s' hasn't logged in "
								"in at least %lu days.  Log in now?"),
		                        jam_account_get_username(acc),
		                        (unsigned long) deltat/(24*60*60));
	}
	dlg = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
			"%s", msg);
	g_free(msg);
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES)
		ret = login_run(GTK_WINDOW(dlg), acclj);
	gtk_widget_destroy(dlg);

	/* no matter what they answered, we shouldn't bug them again
	 * next time if they said no.  the only case to bug them again
	 * is if login failed. */
	if (ret)
		acclj->lastupdate = time(NULL);

	return ret;
}

JamAccount*
login_dlg_run(GtkWindow *parent, JamHost *host, JamAccount *acc) {
	login_dlg ldlg_actual = {0}, *ldlg = &ldlg_actual;
	GtkWidget *dlg, *align, *vbox;

	ldlg->win = dlg = gtk_dialog_new_with_buttons(_("Select User"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			_("Selec_t"), GTK_RESPONSE_OK,
			NULL);

	if (acc) {
		ldlg->curhost = jam_account_get_host(acc);
	} else if (host) {
		ldlg->curhost = host;
	} else {
		ldlg->curhost = conf.hosts->data;
	}

	gtk_widget_realize(ldlg->win);

	vbox = gtk_vbox_new(FALSE, 5);

	gtk_box_pack_start(GTK_BOX(vbox), make_login_table(ldlg), TRUE, TRUE, 0);
	align = gtk_alignment_new(.5, .5, 1, 0);
	gtk_container_add(GTK_CONTAINER(align), vbox);

	jam_dialog_set_contents(GTK_DIALOG(dlg), align);

	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	g_signal_connect_swapped(G_OBJECT(ldlg->win), "show",
			G_CALLBACK(login_shown), ldlg);

	if (acc) {
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ldlg->cusername)->entry), jam_account_get_username(acc));
	}

rundlg:
	if (gtk_dialog_run(GTK_DIALOG(ldlg->win)) == GTK_RESPONSE_OK)
		acc = get_selected_account(ldlg);
	else
		acc = NULL;

	if (acc) {
		conf.lasthost = jam_account_get_host(acc);
		conf.lasthost->lastaccount = acc;
		if (JAM_ACCOUNT_IS_LJ(acc)) {
			if (!login_check_lastupdate(GTK_WINDOW(ldlg->win), JAM_ACCOUNT_LJ(acc)))
				goto rundlg;
		}
	}

	gtk_widget_destroy(dlg);
	return acc;
}

