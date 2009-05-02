/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef G_OS_WIN32
#include <unistd.h> /* unlink */
#else
#include <io.h>
#endif /* G_OS_WIN32 */

#ifdef USE_DASHBOARD
#include "dashboard-frontend.h"
#endif

#include "network.h"
#include "conf.h"
#include "util-gtk.h"
#include "login.h"
#include "jamdoc.h"
#include "jamview.h"
#include "jam.h"
#include "menu.h"
#include "security.h"
#include "icons.h"
#include "checkfriends.h"
#include "draftstore.h"
#include "remote.h"
#include "usejournal.h"
#include "userlabel.h"

#undef USE_STRUCTUREDTEXT

#ifdef USE_STRUCTUREDTEXT
#include "structuredtext.h"
#endif

static void jam_update_title(JamWin *jw);
static void jam_autosave_delete();
static void jam_new_doc(JamWin *jw, JamDoc *doc);
static void jam_update_actions(JamWin *jw);

static const int JAM_AUTOSAVE_INTERVAL = 5*1000;

gboolean
jam_confirm_lose_entry(JamWin *jw) {
	GtkWidget *dlg;
	int ret;

	if (!jam_doc_get_dirty(jw->doc))
		return TRUE;

	dlg = gtk_message_dialog_new(GTK_WINDOW(jw),
			GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			_("Your entry has unsaved changes.  Save them now?"));
	gtk_dialog_add_buttons(GTK_DIALOG(dlg),
			_("Don't Save"), GTK_RESPONSE_NO,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_YES,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_YES);
ask:
	ret = gtk_dialog_run(GTK_DIALOG(dlg));

	switch (ret) {
		case GTK_RESPONSE_NO: /* don't save */
			ret = TRUE;
			break;
		case GTK_RESPONSE_YES: /* save */
			if (!jam_save(jw))
				goto ask;
			ret = TRUE;
			break;
		case GTK_RESPONSE_CANCEL: /* cancel */
		default:
			ret = FALSE;
			break;
	}
	gtk_widget_destroy(dlg);
	return ret;
}

gboolean
jam_save_as_file(JamWin *jw) {
	GtkWidget *filesel;
	GError *err = NULL;
	gboolean ret = FALSE;
	
	filesel = gtk_file_chooser_dialog_new(_("Save Entry As"), GTK_WINDOW(jw),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	while (gtk_dialog_run(GTK_DIALOG(filesel)) == GTK_RESPONSE_ACCEPT) {
		const gchar *filename;
		struct stat sbuf;
		filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(filesel));
		if (stat(filename, &sbuf) == 0) {
			if (!jam_confirm(GTK_WINDOW(filesel), _("Save Entry As"),
						_("File already exists!  Overwrite?")))
				continue;
		}
		if (!jam_doc_save_as_file(jw->doc, filename, &err)) {
			jam_warning(GTK_WINDOW(filesel), err->message);
			g_error_free(err);
		} else { 
			/* save succeeded. */
			ret = TRUE;
			break;
		}
	}

	gtk_widget_destroy(filesel);
	return ret;
}

static char*
prompt_draftname(GtkWindow *parent, const char *basename) {
	GtkWidget *dlg, *box, *entry;

	dlg = gtk_dialog_new_with_buttons(_("New Draft Title"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	entry = gtk_entry_new();
	if (basename)
		gtk_entry_set_text(GTK_ENTRY(entry), basename);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	box = labelled_box_new(_("_Draft Title:"), entry);

	jam_dialog_set_contents(GTK_DIALOG(dlg), box);
	gtk_widget_grab_focus(entry);

	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
		char *text;
		text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
		gtk_widget_destroy(dlg);
		return text;
	}
	gtk_widget_destroy(dlg);
	return NULL;
}

gboolean
jam_save_as_draft(JamWin *jw) {
	GError *err = NULL;
	char *sugg, *title;
	gboolean ret;

	sugg = jam_doc_get_draftname(jw->doc);
	title = prompt_draftname(GTK_WINDOW(jw), sugg);
	g_free(sugg);
	if (!title)
		return FALSE;

	if (!jam_doc_save_as_draft(jw->doc, title, jw->account, &err)) {
		jam_warning(GTK_WINDOW(jw), _("Error saving draft: %s."), err->message);
		g_error_free(err);
		ret = FALSE;
	} else {
		ret = TRUE;
	}
	g_free(title);
	jam_update_actions(jw);
	return ret;
}

gboolean
jam_save(JamWin *jw) {
	GError *err = NULL;

	if (jam_doc_has_save_target(jw->doc)) {
		if (jam_doc_would_save_over_nonxml(jw->doc)) {
			if (!jam_confirm(GTK_WINDOW(jw), _("Saving File"),
							_("Current file was imported from an non-XML file.  "
							"Saving to this file will overwrite the file with XML content.  "
							"Overwrite the file?"))) {
				return FALSE;
			}
		}

		if (!jam_doc_save(jw->doc, jw->account, &err)) {
			jam_warning(GTK_WINDOW(jw), _("Error saving: %s."), err->message);
			g_error_free(err);
			return FALSE;
		}
	} else {
		return jam_save_as_file(jw);
	}
	return TRUE;
}


void
jam_clear_entry(JamWin *jw) {
	JamDoc *doc = jw->doc;
	JamDoc *newdoc;
	JamAccount *acc;
	
	acc = jam_doc_get_account(doc);
	g_object_ref(acc); /* keep the JamAccount around... */
	g_object_unref(G_OBJECT(jw->doc));
	newdoc = jam_doc_new();
	jam_doc_set_account(newdoc, acc);
	g_object_unref(acc); /* ...but don't hang on to it */

	jam_new_doc(jw, newdoc);
}

static void
jam_update_actions(JamWin *jw) {
	int flags;

	flags = jam_doc_get_flags(jw->doc);

	if (flags & LOGJAM_DOC_CAN_SAVE) {
		gtk_button_set_label(GTK_BUTTON(jw->baction), GTK_STOCK_SAVE);
	} else if (flags & LOGJAM_DOC_CAN_SUBMIT) {
		gtk_button_set_label(GTK_BUTTON(jw->baction), "logjam-submit");
		gtk_widget_hide(jw->msaveserver);
	}

	jam_widget_set_visible(jw->bdelete, flags & LOGJAM_DOC_CAN_DELETE);

	jam_widget_set_visible(jw->msaveserver, flags & LOGJAM_DOC_CAN_SAVE);
}

void
entry_changed_cb(JamDoc *doc, JamWin *jw) {
	jam_update_title(jw);
	jam_update_actions(jw);
}

static void
jam_update_title(JamWin *jw) {
	char *doctitle, *title;
	doctitle = jam_doc_get_title(jw->doc);
	title = g_strdup_printf(_("LogJam - %s%s"), doctitle,
			jam_doc_get_dirty(jw->doc) ? "*" : "");
	gtk_window_set_title(GTK_WINDOW(jw), title);
	g_free(title);
	g_free(doctitle);
}

static void
dirty_changed_cb(JamDoc *doc, gpointer foo, JamWin *jw) {
	jam_update_title(jw);
}

static void
usejournal_changed_cb(JamWin *jw) {
	jam_user_label_set_journal(JAM_USER_LABEL(jw->userlabel),
			jam_doc_get_usejournal(jw->doc));
}

static void
can_undo_cb(UndoMgr *um, gboolean can_undo, JamWin *jw) {
	gtk_widget_set_sensitive(jw->mundo, can_undo);
}

static void
can_redo_cb(UndoMgr *um, gboolean can_redo, JamWin *jw) {
	gtk_widget_set_sensitive(jw->mredo, can_redo);
}

static void
jam_new_doc(JamWin *jw, JamDoc *doc) {
	jw->doc = doc;
	g_signal_connect(G_OBJECT(jw->doc), "notify::dirty",
			G_CALLBACK(dirty_changed_cb), jw);
	g_signal_connect_swapped(G_OBJECT(jw->doc), "notify::usejournal",
			G_CALLBACK(usejournal_changed_cb), jw);
	g_signal_connect(G_OBJECT(jw->doc), "entry_changed",
			G_CALLBACK(entry_changed_cb), jw);
	jam_update_title(jw);

	/* if the ui is already up, we need to let everything
	 * know the doc has changed. */
	if (jw->view) {
		jam_view_set_doc(JAM_VIEW(jw->view), jw->doc);
		menu_new_doc(jw);
		jam_update_actions(jw);
		usejournal_changed_cb(jw);
	}
}

void
jam_save_entry_server(JamWin *jw) {
	JamAccount *acc = jam_doc_get_account(jw->doc);
	NetContext *ctx;
	ctx = net_ctx_gtk_new(GTK_WINDOW(jw), NULL);
	if (jam_host_do_edit(jam_account_get_host(acc), ctx, jw->doc, NULL)) {
		jam_clear_entry(jw);
		jam_autosave_delete();
	}
	net_ctx_gtk_free(ctx);
}

static void
delete_draft(JamWin *jw) {
	DraftStore *ds;
	GError *err = NULL;

	ds = draft_store_new(jw->account);

	if (!draft_store_remove_entry(ds,
				jam_doc_get_entry_itemid(jw->doc), &err)) {
		jam_warning(GTK_WINDOW(jw), err->message);
		g_error_free(err);
	} else {
		jam_clear_entry(jw);
	}

	draft_store_free(ds);
}

void
jam_submit_entry(JamWin *jw) {
	JamAccount *acc = jam_doc_get_account(jw->doc);
	NetContext *ctx;
	ctx = net_ctx_gtk_new(GTK_WINDOW(jw), NULL);
	if (jam_host_do_post(jam_account_get_host(acc), ctx, jw->doc, NULL)) {
		gint type = jam_doc_get_entry_type(jw->doc);
		if (type == ENTRY_DRAFT && !conf.options.keepsaveddrafts) {
			if (jam_confirm(GTK_WINDOW(jw),
					_("Delete"), _("Delete this draft from disk?")))
				delete_draft(jw);
		}
		jam_clear_entry(jw);
		if (conf.options.revertusejournal)
			jam_doc_set_usejournal(jw->doc, NULL);
		jam_autosave_delete();
	}
	net_ctx_gtk_free(ctx);
}

static void 
action_cb(GtkWidget *w, JamWin *jw) {
	int flags = jam_doc_get_flags(jw->doc);
	if (flags & LOGJAM_DOC_CAN_SAVE) {
		jam_save_entry_server(jw);
	} else if (flags & LOGJAM_DOC_CAN_SUBMIT) {
		jam_submit_entry(jw);
	} else {
		g_warning("action callback, but no default action?\n");
	}
#ifdef USE_DOCK
	if (conf.options.close_when_send && app.docklet)
		gtk_widget_hide(GTK_WIDGET(jw));
#endif
}

static void
delete_server_entry(JamWin *jw) {
	JamAccount *acc = jam_doc_get_account(jw->doc);
	NetContext *ctx;
	ctx = net_ctx_gtk_new(GTK_WINDOW(jw), NULL);
	if (jam_host_do_delete(jam_account_get_host(acc), ctx, jw->doc, NULL)) {
		jam_clear_entry(jw);
		jam_autosave_delete();
	}
	net_ctx_gtk_free(ctx);
}
	
static void 
delete_cb(GtkWidget *w, JamWin *jw) {
	gint type = jam_doc_get_entry_type(jw->doc); /* defer making a copy */
	const gchar *confirm_text = type == ENTRY_DRAFT ?
		_("Delete this draft from disk?") :
		_("Delete this entry from server?");

	g_assert(type != ENTRY_NEW);

	if (!jam_confirm(GTK_WINDOW(jw),
				_("Delete"), confirm_text))
		return;

	if (type == ENTRY_DRAFT) {
		delete_draft(jw);
	} else {
		delete_server_entry(jw);
	}
#ifdef USE_DOCK
	if (conf.options.close_when_send && app.docklet)
		gtk_widget_hide(GTK_WIDGET(jw));
#endif
}

static void
jam_autosave_delete(void) {
	char *path;
	path = g_build_filename(app.conf_dir, "draft", NULL);
	if (unlink(path) < 0) {
		/* FIXME handle error. */
	}
	g_free(path);
}

static void
update_userlabel(JamWin *jw) {
	jam_user_label_set_account(JAM_USER_LABEL(jw->userlabel),
			jw->account);
	jam_user_label_set_journal(JAM_USER_LABEL(jw->userlabel),
			jam_doc_get_usejournal(jw->doc));
}

/* implicitly works on the *current* document! */
static void
changeuser(JamWin *jw, JamAccount *acc) {
	JamDoc *doc = jw->doc;

	jam_doc_set_account(doc, acc);

	jw->account = acc;

	cfmgr_set_account(app.cfmgr, acc);

	update_userlabel(jw);
}

void 
jam_do_changeuser(JamWin *jw) {
	JamAccount *acc = login_dlg_run(GTK_WINDOW(jw), NULL, jw->account);

	if (acc)
		changeuser(jw, acc);
}

static gboolean
jam_remote_change_user_cb(JamWin *jw, char *username, GError **err) {
	if (username) {
		// XXX evan -- we need user@host
		/*JamAccount *acc;
		acc = jam_host_get_account_by_username(
		LJUser *u;
		u = lj_server_get_user_by_username(jam_account_get_server(jw->account), username);
		if (!u)
			return FALSE;
		
		changeuser(jw, jam_account_make(u));*/
	}
	gtk_window_present(GTK_WINDOW(jw));
	return TRUE;
}

void
jam_save_autosave(JamWin *jw) {
	LJEntry *entry;
	char *path;

	path = g_build_filename(app.conf_dir, "draft", NULL);
	entry = jam_doc_get_entry(jw->doc);
	if (!lj_entry_to_xml_file(entry, path, NULL)) {
		/* FIXME handle error. */
	}
	lj_entry_free(entry);
	g_free(path);
}
	
gboolean
jam_save_autosave_cb(JamWin *jw) {
	if (!jam_doc_get_dirty(jw->doc))
		return TRUE;

	jam_save_autosave(jw);

	return TRUE; /* perpetuate timeout */
}

void 
jam_open_entry(JamWin *jw) {
	GtkWidget *filesel;
	GtkWidget *hbox, *filetype;
	GtkWidget *menu, *item;
	static struct {
		char *label;
		LJEntryFileType type;
	} filetypes[] = {
		{ N_("Autodetect"),   LJ_ENTRY_FILE_AUTODETECT },
		{ N_("XML"),          LJ_ENTRY_FILE_XML        },
		{ N_("RFC822-Style"), LJ_ENTRY_FILE_RFC822     },
		{ N_("Plain Text"),   LJ_ENTRY_FILE_PLAIN      },
		{ NULL                                         },
	}, *ft;

	if (!jam_confirm_lose_entry(jw)) return;
	
	filesel = gtk_file_chooser_dialog_new(_("Open Entry"), GTK_WINDOW(jw),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);

	filetype = gtk_option_menu_new();
	menu = gtk_menu_new();
	for (ft = filetypes; ft->label; ft++) {
		item = gtk_menu_item_new_with_label(_(ft->label));
		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(filetype), menu);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("File Type:")),
			FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), filetype, TRUE, TRUE, 0);
	gtk_widget_show_all(hbox);

	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(filesel), hbox);
	
	while (gtk_dialog_run(GTK_DIALOG(filesel)) == GTK_RESPONSE_ACCEPT) {
		const gchar *filename;
		GError *err = NULL;
		int op = gtk_option_menu_get_history(GTK_OPTION_MENU(filetype));
		LJEntryFileType type = filetypes[op].type;

		filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(filesel));

		if (!jam_doc_load_file(jw->doc, filename, type, &err)) {
			jam_warning(GTK_WINDOW(filesel), err->message);
			g_error_free(err);
		} else {
			undomgr_reset(UNDOMGR(jam_view_get_undomgr(JAM_VIEW(jw->view))));
			break;
		}
	}
	gtk_widget_destroy(filesel);
}

void
jam_open_draft(JamWin *jw) {
	DraftStore *ds;
	LJEntry *e;

	if (!jam_confirm_lose_entry(jw)) return;

	ds = draft_store_new(jw->account);

	e = draft_store_ui_select(ds, GTK_WINDOW(jw));
	if (e) {
		jam_doc_load_draft(jw->doc, e);
		lj_entry_free(e);

		undomgr_reset(UNDOMGR(jam_view_get_undomgr(JAM_VIEW(jw->view))));
	}

	draft_store_free(ds);
}

#ifdef USE_STRUCTUREDTEXT

static void
textstyle_changed_cb(GtkOptionMenu *om, JamWin *jw) {
	TextStyle newstyle = gtk_option_menu_get_history(om);
	GError *err = NULL;
	JamDoc *doc = jam_get_doc(jw);

	if (newstyle == doc->textstyle)
		return;

	if (!jam_doc_change_textstyle(doc, newstyle, &err)) {
		jam_message(GTK_WIDGET(jw), JAM_MSG_ERROR, FALSE,
				_("Error Changing Text Format"), "%s", err->message);
		g_error_free(err);
		gtk_option_menu_set_history(om, doc->textstyle);
	}
}

static void
make_textmods(JamWin *jw) {
	GtkWidget *hbox;
	GtkWidget *omenu, *menu, *item;
	GtkRequisition req;

	omenu = gtk_option_menu_new();
	menu = gtk_menu_new();
	item = gtk_menu_item_new_with_label(_("Normal"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("Structured"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	/*item = gtk_menu_item_new_with_label("Formatted");
	gtk_widget_set_sensitive(item, FALSE);
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);*/

	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);

	g_signal_connect(G_OBJECT(omenu), "changed",
			G_CALLBACK(textstyle_changed_cb), jw);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Entry Format:")),
			FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), omenu, FALSE, FALSE, 0);

	gtk_widget_size_request(omenu, &req);

	gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(jw->eentry),
			GTK_TEXT_WINDOW_TOP, req.height+10);
	gtk_text_view_add_child_in_window(GTK_TEXT_VIEW(jw->eentry),
			hbox, GTK_TEXT_WINDOW_TOP, 5, 5);
}
#endif /* USE_STRUCTUREDTEXT */

static GtkWidget*
make_main_view(JamWin *jw, JamView *view) {
	UndoMgr *um = UNDOMGR(jam_view_get_undomgr(view));

	g_signal_connect(G_OBJECT(um), "can-undo",
		G_CALLBACK(can_undo_cb), jw);
	g_signal_connect(G_OBJECT(um), "can-redo",
		G_CALLBACK(can_redo_cb), jw);

	return GTK_WIDGET(view);
}

static void
usejournal_cb(GtkWidget *w, JamWin *jw) {
	GtkWidget *menu;
	JamAccount *acc = jam_doc_get_account(jw->doc);
	LJUser *u = jam_account_lj_get_user(JAM_ACCOUNT_LJ(acc));

	menu = usejournal_build_menu(u->username,
	                             jam_doc_get_usejournal(jw->doc),
	                             u->usejournals,
	                             jam_win_get_cur_doc(jw));
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			1, gtk_get_current_event_time());
}

static GtkWidget*
make_action_bar(JamWin *jw) {
	GtkWidget *actionbox, *buttonbox;

	actionbox = gtk_hbox_new(FALSE, 18); 

	/* XXX blogger jw->cfi_main = cfindicator_new(fetch_cfmgr(jw), CF_MAIN, GTK_WINDOW(jw));
	cfmgr_set_state(fetch_cfmgr(jw), CF_DISABLED);
	gtk_box_pack_start(GTK_BOX(jw->actionbox),
			cfindicator_box_get(jw->cfi_main),
			FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(jw->actionbox),
			gtk_label_new(_("Current User:")),
			FALSE, FALSE, 0);*/

	jw->userlabel = jam_user_label_new();
	g_signal_connect(G_OBJECT(jw->userlabel), "clicked",
			G_CALLBACK(usejournal_cb), jw);
	gtk_box_pack_start(GTK_BOX(actionbox), jw->userlabel, FALSE, FALSE, 0);
	update_userlabel(jw);

	buttonbox = gtk_hbox_new(FALSE, 6); 
	/*buttonbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(buttonbox), 6);*/

	jw->bdelete = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	g_signal_connect(G_OBJECT(jw->bdelete), "clicked",
			G_CALLBACK(delete_cb), jw);
	gtk_box_pack_start(GTK_BOX(buttonbox), jw->bdelete, FALSE, FALSE, 0);

	jw->baction = gtk_button_new_from_stock("logjam-submit");
	g_signal_connect(G_OBJECT(jw->baction), "clicked",
			G_CALLBACK(action_cb), jw);
	gtk_box_pack_start(GTK_BOX(buttonbox), jw->baction, FALSE, FALSE, 0);

	gtk_box_pack_end(GTK_BOX(actionbox), buttonbox, FALSE, FALSE, 0);

	return actionbox;
}

static GtkWidget*
make_app_contents(JamWin *jw, JamDoc *doc) {
	GtkWidget *vbox;

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

	/* TODO: when we have multiple tabs, we'll create the notebook
	 * and pack the first view into it here */

	gtk_box_pack_start(GTK_BOX(vbox), 
			make_main_view(jw, JAM_VIEW(jw->view)), TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), 
			make_action_bar(jw), FALSE, FALSE, 0);
	
	gtk_widget_show_all(vbox);
	return vbox;
}

void
jam_autosave_init(JamWin* jw) {
	if (app.autosave || !conf.options.autosave)
		return;

	app.autosave = g_timeout_add(JAM_AUTOSAVE_INTERVAL,
			(GSourceFunc)jam_save_autosave_cb, jw);
}

void
jam_autosave_stop(JamWin* jw) {
	if (!app.autosave)
		return;
	jam_autosave_delete();
	g_source_remove(app.autosave);
	app.autosave = 0;
}

/* load a saved autosave if there is one and it's appropriate to do so */
static LJEntry*
jam_load_autosave(JamWin* jw) {
	if (conf.options.autosave) {
		GError *err = NULL;
		char *path;
		LJEntry *entry;

		path = g_build_filename(app.conf_dir, "draft", NULL);

		if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
			g_free(path);
			return FALSE;
		}

		entry = lj_entry_new_from_filename(path, LJ_ENTRY_FILE_XML, NULL, &err);
		if (entry == NULL) {
			jam_warning(GTK_WINDOW(jw), _("Error loading draft: %s."),
					err->message);
			g_error_free(err);
		}
		g_free(path);
		return entry;
	}
	return NULL; /* didn't read draft */
}

void
jam_quit(JamWin *jw) {
	if (jam_confirm_lose_entry(jw))
		gtk_main_quit();
}

static gboolean
delete_event_cb(JamWin *jw) {
#ifdef USE_DOCK
	if (app.docklet)
		gtk_widget_hide(GTK_WIDGET(jw));
	else
#endif
	jam_quit(jw);
	return TRUE; /* don't ever let this delete the window; quit will do it. */
}

/* gtk stuff */
static GType
jam_win_get_type(void) {
	static GType jw_type = 0;
	if (!jw_type) {
		const GTypeInfo jw_info = {
			sizeof (GtkWindowClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof (JamWin),
			0,
			//(GInstanceInitFunc) jam_win_init, /* no init func needed since */
			NULL,     /* GTK_WINDOW_TOPLEVEL is the default GtkWindow:type */
		};
		jw_type = g_type_register_static(GTK_TYPE_WINDOW,
				"JamWin", &jw_info, 0);
	}
	return jw_type;
}

#define JAM_WIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), jam_win_get_type(), JamWin))

static GtkWidget*
jam_win_new(void) {
	JamWin *jw = JAM_WIN(g_object_new(jam_win_get_type(), NULL));
	return GTK_WIDGET(jw);
}

void docklet_setup(GtkWindow *win);

void 
jam_run(JamDoc *doc) {
	GtkWidget *vbox;
	LJEntry *draftentry;
	JamWin *jw;

	jw = JAM_WIN(jam_win_new());
	gtk_window_set_default_size(GTK_WINDOW(jw), 400, 300);
	geometry_tie(GTK_WIDGET(jw), GEOM_MAIN);

	jam_new_doc(jw, doc);
	jw->view = jam_view_new(doc);
	jw->account = jam_doc_get_account(doc);

	g_signal_connect(G_OBJECT(jw), "delete-event",
			G_CALLBACK(delete_event_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(app.remote), "present",
			G_CALLBACK(gtk_window_present), jw);
	g_signal_connect_swapped(G_OBJECT(app.remote), "change_user",
			G_CALLBACK(jam_remote_change_user_cb), jw);

	app.cfmgr = cfmgr_new(jw->account);

	vbox = gtk_vbox_new(FALSE, 0); 

	gtk_box_pack_start(GTK_BOX(vbox), 
			menu_make_bar(jw), FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), make_app_contents(jw, doc),
			TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(jw), vbox);

	jam_autosave_init(jw);

	gtk_widget_show(vbox);
	jam_update_actions(jw);

	/* suck a bunch of events in. */
	while (gtk_events_pending())
		gtk_main_iteration();

	if (!conf.options.allowmultipleinstances) {
		GError *err = NULL;
		if (!logjam_remote_listen(app.remote, &err)) {
			if (err) {
				jam_warning(GTK_WINDOW(jw),
						_("Error initializing remote command socket: %s."),
						err->message);
				g_error_free(err);
			}
		}
	}

#ifdef USE_DOCK
	if (conf.options.docklet)
		docklet_setup(GTK_WINDOW(jw));

	if (app.docklet) {
		if (!conf.options.start_in_dock)
			gtk_widget_show(GTK_WIDGET(jw));
	} else
#endif
	gtk_widget_show(GTK_WIDGET(jw));

	draftentry = jam_load_autosave(jw);
	if (draftentry) {
		if (jam_confirm(GTK_WINDOW(jw), _("Autosaved Draft Found"),
					_("An autosaved draft was found, possibly from a previous run of LogJam that crashed.  "
					  "Would you like to recover it?"))) {
			jam_doc_load_entry(jw->doc, draftentry);
		}
		lj_entry_free(draftentry);
	}

	jam_new_doc(jw, doc);

#ifdef USE_DOCK
	if (JAM_ACCOUNT_IS_LJ(jw->account))
		cf_update_dock(app.cfmgr, GTK_WINDOW(jw));
#endif

	/* to prevent flicker, make this GtkWindow immediately after
	 * pending events have been handled, and immediately handle
	 its * own event afterwards.  * * XXX: Should make_cf_float()
	 have its own event-sucking loop? */
	cf_app_update_float();
	while (gtk_events_pending())
		gtk_main_iteration();

	gtk_main();

	jam_autosave_delete();
}

JamDoc*
jam_win_get_cur_doc(JamWin *jw) {
	return jw->doc;
}

JamView*
jam_win_get_cur_view(JamWin *jw) {
	return JAM_VIEW(jw->view);
}
