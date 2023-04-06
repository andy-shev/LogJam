/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <stdlib.h>
#include "util-gtk.h"
#include "account.h"

#include "conf.h"
#include "tie.h"
#include "checkfriends.h"
#include "security.h"
#include "groupedbox.h"
#include "util.h"

/* manager:
 *  - host
 *    - account
 *      - (entries, outbox, etc?)
 */

typedef struct _ManagerUI ManagerUI;
typedef struct _AccountUI AccountUI;

struct _ManagerUI {
	GtkWidget *win;

	GtkTreeStore *store;
	GtkWidget *treeview;

	GSList *pixbuf_names;
	GSList *pixbufs;

	GtkWidget *add;
	GtkWidget *edit;
	GtkWidget *del;

	GtkWidget *filterlabel;
	gpointer selection;
};

enum {
	COL_ICON,
	COL_TEXT,
	COL_ISHOST,
	COL_DATA,
	COL_COUNT
};

static GdkPixbuf*
get_pixbuf(ManagerUI *mui, const char *stockid) {
	int i;
	GSList *l;
	GdkPixbuf *pixbuf;

	for (i = 0, l = mui->pixbuf_names; l; i++, l = l->next) {
		if (strcmp(l->data, stockid) == 0)
			return g_slist_nth(mui->pixbufs, i)->data;
	}

	pixbuf = gtk_widget_render_icon(mui->treeview, stockid,
			GTK_ICON_SIZE_MENU, NULL);
	mui->pixbuf_names = g_slist_append(mui->pixbuf_names, g_strdup(stockid));
	mui->pixbufs = g_slist_append(mui->pixbufs, pixbuf);
	return pixbuf;
}

static GtkWidget*
label_list(GSList *l, gboolean friendgroups /* FIXME: hack */) {
	GtkWidget *label;
	label = jam_form_label_new("");
	if (l) {
		GString *str = g_string_sized_new(1024);

		while (l) {
			if (str->len > 0)
				g_string_append(str, ", ");
			if (friendgroups) {
				LJFriendGroup *fg = l->data;
				g_string_append_printf(str, "<b>%s</b>", fg->name);
			} else {
				g_string_append_printf(str, "<b>%s</b>", (char*)l->data);
			}
			l = l->next;
		}
		gtk_label_set_markup(GTK_LABEL(label), str->str);
		g_string_free(str, TRUE);
	} else {
		gtk_label_set_markup(GTK_LABEL(label), _("<i>none</i>"));
	}
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	return label;
}

static void
run_cfmask_manager_dlg(GtkWidget *b, ManagerUI *mui) {
	JamAccountLJ *acc = mui->selection;
	guint newmask, oldmask;

	oldmask = jam_account_lj_get_cfmask(acc);
	newmask = custom_security_dlg_run(GTK_WINDOW(mui->win), oldmask, acc);
	g_assert(mui->filterlabel);
	if (newmask != oldmask) {
		jam_account_lj_set_cfmask(acc, newmask);
		/* XXX how do we notify the cfmgr of this new mask?
		 * cfmgr_set_mask(cfmgr_new(acc), newmask);*/
		gtk_label_set_text(GTK_LABEL(mui->filterlabel),
				newmask ? _("active") : _("inactive"));
	}
}

static GtkWidget*
lj_user_make_ui_identity(ManagerUI *mui, LJUser *user) {
	GtkSizeGroup *sg;
	GtkWidget *vbox, *epassword;

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_User Name:"),
				jam_form_label_new(user->username), sg),
			FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_Full Name:"),
				jam_form_label_new(user->fullname), sg),
			FALSE, FALSE, 0);

	epassword = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(epassword), FALSE);
	gtk_entry_set_text(GTK_ENTRY(epassword), user->password);
	gtk_widget_set_usize(epassword, 100, -1);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_Password:"), epassword, sg),
			FALSE, FALSE, 0);

	return vbox;
}

static void
account_add_ui_loginoptions(ManagerUI *mui, GtkBox *vbox, JamAccount *acc) {
	GtkWidget *cruser, *crpassword;
	gboolean ru, rp;

	jam_account_get_remember(acc, &ru, &rp);

	cruser = gtk_check_button_new_with_label(_("Remember user"));
	tie_toggle(GTK_TOGGLE_BUTTON(cruser), &acc->remember_user);
	gtk_box_pack_start(vbox, cruser, FALSE, FALSE, 0);

	crpassword = gtk_check_button_new_with_label(_("Remember password"));
	tie_toggle(GTK_TOGGLE_BUTTON(crpassword), &acc->remember_password);
	gtk_box_pack_start(vbox, crpassword, FALSE, FALSE, 0);
}

static GtkWidget*
user_make_ui_pictures(ManagerUI *mui, LJUser *user) {
	GtkWidget *box;
	box = groupedbox_new();
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	groupedbox_set_header(GROUPEDBOX(box), _("User picture keywords:"), FALSE);
	groupedbox_pack(GROUPEDBOX(box), label_list(user->pickws, FALSE), FALSE);
	return box;
}

static GtkWidget*
account_lj_make_ui_friends(ManagerUI *mui, JamAccountLJ *acc) {
	GtkWidget *vbox, *box, *hbcff, *licff, *bcff;
	LJUser *user = jam_account_lj_get_user(acc);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

	/* per-user checkfriends on/off */
	gtk_box_pack_start(GTK_BOX(vbox),
			tie_toggle(GTK_TOGGLE_BUTTON(gtk_check_button_new_with_mnemonic(
					_("_Monitor this user's friends list for updates"))),
				&user->checkfriends),
			FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox),
			label_list(user->friendgroups, TRUE),
			FALSE, FALSE, 0);

	box = groupedbox_new();
	groupedbox_set_header(GROUPEDBOX(box),
			_("Filter for friends list monitor:"), FALSE);

	hbcff = gtk_hbox_new(FALSE, 5);

	licff = gtk_label_new(acc->cfmask ? _("active") : _("inactive"));
	gtk_misc_set_alignment(GTK_MISC(licff), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbcff), licff, TRUE, TRUE, 0);
	mui->filterlabel = licff; /* not the prettiest of hacks */

	bcff  = gtk_button_new_with_mnemonic(_("_Edit filter"));
	/* guard against a user with no friendgroup info */
	gtk_widget_set_sensitive(bcff, user->friendgroups != NULL);
	gtk_box_pack_start(GTK_BOX(hbcff), bcff,  FALSE, FALSE, 0);

	groupedbox_pack(GROUPEDBOX(box), hbcff, FALSE);

	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(bcff), "clicked",
			G_CALLBACK(run_cfmask_manager_dlg), mui);

	return vbox;
}

static GtkWidget*
account_lj_make_ui(ManagerUI *mui, JamAccount *acc) {
	LJUser *user;
	GtkWidget *nb, *box;

	nb = gtk_notebook_new();

	user = jam_account_lj_get_user(JAM_ACCOUNT_LJ(acc));

	box = lj_user_make_ui_identity(mui, user),
	account_add_ui_loginoptions(mui, GTK_BOX(box), acc);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb),
			box,
			gtk_label_new(_("Identity")));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb),
			user_make_ui_pictures(mui, user),
			gtk_label_new(_("Pictures")));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb),
			account_lj_make_ui_friends(mui, JAM_ACCOUNT_LJ(acc)),
			gtk_label_new(_("Friends")));
	return nb;
}

#ifdef blogger_punted_for_this_release
static GtkWidget*
account_blogger_make_ui(ManagerUI *mui, JamAccount *acc) {
	GtkSizeGroup *sg;
	GtkWidget *vbox, *epassword;

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = gtk_vbox_new(FALSE, 6);

	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_User Name:"),
				jam_form_label_new(jam_account_get_username(acc)), sg),
			FALSE, FALSE, 0);

	epassword = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(epassword), FALSE);
	gtk_widget_set_usize(epassword, 100, -1);
	gtk_entry_set_text(GTK_ENTRY(epassword), jam_account_get_password(acc));
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_Password:"), epassword, sg),
			FALSE, FALSE, 0);

	account_add_ui_loginoptions(mui, GTK_BOX(vbox), acc);

	return vbox;
}
#endif /* blogger_punted_for_this_release */

static GtkWidget*
account_make_ui(ManagerUI *mui, JamAccount *acc) {
	if (JAM_ACCOUNT_IS_LJ(acc)) {
		return account_lj_make_ui(mui, acc);
#ifdef blogger_punted_for_this_release
	} else if (JAM_ACCOUNT_IS_BLOGGER(acc)) {
		return account_blogger_make_ui(mui, acc);
#endif /* blogger_punted_for_this_release */
	} else {
		g_error("unknown account type!");
	}
	return NULL;
}

typedef struct {
	GtkTreeIter iter;
	gpointer searchdata;
	gboolean found;
} SearchData;

static gboolean
search_model_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
		gpointer data) {
	SearchData *sd = data;
	gpointer datacol;
	gtk_tree_model_get(model, iter, COL_DATA, &datacol, -1);
	if (datacol == sd->searchdata) {
		sd->found = TRUE;
		sd->iter = *iter;
		return TRUE;
	}
	return FALSE;
}

static GtkTreeIter
search_model(ManagerUI *mui, gpointer data) {
	SearchData sd;
	sd.found = FALSE;
	sd.searchdata = data;
	gtk_tree_model_foreach(GTK_TREE_MODEL(mui->store),
			search_model_cb, &sd);
	return sd.iter;
}

static void
account_edit(ManagerUI *mui, GtkTreeIter *iter, JamAccount *acc) {
	GtkWidget *dlg;
	char *oldname;
	const char *newname;

	oldname = g_strdup(jam_account_get_username(acc));

	dlg = gtk_dialog_new_with_buttons(_("User Properties"),
			GTK_WINDOW(mui->win), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	//gtk_window_set_default_size(GTK_WINDOW(dlg), 300, 200);

	jam_dialog_set_contents(GTK_DIALOG(dlg), account_make_ui(mui, acc));

	gtk_dialog_run(GTK_DIALOG(dlg));

	gtk_widget_destroy(dlg);

	newname = jam_account_get_username(acc);
	if (strcmp(oldname, newname) != 0) {
		gtk_tree_store_set(mui->store, iter,
				COL_TEXT, newname,
				-1);
	}
	g_free(oldname);
}

static void
account_del(ManagerUI *mui, GtkTreeIter *iter, JamAccount *acc) {
	/*GtkTreeIter it;
	LJUser *user = jam_account_get_user(acc);
	it = search_model(mui, user);*/
	g_warning("unimplemented\n");
}

/*
static void
add_entry_cache(ManagerUI *mui, GtkTreeStore *ts, GtkTreeIter *parent, JamAccount *user) {
	GtkTreeIter ientries;

	gtk_tree_store_append(ts, &ientries, parent);
	gtk_tree_store_set(ts, &ientries,
			COL_TEXT, _("Entries"),
			-1);
}
*/

static void
add_account(ManagerUI *mui, GtkTreeIter *parent, JamAccount *account) {
	GtkTreeIter iaccount;

	gtk_tree_store_append(mui->store, &iaccount, parent);
	gtk_tree_store_set(mui->store, &iaccount,
			COL_ICON, NULL,
			COL_TEXT, jam_account_get_username(account),
			COL_ISHOST, FALSE,
			COL_DATA, account,
			-1);

	/*
	add_entry_cache(mui, ts, &iuser, user);

	gtk_tree_store_append(ts, &idrafts, &iuser);
	gtk_tree_store_set(ts, &idrafts,
			COL_TEXT, _("Drafts"),
			-1);

	gtk_tree_store_append(ts, &ioutbox, &iuser);
	gtk_tree_store_set(ts, &ioutbox,
			COL_TEXT, _("Outbox"),
			-1);
	*/
}

static gboolean
entry_tie_cb(GtkEntry *entry, GdkEventFocus *e, char **data) {
	string_replace(data, gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1));
	return FALSE;
}
static void
entry_tie(GtkEntry *entry, char **data) {
	if (*data)
		gtk_entry_set_text(GTK_ENTRY(entry), *data);
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
			G_CALLBACK(entry_tie_cb), data);
}

#ifdef blogger_punted_for_this_release
static gboolean
blogger_rpc_edit_cb(GtkEntry *entry, GdkEventFocus *e, JamHostBlogger *hostb) {
	const char *url = gtk_entry_get_text(entry);
	jam_host_blogger_set_rpcurl(hostb, url);
	return FALSE;
}
#endif /* blogger_punted_for_this_release */

static gboolean
host_rename_cb(GtkEntry *entry, GdkEventFocus *e, JamHost *host) {
	const char *newname;
	GError *err = NULL;

	newname = gtk_entry_get_text(entry);
	if (strcmp(host->name, newname) == 0)
		return FALSE;
	if (!conf_rename_host(host, newname, &err)) {
		jam_warning(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(entry))),
				_("Unable to rename server: %s."), err->message);
		g_error_free(err);
		gtk_entry_set_text(entry, host->name);
		return FALSE;
	}
	return FALSE;
}

static GtkWidget*
host_make_ui(ManagerUI *mui, JamHost *host) {
	GtkWidget *vbox, *name;
	GtkSizeGroup *sg;

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = gtk_vbox_new(FALSE, 6);

	name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(name), host->name);
	g_signal_connect_after(G_OBJECT(name), "focus-out-event",
			G_CALLBACK(host_rename_cb), host);
	gtk_widget_set_usize(name, 200, -1);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_Name:"), name, sg),
			FALSE, FALSE, 0);

	if (JAM_HOST_IS_LJ(host)) {
		JamHostLJ *hostlj = JAM_HOST_LJ(host);
		LJServer *server = jam_host_lj_get_server(hostlj);
		GtkWidget *eurl, *cprotocol;

		eurl = gtk_entry_new();
		entry_tie(GTK_ENTRY(eurl), &server->url);
		gtk_widget_set_usize(eurl, 200, -1);
		gtk_box_pack_start(GTK_BOX(vbox),
				labelled_box_new_sg(_("_URL:"), eurl, sg),
				FALSE, FALSE, 0);

		cprotocol = gtk_check_button_new_with_mnemonic(
				_("_Server supports protocol version 1"));
		tie_toggle(GTK_TOGGLE_BUTTON(cprotocol), &server->protocolversion);
		gtk_box_pack_start(GTK_BOX(vbox), cprotocol, FALSE, FALSE, 0);
#ifdef blogger_punted_for_this_release
	} else if (JAM_HOST_IS_BLOGGER(host)) {
		JamHostBlogger *hostb = JAM_HOST_BLOGGER(host);
		GtkWidget *eurl;
		char *url;

		eurl = gtk_entry_new();
		url = jam_host_blogger_get_rpcurl(hostb);
		if (url)
			gtk_entry_set_text(GTK_ENTRY(eurl), url);
		g_signal_connect(G_OBJECT(eurl), "focus-out-event",
				G_CALLBACK(blogger_rpc_edit_cb), hostb);

		gtk_widget_set_usize(eurl, 200, -1);
		gtk_box_pack_start(GTK_BOX(vbox),
				labelled_box_new_sg(_("_RPC URL:"), eurl, sg),
				FALSE, FALSE, 0);
#endif /* blogger_punted_for_this_release */
	} else {
		g_error("unknown host type!");
	}

	return vbox;
}

static void add_host(ManagerUI *mui, JamHost *host);

static void
host_edit(ManagerUI *mui, GtkTreeIter *iter, JamHost *host) {
	GtkWidget *dlg;
	char *oldname;

	oldname = g_strdup(host->name);

	dlg = gtk_dialog_new_with_buttons(_("Server Properties"),
			GTK_WINDOW(mui->win), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	//gtk_window_set_default_size(GTK_WINDOW(dlg), 300, 200);

	jam_dialog_set_contents(GTK_DIALOG(dlg), host_make_ui(mui, host));

	gtk_dialog_run(GTK_DIALOG(dlg));

	gtk_widget_destroy(dlg);

	if (strcmp(host->name, oldname) != 0) {
		gtk_tree_store_set(mui->store, iter,
				COL_TEXT, host->name,
				-1);
	}
	g_free(oldname);
}

static void
host_del(ManagerUI *mui, GtkTreeIter *iter, JamHost *host) {
	GtkTreeIter it;

	it = search_model(mui, host);
	gtk_tree_store_remove(mui->store, &it);

	conf.hosts = g_slist_remove(conf.hosts, host);
	// XXX evan g_free(server);
}

static void
add_host(ManagerUI *mui, JamHost *host) {
	GtkTreeIter ihost;
	GSList *l;

	gtk_tree_store_append(mui->store, &ihost, NULL);
	gtk_tree_store_set(mui->store, &ihost,
			COL_ICON, get_pixbuf(mui, jam_host_get_stock_icon(host)),
			COL_TEXT, host->name,
			COL_ISHOST, TRUE,
			COL_DATA, host,
			-1);

	for (l = host->accounts; l; l = l->next) {
		add_account(mui, &ihost, l->data);
	}
}

static void
populate_store(ManagerUI *mui) {
	GSList *l;

	gtk_tree_store_clear(mui->store);
	for (l = conf.hosts; l; l = l->next) {
		if (l->data)
			add_host(mui, l->data);
	}
}

static GtkTreeStore*
make_model(ManagerUI *mui) {
	mui->store = gtk_tree_store_new(COL_COUNT,
			/* icon */ GDK_TYPE_PIXBUF,
			/* name */ G_TYPE_STRING,
			/* ishost */ G_TYPE_BOOLEAN,
			/* data */ G_TYPE_POINTER);

	populate_store(mui);

	return mui->store;
}

static void
selection_changed_cb(GtkTreeSelection *ts, ManagerUI *mui) {
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (!gtk_tree_selection_get_selected(ts, &model, &iter)) {
		gtk_widget_set_sensitive(mui->edit, FALSE);
		gtk_widget_set_sensitive(mui->del, FALSE);
	} else {
		/* XXX gboolean ishost;
		gtk_tree_model_get(model, &iter, COL_ISHOST, &ishost, -1);*/
		gtk_widget_set_sensitive(mui->edit, TRUE);
		gtk_widget_set_sensitive(mui->del, TRUE);
	}
}

static void
add_lj_cb(GtkWidget *i, ManagerUI *mui) {
	JamHost *h;
	LJServer *s;
	s = lj_server_new("http://hostname:port");
	h = JAM_HOST(jam_host_lj_new(s));
	h->name = g_strdup("New LiveJournal Server");
	conf.hosts = g_slist_append(conf.hosts, h);
	gtk_tree_selection_unselect_all(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(mui->treeview)));
	add_host(mui, h);
}
#ifdef blogger_punted_for_this_release
static void
add_blogger_cb(GtkWidget *i, ManagerUI *mui) {
	JamHost *h;
	h = JAM_HOST(jam_host_blogger_new());
	h->name = g_strdup("New Blogger Server");
	conf.hosts = g_slist_append(conf.hosts, h);
	gtk_tree_selection_unselect_all(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(mui->treeview)));
	add_host(mui, h);
}

static gboolean
add_cb(GtkWidget *b, GdkEventButton *eb, ManagerUI *mui) {
	GtkWidget *menu, *item;

	menu = gtk_menu_new();
	item = gtk_menu_item_new_with_mnemonic("_LiveJournal Server...");
	g_signal_connect(G_OBJECT(item), "activate",
			G_CALLBACK(add_lj_cb), mui);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_mnemonic("_Blogger Server...");
	g_signal_connect(G_OBJECT(item), "activate",
			G_CALLBACK(add_blogger_cb), mui);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show_all(menu);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, eb->button, eb->time);

	return TRUE;
}
#endif /* blogger_punted_for_this_release */

static void
edit_cb(GtkWidget *b, ManagerUI *mui) {
	GtkTreeSelection *ts;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean ishost;
	gpointer data;

	ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(mui->treeview));
	if (!gtk_tree_selection_get_selected(ts, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, COL_ISHOST, &ishost, COL_DATA, &data, -1);
	mui->selection = data;
	if (ishost) {
		host_edit(mui, &iter, JAM_HOST(data));
	} else {
		account_edit(mui, &iter, JAM_ACCOUNT(data));
	}
}

static void
del_cb(GtkWidget *b, ManagerUI *mui) {
	GtkTreeSelection *ts;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean ishost;
	gpointer data;

	ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(mui->treeview));
	if (!gtk_tree_selection_get_selected(ts, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, COL_ISHOST, &ishost, COL_DATA, &data, -1);

	/* XXX evan this logic is weird; we don't want to delete the "current"
	 * stuff, but there is no real "current" anymore.  maybe we should
	 * just unref it?
	if (mui->selection == c_cur_server()
			|| mui->selection == c_cur_user()) {
		return;
	}*/

	if (ishost) {
		host_del(mui, &iter, JAM_HOST(data));
	} else {
		account_del(mui, &iter, JAM_ACCOUNT(data));
	}
}

static GtkWidget*
make_tree(ManagerUI *mui) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;
	GtkWidget *sw;

	mui->treeview = gtk_tree_view_new();
	gtk_tree_view_set_model(GTK_TREE_VIEW(mui->treeview),
			GTK_TREE_MODEL(make_model(mui)));
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(mui->treeview));
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(selection_changed_cb), mui);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(mui->treeview), FALSE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "Node");

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
			"pixbuf", COL_ICON);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
			"text", COL_TEXT);

	gtk_tree_view_append_column(GTK_TREE_VIEW(mui->treeview), column);

	sw = scroll_wrap(mui->treeview);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	return sw;
}

void
manager_dialog(GtkWindow *parent) {
	STACK(ManagerUI, mui);
	GtkWidget *mainbox, *bbox;

	mui->win = gtk_dialog_new_with_buttons(_("Servers"),
			parent, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	gtk_window_set_default_size(GTK_WINDOW(mui->win), 300, 200);
	geometry_tie(mui->win, GEOM_MANAGER);

	mainbox = gtk_hbox_new(FALSE, 6);

	gtk_box_pack_start(GTK_BOX(mainbox), make_tree(mui), TRUE, TRUE, 0);

	bbox = gtk_vbox_new(FALSE, 6);
	mui->add = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(mui->add), "clicked",
			G_CALLBACK(add_lj_cb), mui);
#ifdef blogger_punted_for_this_release
	g_signal_connect(G_OBJECT(mui->add), "button-press-event",
			G_CALLBACK(add_cb), mui);
#endif /* blogger_punted_for_this_release */
	gtk_box_pack_start(GTK_BOX(bbox), mui->add, FALSE, FALSE, 0);
	mui->edit = gtk_button_new_from_stock(GTK_STOCK_PROPERTIES);
	g_signal_connect(G_OBJECT(mui->edit), "clicked",
			G_CALLBACK(edit_cb), mui);
	gtk_box_pack_start(GTK_BOX(bbox), mui->edit, FALSE, FALSE, 0);
	mui->del = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_signal_connect(G_OBJECT(mui->del), "clicked",
			G_CALLBACK(del_cb), mui);
	gtk_box_pack_start(GTK_BOX(bbox), mui->del, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 0);

	jam_dialog_set_contents(GTK_DIALOG(mui->win), mainbox);

	gtk_dialog_run(GTK_DIALOG(mui->win));

	conf_verify_a_host_exists();

	gtk_widget_destroy(mui->win);
	g_slist_foreach(mui->pixbuf_names, (GFunc)g_free, NULL);
	g_slist_free(mui->pixbuf_names);
	g_slist_foreach(mui->pixbufs, (GFunc)g_object_unref, NULL);
	g_slist_free(mui->pixbufs);
}

