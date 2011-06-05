/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "about.h"
#include "conf.h"
#include "network.h"
#include "menu.h"
#include "jam.h"
#include "friends.h"
#include "history.h"
#include "util-gtk.h"
#include "spawn.h"
#include "security.h"
#include "icons.h"
#include "settings.h"
#include "pollcreator.h"
#include "tools.h"
#include "undo.h"
#include "usejournal.h"
#ifdef HAVE_GTKHTML
#include "preview.h"
#endif
#include "sync.h"
#include "html_markup.h"
#include "console.h"

enum {
	ACTION_NONE=0,
	ACTION_WEB_LINKS,

	ACTION_UNDO,
	ACTION_REDO,

	ACTION_CUT,
	ACTION_COPY,
	ACTION_PASTE,

	ACTION_ENTRY_SUBMIT_SEP,
	ACTION_ENTRY_SUBMIT_NEW,
	ACTION_ENTRY_SAVE_SERVER,

	ACTION_VIEW,
	/* these must match JamViewMeta in meta.h. */
	ACTION_VIEW_FIRST,
	ACTION_VIEW_SECURITY,
	ACTION_VIEW_MOOD,
	ACTION_VIEW_PICTURE,
	ACTION_VIEW_MUSIC,
	ACTION_VIEW_LOCATION,
	ACTION_VIEW_TAGS,
	ACTION_VIEW_PREFORMATTED,
	ACTION_VIEW_DATESEL,
	ACTION_VIEW_COMMENTS,
	ACTION_VIEW_SCREENING,

	ACTION_JOURNAL,
	ACTION_JOURNAL_USE,
	ACTION_JOURNAL_USE_SEP,
	ACTION_CHANGE_USER,
	ACTION_MANAGER,

	ACTION_TOOLS_CONSOLE
};

static void menu_account_changed(JamWin *jw);

static void
menu_make_link(JamWin *jw) {
	void link_dialog_run(GtkWindow *win, JamDoc *doc);
	link_dialog_run(GTK_WINDOW(jw), jw->doc);
}
static void
menu_make_journal_link(JamWin *jw) {
	void link_journal_dialog_run(GtkWindow *win, JamDoc *doc);
	link_journal_dialog_run(GTK_WINDOW(jw), jw->doc);
}
static void
menu_insert_image(JamWin *jw) {
	void image_dialog_run(GtkWindow *win, JamDoc *doc);
	image_dialog_run(GTK_WINDOW(jw), jw->doc);
}

static void
new_cb(JamWin *jw) {
	if (!jam_confirm_lose_entry(jw)) return;
	jam_clear_entry(jw);
}
static void
load_last_entry(JamWin *jw) {
	LJEntry *entry;

	if (!jam_confirm_lose_entry(jw)) return;

	entry = history_load_latest(GTK_WINDOW(jw), jw->account, jam_doc_get_usejournal(jw->doc));
	if (!entry)
		return;

	jam_doc_load_entry(jw->doc, entry);
	lj_entry_free(entry);
	undomgr_reset(UNDOMGR(jam_view_get_undomgr(JAM_VIEW(jw->view))));
}
static void
run_history_recent_dlg(JamWin *jw) {
	LJEntry *entry;

	if (!jam_confirm_lose_entry(jw)) return;

	if ((entry = history_recent_dialog_run(GTK_WINDOW(jw), jw->account, jam_doc_get_usejournal(jw->doc))) == NULL)
		return;

	jam_doc_load_entry(jw->doc, entry);
	lj_entry_free(entry);
	undomgr_reset(UNDOMGR(jam_view_get_undomgr(JAM_VIEW(jw->view))));
}

static void
run_offline_dlg(JamWin *jw) {
	LJEntry *offline_dlg_run(GtkWindow *, JamAccount *);
	LJEntry *entry;

	if (!jam_confirm_lose_entry(jw)) return;

	if ((entry = offline_dlg_run(GTK_WINDOW(jw), jw->account)) == NULL)
		return;

	jam_doc_load_entry(jw->doc, entry);
	lj_entry_free(entry);
	undomgr_reset(UNDOMGR(jam_view_get_undomgr(JAM_VIEW(jw->view))));
}

static void menu_save(JamWin *jw) { jam_save(jw); }
static void menu_save_as_file(JamWin *jw) { jam_save_as_file(jw); }
static void menu_save_as_draft(JamWin *jw) { jam_save_as_draft(jw); }

static void
menu_view_cb(JamWin *jw, int action, GtkCheckMenuItem *item) {
	JamView *view = jam_win_get_cur_view(jw);
	JamViewMeta meta = (JamViewMeta)(action - ACTION_VIEW_FIRST);
	gboolean show;

	show = gtk_check_menu_item_get_active(item);
	conf.options.showmeta[meta] = show;
	jam_view_toggle_meta(view, meta, show);
}

static void
meta_toggle_cb(JamView *view, JamViewMeta meta, gboolean show, JamWin *jw) {
	GtkCheckMenuItem *item;
	gboolean oldmeta;

	item = GTK_CHECK_MENU_ITEM(
		gtk_item_factory_get_item_by_action(jw->factory,
				ACTION_VIEW_FIRST+meta));

	/* don't let view-activated meta changes
	 * actually change the user's meta prefs. */
	oldmeta = conf.options.showmeta[meta];
	gtk_check_menu_item_set_active(item, show);
	conf.options.showmeta[meta] = oldmeta;
}

#if 0
static void
security_cb(JamWin *jw, int action, GtkCheckMenuItem *item) {
	LJSecurity sec = jam_doc_get_security(jw->doc);

	if (!gtk_check_menu_item_get_active(item))
		return;

	sec.type = action - ACTION_ENTRY_SECURITY_PUBLIC;
	if (sec.type == LJ_SECURITY_CUSTOM) {
		sec.allowmask = custom_security_dlg_run(GTK_WINDOW(jw),
					sec.allowmask, JAM_ACCOUNT_LJ(jw->account));
		/* if they didn't include anyone, it's a private entry. */
		if (sec.allowmask == 0)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
						gtk_item_factory_get_widget_by_action(jw->factory,
							ACTION_ENTRY_SECURITY_PRIVATE)), TRUE);
	}

	jam_doc_set_security(jw->doc, &sec);
}

static void
comments_cb(JamWin *jw, int action, GtkCheckMenuItem *item) {
	if (!gtk_check_menu_item_get_active(item))
		return;
	jam_doc_set_comments(jw->doc, action - ACTION_ENTRY_COMMENTS_DEFAULT);
}

static void
screening_cb(JamWin *jw, int action, GtkCheckMenuItem *item) {
	if (!gtk_check_menu_item_get_active(item))
		return;
	jam_doc_set_screening(jw->doc, action - ACTION_ENTRY_SCREENING_DEFAULT);
}

static void
backdated_cb(JamWin *jw, int action, GtkCheckMenuItem *item) {
	jam_doc_set_backdated(jw->doc, gtk_check_menu_item_get_active(item));
}

static void
preformatted_cb(JamWin *jw, int action, GtkCheckMenuItem *item) {
	jam_doc_set_preformatted(jw->doc, gtk_check_menu_item_get_active(item));
}
static void
picture_cb(GObject *item, JamWin *jw) {
	char *pic;
	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
		return;
	pic = g_object_get_data(item, KEY_PICTUREKEYWORD);
	jam_doc_set_pickeyword(jw->doc, pic);
}

#endif

static void
cutcopypaste_cb(JamWin *jw, int action) {
	GtkWidget *target;
	const char* signalname;
	guint signalid;

	switch (action) {
		case ACTION_CUT:   signalname = "cut_clipboard";   break;
		case ACTION_COPY:  signalname = "copy_clipboard";  break;
		case ACTION_PASTE: signalname = "paste_clipboard"; break;
		default: return;
	}

	/* verify that the focused widget actually responds to this signal
	 * before emitting it, because if it doesn't we get an ugly error. */
	target = gtk_window_get_focus(GTK_WINDOW(jw));
	signalid = g_signal_lookup(signalname, G_OBJECT_TYPE(target));
	if (signalid)
		g_signal_emit(target, signalid, 0); /* the last param appears to only
	                                           be used for property signals. */
}

static void
undoredo_cb(JamWin *jw, int action) {
	UndoMgr *um = UNDOMGR(jam_view_get_undomgr(JAM_VIEW(jw->view)));
	if (action == ACTION_UNDO) {
		if (undomgr_can_undo(um)) {
			undomgr_undo(um);
		}
	} else if (action == ACTION_REDO) {
		if (undomgr_can_redo(um)) {
			undomgr_redo(um);
		}
	}
}

static void
menu_insert_file(JamWin *jw) {
	tools_insert_file(GTK_WINDOW(jw), jw->doc);
}

static void
menu_insert_command_output(JamWin *jw) {
	tools_insert_command_output(GTK_WINDOW(jw), jw->doc);
}

static void
menu_html_escape(JamWin *jw) {
	tools_html_escape(GTK_WINDOW(jw), jw->doc);
}
static void
menu_remove_linebreaks(JamWin *jw) {
	tools_remove_linebreaks(GTK_WINDOW(jw), jw->doc);
}
static void
menu_ljcut(JamWin *jw) {
	tools_ljcut(GTK_WINDOW(jw), jw->doc);
}

static void
menu_lj_repost(JamWin *jw) {
	tools_lj_repost(GTK_WINDOW(jw), jw->doc);
}

static void
menu_embedded_media(JamWin *jw) {
	tools_embedded_media(GTK_WINDOW(jw), jw->doc);
}

static void
menu_validate_xml(JamWin *jw) {
	tools_validate_xml(GTK_WINDOW(jw), jw->doc);
}
static void
menu_sync(JamWin *jw) {
	sync_run(JAM_ACCOUNT_LJ(jw->account), GTK_WINDOW(jw));
}

static void
menu_console(JamWin *jw) {
	g_assert(JAM_ACCOUNT_IS_LJ(jw->account));
	console_dialog_run(GTK_WINDOW(jw), JAM_ACCOUNT_LJ(jw->account));
}

void
menu_friends_manager(JamWin *jw) {
	g_assert(JAM_ACCOUNT_IS_LJ(jw->account));
	friends_manager_show(GTK_WINDOW(jw), JAM_ACCOUNT_LJ(jw->account));
}

static void
menu_html_mark_bold(JamWin *jw) {
	html_mark_bold(jw->doc);
}
static void
menu_html_mark_italic(JamWin *jw) {
	html_mark_italic(jw->doc);
}
static void
menu_html_mark_em(JamWin *jw) {
	html_mark_em(jw->doc);
}
static void
menu_html_mark_underline(JamWin *jw) {
	html_mark_underline(jw->doc);
}
static void
menu_html_mark_strikeout(JamWin *jw) {
	html_mark_strikeout(jw->doc);
}
static void
menu_html_mark_monospaced(JamWin *jw) {
	html_mark_monospaced(jw->doc);
}
static void
menu_html_mark_para(JamWin *jw) {
	html_mark_para(jw->doc);
}
static void
menu_html_mark_smallcaps(JamWin *jw) {
	html_mark_smallcaps(jw->doc);
}
static void
menu_html_mark_superscript(JamWin *jw) {
	html_mark_superscript(jw->doc);
}
static void
menu_html_mark_subscript(JamWin *jw) {
	html_mark_subscript(jw->doc);
}
static void
menu_html_mark_blockquote(JamWin *jw) {
	html_mark_blockquote(jw->doc);
}
static void
menu_html_mark_small(JamWin *jw) {
	html_mark_small(jw->doc);
}
static void
menu_html_mark_big(JamWin *jw) {
	html_mark_big(jw->doc);
}
static void
menu_html_mark_ulist(JamWin *jw) {
	html_mark_ulist(jw->doc);
}
static void
menu_html_mark_olist(JamWin *jw) {
	html_mark_olist(jw->doc);
}
static void
menu_html_mark_listitem(JamWin *jw) {
	html_mark_listitem(jw->doc);
}
static void
menu_html_mark_h1(JamWin *jw) {
	html_mark_h1(jw->doc);
}
static void
menu_html_mark_h2(JamWin *jw) {
	html_mark_h2(jw->doc);
}
static void
menu_html_mark_h3(JamWin *jw) {
	html_mark_h3(jw->doc);
}
static void
menu_html_mark_h4(JamWin *jw) {
	html_mark_h4(jw->doc);
}

void manager_dialog(GtkWidget *parent);

static void
menu_usejournal_changed(JamWin *jw) {
	JamAccount *acc;
	GtkWidget *menu, *musejournal, *musejournalsep;
	LJUser *u = NULL;

	acc = jam_doc_get_account(jw->doc);
	if (JAM_ACCOUNT_IS_LJ(acc))
		u = jam_account_lj_get_user(JAM_ACCOUNT_LJ(acc));

	musejournal = gtk_item_factory_get_item_by_action(jw->factory,
			ACTION_JOURNAL_USE);
	musejournalsep = gtk_item_factory_get_item_by_action(jw->factory,
			ACTION_JOURNAL_USE_SEP);

	if (!u || !u->usejournals) {
		gtk_widget_hide(musejournal);
		gtk_widget_hide(musejournalsep);
		return;
	}

	menu = usejournal_build_menu(u->username,
	                             jam_doc_get_usejournal(jw->doc),
	                             u->usejournals,
	                             jam_win_get_cur_doc(jw));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(musejournal), menu);
	gtk_widget_show(musejournal);
	gtk_widget_show(musejournalsep);
}

GtkWidget*
menu_make_bar(JamWin *jw) {
	GtkWidget *bar;
	GtkAccelGroup *accelgroup = NULL;

/* a note on accelerators:
 *  shift-ctl-[x]  is used to type in UTF-8 chars;
 *  this means we can't use any shift-ctl accels
 *  using letters in the hex range [a-f]. */
static GtkItemFactoryEntry menu_items[] = {
{ N_("/_LogJam"),                      NULL, NULL, 0, "<Branch>" },
{ N_("/LogJam/_About LogJam..."),      NULL, about_dlg, 0, "<StockItem>", "logjam-goat" },
{ N_("/LogJam/_Servers and Users..."), NULL, manager_dialog, ACTION_MANAGER, NULL },
{ N_("/LogJam/_Change User..."),       NULL, jam_do_changeuser, ACTION_CHANGE_USER, NULL },
{ N_("/LogJam/_Preferences..."),       NULL, settings_run, 0, "<StockItem>", GTK_STOCK_PREFERENCES },
{    "/LogJam/---",                    NULL, NULL, 0, "<Separator>" },
{ N_("/LogJam/_Quit"),                 "<ctl>Q", jam_quit, 0, "<StockItem>", GTK_STOCK_QUIT },

{ N_("/E_ntry"),                       NULL, NULL, 0, "<Branch>" },
{ N_("/Entry/_New"),                   NULL, new_cb, 0, "<StockItem>", GTK_STOCK_NEW },
{ N_("/Entry/_Open"),                  NULL, NULL, 0, "<Branch>" },
{ N_("/Entry/Open/File..."),           NULL, jam_open_entry, 0, "<StockItem>", GTK_STOCK_OPEN },
{ N_("/Entry/Open/Draft..."),          NULL, jam_open_draft, 0, NULL },
{ N_("/Entry/_Save"),                  NULL, menu_save, 0, "<StockItem>", GTK_STOCK_SAVE },
{ N_("/Entry/Save _As"),               NULL, NULL, 0, "<Branch>" },
{ N_("/Entry/Save As/_File..."),       NULL, menu_save_as_file, 0, "<StockItem>", GTK_STOCK_SAVE_AS },
{ N_("/Entry/Save As/_Draft..."),      NULL, menu_save_as_draft, 0, NULL },

{    "/Entry/---",                     NULL, NULL, 0, "<Separator>" },
{ N_("/Entry/_Validate XML"),    NULL, menu_validate_xml, 0, NULL },
#ifdef HAVE_GTKHTML
{ N_("/Entry/_Preview HTML..."), NULL, preview_ui_show, 0, NULL },
#endif
{    "/Entry/---",                     NULL, NULL, ACTION_ENTRY_SUBMIT_SEP, "<Separator>" },
{ N_("/Entry/S_ubmit as New"),         NULL, jam_submit_entry, ACTION_ENTRY_SUBMIT_NEW, NULL },
{ N_("/Entry/_Save Changes to Server"), NULL, jam_save_entry_server, ACTION_ENTRY_SAVE_SERVER, NULL },

{ N_("/_Edit"),       NULL, NULL, 0, "<Branch>" },
{ N_("/Edit/_Undo"),  "<ctl>Z", undoredo_cb,
	ACTION_UNDO,	"<StockItem>", GTK_STOCK_UNDO },
{ N_("/Edit/_Redo"),  "<ctl>R", undoredo_cb,
	ACTION_REDO,	"<StockItem>", GTK_STOCK_REDO },
{    "/Edit/---",     NULL, NULL, 0, "<Separator>" },
{ N_("/Edit/Cu_t"),   NULL, cutcopypaste_cb,
	ACTION_CUT,   "<StockItem>", GTK_STOCK_CUT },
{ N_("/Edit/_Copy"),  NULL, cutcopypaste_cb,
	ACTION_COPY,  "<StockItem>", GTK_STOCK_COPY },
{ N_("/Edit/_Paste"), NULL, cutcopypaste_cb,
	ACTION_PASTE, "<StockItem>", GTK_STOCK_PASTE },
{    "/Edit/---",     NULL, NULL, 0, "<Separator>" },
{ N_("/Edit/_HTML Escape"),       NULL, menu_html_escape, 0, NULL },
{ N_("/Edit/_Remove Linebreaks"), NULL, menu_remove_linebreaks, 0, NULL },

{ N_("/_Insert"),                 NULL, NULL, 0, "<Branch>" },
{ N_("/Insert/_Poll..."),  NULL, run_poll_creator_dlg, 0, NULL },
{ N_("/Insert/_File..."),  NULL, menu_insert_file, 0, NULL },
{ N_("/Insert/Command _Output..."),  NULL, menu_insert_command_output,
	0, NULL },
{    "/Insert/---",               NULL, NULL, 0, "<Separator>" },
{ N_("/Insert/_Link..."),         "<ctl>L",      menu_make_link },
{ N_("/Insert/_Image..."),        NULL,          menu_insert_image },
{ N_("/Insert/_Journal Link..."), "<ctl><alt>L", menu_make_journal_link },
{ N_("/Insert/lj-_cut..."),       "<ctl><alt>X", menu_ljcut, 0, NULL },
{ N_("/Insert/lj-_repost..."),    "<ctl><alt>P", menu_lj_repost, 0, NULL },
{ N_("/Insert/_Embedded Media..."), "<ctl><alt>E", menu_embedded_media },

{ N_("/_View"),                   NULL, NULL, ACTION_VIEW, "<Branch>" },
{ N_("/View/_Security"),          NULL, menu_view_cb, ACTION_VIEW_SECURITY,     "<CheckItem>" },
{ N_("/View/Entry _Date"),        NULL, menu_view_cb, ACTION_VIEW_DATESEL,      "<CheckItem>" },
{ N_("/View/_Mood"),              NULL, menu_view_cb, ACTION_VIEW_MOOD,         "<CheckItem>" },
{ N_("/View/_Picture"),           NULL, menu_view_cb, ACTION_VIEW_PICTURE,      "<CheckItem>" },
{ N_("/View/_Tags"),              NULL, menu_view_cb, ACTION_VIEW_TAGS,         "<CheckItem>" },
{ N_("/View/M_usic"),             NULL, menu_view_cb, ACTION_VIEW_MUSIC,        "<CheckItem>" },
{ N_("/View/_Location"),          NULL, menu_view_cb, ACTION_VIEW_LOCATION,     "<CheckItem>" },
{ N_("/View/_Preformatted"),      NULL, menu_view_cb, ACTION_VIEW_PREFORMATTED, "<CheckItem>" },
{ N_("/View/_Comments"),          NULL, menu_view_cb, ACTION_VIEW_COMMENTS,     "<CheckItem>" },
{ N_("/View/Scr_eening"),         NULL, menu_view_cb, ACTION_VIEW_SCREENING,    "<CheckItem>" },

{ N_("/_HTML"),						NULL, NULL, 0, "<Branch>" },
{ N_("/HTML/_Bold"),				"<ctl><alt>B", menu_html_mark_bold },
{ N_("/HTML/_Italic"),				"<ctl><alt>I", menu_html_mark_italic },
{ N_("/HTML/Emphasized"),			"<ctl><shift>M", menu_html_mark_em },
{ N_("/HTML/_Strikeout"),			"<ctl><alt>S", menu_html_mark_strikeout },
{ N_("/HTML/Small _Caps"),			"<ctl><alt>C", menu_html_mark_smallcaps },
{ N_("/HTML/_Monospaced"),			"<ctl><alt>M", menu_html_mark_monospaced },
{ N_("/HTML/_Underline"),			"<ctl><alt>U", menu_html_mark_underline },
{ N_("/HTML/_Blockquote"),			"<ctl><alt>Q", menu_html_mark_blockquote },
{ N_("/HTML/_Paragraph"),	        "<ctl><shift>P", menu_html_mark_para },
{    "/HTML/---",					NULL, NULL, 0, "<Separator>" },
{ N_("/HTML/Heading _1"),			"<ctl><alt>1", menu_html_mark_h1 },
{ N_("/HTML/Heading _2"),			"<ctl><alt>2", menu_html_mark_h2 },
{ N_("/HTML/Heading _3"),			"<ctl><alt>3", menu_html_mark_h3 },
{ N_("/HTML/Heading _4"),			"<ctl><alt>4", menu_html_mark_h4 },
{ N_("/HTML/Bi_gger"),				"<ctl><shift>B", menu_html_mark_big },
{ N_("/HTML/S_maller"),				"<ctl><shift>S", menu_html_mark_small },
{ N_("/HTML/Superscrip_t"),			"<ctl><shift>T", menu_html_mark_superscript },
{ N_("/HTML/S_ubscript"),			"<ctl><shift>U", menu_html_mark_subscript },
{    "/HTML/---",NULL, NULL, 0, 	"<Separator>" },
{ N_("/HTML/U_nordered List"),		"<ctl><shift>N", menu_html_mark_ulist },
{ N_("/HTML/_Ordered list"),		"<ctl><shift>O", menu_html_mark_olist },
{ N_("/HTML/_List item"),			"<ctl><shift>L", menu_html_mark_listitem },

{ N_("/_Journal"),                      NULL,          NULL, ACTION_JOURNAL, "<Branch>" },
{ N_("/Journal/_Use Journal"),          NULL,          NULL, ACTION_JOURNAL_USE, NULL },
{    "/Journal/---",                    NULL, NULL, ACTION_JOURNAL_USE_SEP, "<Separator>" },
{ N_("/Journal/_Friends..."),           "<ctl><alt>F", menu_friends_manager, 0, NULL },
{    "/Journal/---",                    NULL, NULL, 0, "<Separator>" },
{ N_("/Journal/_Load Last Entry"),      NULL,          load_last_entry, 0, NULL },
{ N_("/Journal/_Load Recent Entry..."), NULL,          run_history_recent_dlg, 0, NULL },
{ N_("/Journal/_Load Offline Entry..."), NULL,         run_offline_dlg, 0, NULL },
{ N_("/Journal/_Synchronize Offline Copy..."), NULL,   menu_sync, 0, NULL },
{    "/Journal/---",                      NULL, NULL, 0, "<Separator>" },
{ N_("/Journal/_LiveJournal Console..."), NULL, menu_console, ACTION_TOOLS_CONSOLE, NULL },
{ N_("/Journal/_Web Links"),              NULL, NULL, ACTION_WEB_LINKS, "<Branch>" },

};
	int itemcount = sizeof(menu_items) / sizeof(menu_items[0]);
	JamView *view;

	accelgroup = gtk_accel_group_new();
	jw->factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accelgroup);
	g_object_ref(jw->factory);
	gtk_item_factory_set_translate_func(jw->factory, gettext_translate_func, NULL, NULL);
	gtk_item_factory_create_items(jw->factory, itemcount, menu_items, jw);
	gtk_window_add_accel_group(GTK_WINDOW(jw), accelgroup);

	bar = gtk_item_factory_get_widget(jw->factory, "<main>");

	/* Entry menu */
	jw->msubmitsep = gtk_item_factory_get_widget_by_action(jw->factory,
			ACTION_ENTRY_SUBMIT_SEP);
	jw->msubmit = gtk_item_factory_get_widget_by_action(jw->factory,
			ACTION_ENTRY_SUBMIT_NEW);
	jw->msaveserver = gtk_item_factory_get_widget_by_action(jw->factory,
			ACTION_ENTRY_SAVE_SERVER);
	gtk_widget_hide(jw->msaveserver);

	/* Edit menu */
	jw->mundo = gtk_item_factory_get_widget_by_action(jw->factory,
			ACTION_UNDO);
	jw->mredo = gtk_item_factory_get_widget_by_action(jw->factory,
			ACTION_REDO);

	/* Journal menu */
	jw->mweb = gtk_item_factory_get_item_by_action(jw->factory, ACTION_WEB_LINKS);

	view = jam_win_get_cur_view(jw);
	g_signal_connect(G_OBJECT(view), "meta_toggle",
			G_CALLBACK(meta_toggle_cb), jw);
	/* the view has already loaded which metas are available;
	 * we tell it to emit those signals again. */
	jam_view_emit_conf(view);

	menu_new_doc(jw);

	gtk_widget_show(bar);
	return bar;
}

void
menu_new_doc(JamWin *jw) {
	JamDoc *doc = jw->doc;
	g_signal_connect_swapped(doc, "notify::account",
			G_CALLBACK(menu_account_changed), jw);
	g_signal_connect_swapped(doc, "notify::usejournal",
			G_CALLBACK(menu_usejournal_changed), jw);
	menu_account_changed(jw);
	menu_usejournal_changed(jw);
}

static void
menu_spawn_url(GtkWidget *w, GtkWindow *jw) {
	char *url;
	url = g_object_get_data(G_OBJECT(w), "url");
	if (url)
		spawn_url(jw, url);
}

static GtkWidget* webmenu_widget(JamWin *jw, GSList *webmenu);

static GtkWidget*
webmenuitem_widget(JamWin *jw, LJWebMenuItem *wmi) {
	GtkWidget *item = NULL;
	if (!wmi->text) { /* separator */
		item = gtk_menu_item_new();
	} else if (wmi->url) { /* url menu item */
		item = gtk_image_menu_item_new_with_label(wmi->text);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
				gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));

		g_object_set_data_full(G_OBJECT(item), "url", wmi->url, NULL);
		g_signal_connect(G_OBJECT(item), "activate",
				G_CALLBACK(menu_spawn_url), jw);
	} else if (wmi->subitems) { /* submenu */
		GtkWidget *submenu;

		item = gtk_menu_item_new_with_mnemonic(wmi->text);
		submenu = webmenu_widget(jw, wmi->subitems);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	}
	return item;
}

static GtkWidget*
webmenu_widget(JamWin *jw, GSList *webmenu) {
	GtkWidget *menu, *item;
	GSList *l;

	menu = gtk_menu_new();
	for (l = webmenu; l != NULL; l = l->next) {
		item = webmenuitem_widget(jw, l->data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
	return menu;
}

static void
menu_update_web(JamWin *jw, JamAccount *acc) {
	LJUser *u = NULL;

	if (JAM_ACCOUNT_IS_LJ(acc))
		u = jam_account_lj_get_user(JAM_ACCOUNT_LJ(acc));

	if (!u || !u->webmenu) {
		gtk_widget_hide(jw->mweb);
		return;
	}

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(jw->mweb),
			webmenu_widget(jw, u->webmenu));
	gtk_widget_show_all(jw->mweb);
}

static void
menu_account_changed(JamWin *jw) {
	JamAccount *acc = jam_doc_get_account(jw->doc);

	if (JAM_ACCOUNT_IS_LJ(acc)) {
		menu_update_web(jw, acc);
		gtk_widget_set_sensitive(gtk_item_factory_get_item_by_action(jw->factory,
			ACTION_VIEW), TRUE);
		gtk_widget_set_sensitive(gtk_item_factory_get_item_by_action(jw->factory,
			ACTION_JOURNAL), TRUE);
	} else {
		gtk_widget_set_sensitive(gtk_item_factory_get_item_by_action(jw->factory,
			ACTION_VIEW), FALSE);
		gtk_widget_set_sensitive(gtk_item_factory_get_item_by_action(jw->factory,
			ACTION_JOURNAL), FALSE);
	}
}

