/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <string.h>

#include "util-gtk.h"
#ifdef HAVE_GTKHTML
#include "preview.h"
#endif
#include "journalstore.h"
#include "sync.h"
#include "spawn.h"

typedef struct {
	GtkDialog dlg;

	GtkWidget *cal, *searchentry, *searchcase;
	GtkWidget *summary, *listbox, *list, *preview;
	GdkPixbuf *pb_friends, *pb_private;
	GtkListStore *store;

	JournalStore *journalstore;
	JamAccount *account;

	int current_itemid; /* itemid of entry being previewed. useful to
	                       pop back the calendar after some browsing */
} OfflineUI;

enum {
	COL_ITEMID,
	COL_DATE,
	COL_SECURITY,
	COL_SUMMARY
};

static GType offlineui_get_type(void);
#define OFFLINEUI(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), offlineui_get_type(), OfflineUI))

//static const gint LJ_OFFLINE_JUMP_TO = 1;
#define LJ_OFFLINE_JUMP_TO  1
#define LJ_OFFLINE_COPY_URL 2

static void
update_marks(OfflineUI *oui) {
	guint year, mon;
	int day;
	guint32 days;

	gtk_calendar_get_date(GTK_CALENDAR(oui->cal), &year, &mon, NULL);
	days = journal_store_get_month_entries(oui->journalstore, year, mon+1);

	gtk_calendar_clear_marks(GTK_CALENDAR(oui->cal));
	for (day = 1; day <= 31; day++) {
		if (days & (1 << day))
			gtk_calendar_mark_day(GTK_CALENDAR(oui->cal), day);
	}
}

static void
month_changed_cb(GtkCalendar *calendar, OfflineUI *oui) {
	update_marks(oui);
}

static void
get_entries_cb(int itemid, time_t etime, const char *summary, LJSecurity *sec, gpointer data) {
	GtkTreeIter iter;
	OfflineUI *oui = (OfflineUI*)data;
	GdkPixbuf *pb;

	switch (sec->type) {
		case LJ_SECURITY_FRIENDS:
		case LJ_SECURITY_CUSTOM:
			pb = oui->pb_friends; break;
		case LJ_SECURITY_PRIVATE:
			pb = oui->pb_private; break;
		default:
			pb = NULL; /* public: no icon */
	}

	gtk_list_store_append(oui->store, &iter);
	gtk_list_store_set(oui->store, &iter,
			COL_ITEMID, itemid,
			COL_DATE, etime,
			COL_SECURITY, pb,
			COL_SUMMARY, summary,
			-1);
}

static void
load_day(OfflineUI *oui) {
	guint year, mon, day;
	gtk_calendar_get_date(GTK_CALENDAR(oui->cal), &year, &mon, &day);
	if (oui->summary) {
		gtk_widget_destroy(oui->summary);
		oui->summary = NULL;
	}
	gtk_list_store_clear(oui->store);
	journal_store_get_day_entries(oui->journalstore, year, mon+1, day,
			get_entries_cb, oui);
}

static void
day_selected_cb(GtkCalendar *calendar, OfflineUI *oui) {
	GtkTreeIter iter;

	load_day(oui);
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(oui->store), &iter)) {
		gtk_tree_selection_select_iter(
				gtk_tree_view_get_selection(GTK_TREE_VIEW(oui->list)),
				&iter);
	}
}

static void
selection_changed_cb(GtkTreeSelection *ts, OfflineUI *oui) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean sensitive = gtk_tree_selection_get_selected(ts, &model, &iter);

	gtk_dialog_set_response_sensitive(GTK_DIALOG(oui),
			LJ_OFFLINE_JUMP_TO, sensitive);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(oui),
			LJ_OFFLINE_COPY_URL, sensitive);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(oui),
			GTK_RESPONSE_OK, sensitive);
}

static void
make_list(OfflineUI *oui) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;

	oui->store = gtk_list_store_new(4, G_TYPE_INT, G_TYPE_LONG, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(oui->store),
			COL_DATE, GTK_SORT_ASCENDING);
	oui->list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(oui->store));
	g_object_unref(G_OBJECT(oui->store));

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(oui->list));
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(selection_changed_cb), oui);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(oui->list), FALSE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Summary"));
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", COL_SECURITY);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", COL_SUMMARY);

	gtk_tree_view_column_set_sort_column_id(column, COL_SUMMARY);
	gtk_tree_view_append_column(GTK_TREE_VIEW(oui->list), column);
}

static int
get_selected_itemid(OfflineUI *oui) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	int itemid;
	if (!gtk_tree_selection_get_selected(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(oui->list)),
			&model, &iter))
		return 0;
	gtk_tree_model_get(model, &iter,
			COL_ITEMID, &itemid,
			-1);
	return itemid;
}

static LJEntry*
load_selected(OfflineUI *oui) {
	LJEntry *entry;
	int itemid;

	itemid = get_selected_itemid(oui);
	if (!itemid)
		return NULL;
	entry = journal_store_get_entry(oui->journalstore, itemid);
	if (!entry)
		g_warning("unable to find entry %d\n", itemid);
	return entry;
}

static void
select_entry(OfflineUI *oui, int itemid) {
 	time_t etime;
	struct tm *etm;
	guint year, mon, day;
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(oui->store);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(oui->list));
	int fitemid;

	etime = journal_store_lookup_entry_time(oui->journalstore, itemid);

	etm = gmtime(&etime);
	gtk_calendar_get_date(GTK_CALENDAR(oui->cal), &year, &mon, &day);

	gtk_calendar_freeze(GTK_CALENDAR(oui->cal));
	g_signal_handlers_block_matched(oui->cal, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, oui);

	/* the calendar gets confused in situations like switching into
	 * a 30-day month and have day 31 selected.
	 * so we move to the middle, switch months, and then move to the right place. */
	if (day != etm->tm_mday)
		gtk_calendar_select_day(GTK_CALENDAR(oui->cal), 15);
	if (year != etm->tm_year+1900 || mon != etm->tm_mon)
		gtk_calendar_select_month(GTK_CALENDAR(oui->cal),
				etm->tm_mon, etm->tm_year+1900);
	if (day != etm->tm_mday)
		gtk_calendar_select_day(GTK_CALENDAR(oui->cal), etm->tm_mday);

	if (day != etm->tm_mday)
		load_day(oui);
	if (year != etm->tm_year+1900 || mon != etm->tm_mon)
		update_marks(oui);

	g_signal_handlers_unblock_matched(oui->cal, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, oui);
	gtk_calendar_thaw(GTK_CALENDAR(oui->cal));

	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gtk_tree_model_get(model, &iter, COL_ITEMID, &fitemid, -1);
			if (fitemid == itemid) {
				gtk_tree_selection_select_iter(sel, &iter);
				break;
			}
		} while (gtk_tree_model_iter_next(model, &iter));
	}
}

/* move the selection to the nearest next or previous item. The present
 * selection need not be an actual entry. */
static void
move_relative(OfflineUI *oui, int dir) {
	time_t etime;
	gboolean found = FALSE;
	int itemid = get_selected_itemid(oui);

	if (itemid) { /* selection exists */
		found = journal_store_find_relative(oui->journalstore, itemid,
				&itemid, dir, NULL);
	} else {     /* some more cajoling necessary */
		guint year, month, day;
		struct tm when_tm = {0};
		gtk_calendar_get_date(GTK_CALENDAR(oui->cal), &year, &month, &day);
		when_tm.tm_year = year-1900;
		when_tm.tm_mon  = month;
		when_tm.tm_mday = day;
		
		found = journal_store_find_relative_by_time(oui->journalstore,
				lj_timegm(&when_tm), &itemid, dir, NULL);
	}
	if (found)
		select_entry(oui, itemid);
}

static void
prev_cb(GtkWidget *button, OfflineUI *oui) {
	move_relative(oui, -1);
}

static void
next_cb(GtkWidget *button, OfflineUI *oui) {
	move_relative(oui, 1);
}

static GtkWidget*
make_cal(OfflineUI *oui) {
	GtkWidget *calbox, *bbox, *next, *prev;

	calbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(calbox), 6);

	oui->cal = gtk_calendar_new();
	gtk_calendar_display_options(GTK_CALENDAR(oui->cal),
			GTK_CALENDAR_SHOW_HEADING);
	
	g_signal_connect(G_OBJECT(oui->cal), "month_changed",
			G_CALLBACK(month_changed_cb), oui);
	g_signal_connect(G_OBJECT(oui->cal), "day_selected",
			G_CALLBACK(day_selected_cb), oui);
	gtk_box_pack_start(GTK_BOX(calbox), oui->cal, FALSE, FALSE, 0);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_EDGE);
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	prev = gtk_button_new_from_stock(GTK_STOCK_GO_BACK);
	g_signal_connect(G_OBJECT(prev), "clicked",
			G_CALLBACK(prev_cb), oui);
	gtk_box_pack_start(GTK_BOX(bbox), prev, FALSE, FALSE, 0);
	next = gtk_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	g_signal_connect(G_OBJECT(next), "clicked",
			G_CALLBACK(next_cb), oui);
	gtk_box_pack_start(GTK_BOX(bbox), next, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(calbox), bbox, FALSE, FALSE, 0);

	return calbox;
}

static gboolean
search_case_cb(const char *str, gpointer data) {
	char *down;
	gboolean ret;
	if (!str) return FALSE;
	down = g_utf8_strdown(str, -1);
	ret = (strstr(down, data) != NULL);
	g_free(down);
	return ret;
}

static gboolean
search_cb(const char *str, gpointer data) {
	return str && strstr(str, data) != NULL;
}

static void
find_cb(GtkWidget *button, OfflineUI *oui) {
	const char *search;
	char *summary;

	search = gtk_entry_get_text(GTK_ENTRY(oui->searchentry));

	gtk_list_store_clear(oui->store);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(oui->searchcase))) {
		journal_store_scan(oui->journalstore,
				search_cb, (gpointer)search,
				get_entries_cb, oui);
	} else {
		char *lsearch = g_utf8_strdown(search, -1);
		journal_store_scan(oui->journalstore,
				search_case_cb, lsearch,
				get_entries_cb, oui);
		g_free(lsearch);
	}

	summary = g_strdup_printf(_("Search found %d entries:"),
			gtk_tree_model_iter_n_children(GTK_TREE_MODEL(oui->store), NULL));
	if (!oui->summary) {
		oui->summary = gtk_label_new(summary);
		gtk_widget_show(oui->summary);
		gtk_box_pack_start(GTK_BOX(oui->listbox), oui->summary,
				FALSE, FALSE, 0);
	} else {
		gtk_label_set_text(GTK_LABEL(oui->summary), summary);
	}
	g_free(summary);
}

static GtkWidget*
make_search(OfflineUI *oui) {
	GtkWidget *box, *l, *bbox, *button;

	box = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(box), 6);

	l = gtk_label_new_with_mnemonic(_("_Search:"));
	gtk_misc_set_alignment(GTK_MISC(l), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
	oui->searchentry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), oui->searchentry, FALSE, FALSE, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(l), oui->searchentry);

	oui->searchcase = gtk_check_button_new_with_mnemonic(_("_Case sensitive"));
	gtk_box_pack_start(GTK_BOX(box), oui->searchcase, FALSE, FALSE, 0);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	button = gtk_button_new_from_stock(GTK_STOCK_FIND);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(find_cb), oui);
	g_signal_connect(G_OBJECT(oui->searchentry), "activate",
			G_CALLBACK(find_cb), oui);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), bbox, FALSE, FALSE, 0);
	
	return box;
}

static gboolean
check_entries(GtkWindow *parent, JamAccount *acc) {
	GtkWidget *dlg;
	int res;

	dlg = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
			GTK_MESSAGE_INFO, GTK_BUTTONS_NONE,
			_("LogJam has no offline entries for this journal. "
				"You must first synchronize this journal to your local "
				"storage before you can load entries from it."));
	gtk_dialog_add_buttons(GTK_DIALOG(dlg),
			_("_Synchronize"), GTK_RESPONSE_OK,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			NULL);
	res = gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
	if (res == GTK_RESPONSE_OK)
		return sync_run(JAM_ACCOUNT_LJ(acc), parent);
	return FALSE;
}

#ifndef HAVE_GTKHTML
static void
content_update(OfflineUI *oui) {
	LJEntry *entry;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	entry = load_selected(oui);
	if (!entry)
		return;
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(oui->preview));
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	gtk_text_buffer_delete(buffer, &start, &end);
	gtk_text_buffer_insert_at_cursor(buffer,
			"(Note: this is not a real HTML preview because LogJam "
			"was compiled without HTML support.  This widget is in "
			"here as a placeholder until we figure out what to put "
			"here.)\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, entry->event, -1);

	lj_entry_free(entry);
}
#endif

static void
offlineui_init(GTypeInstance *instance, gpointer g_class) {
	OfflineUI *oui = OFFLINEUI(instance);
	GtkWidget *hbox, *vbox, *nb, *copy;
	GtkTreeSelection *sel;

	oui->pb_friends = gtk_widget_render_icon(GTK_WIDGET(oui), 
			"logjam-protected", GTK_ICON_SIZE_MENU, NULL);
	oui->pb_private = gtk_widget_render_icon(GTK_WIDGET(oui), 
			"logjam-private", GTK_ICON_SIZE_MENU, NULL);

	gtk_window_set_default_size(GTK_WINDOW(oui), 400, 500);
	gtk_window_set_title(GTK_WINDOW(oui), _("Select Offline Entry"));

	vbox = gtk_vbox_new(FALSE, 6);
	hbox = gtk_hbox_new(FALSE, 6);

	nb = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), make_cal(oui),
			gtk_label_new(_("Select by Date")));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), make_search(oui),
			gtk_label_new(_("Select by Search")));

	gtk_box_pack_start(GTK_BOX(hbox), nb, FALSE, FALSE, 0);

	make_list(oui);
	oui->listbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_end(GTK_BOX(oui->listbox), scroll_wrap(oui->list),
			TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), oui->listbox, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(oui->list));
#ifdef HAVE_GTKHTML
	oui->preview = html_preview_new((GetEntryFunc)load_selected, oui);
	gtk_box_pack_start(GTK_BOX(vbox), scroll_wrap(oui->preview), TRUE, TRUE, 0);
	g_signal_connect_swapped(G_OBJECT(sel), "changed",
			G_CALLBACK(preview_update), oui->preview);
#else
	oui->preview = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(oui->preview), GTK_WRAP_WORD);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(oui->preview), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), scroll_wrap(oui->preview), TRUE, TRUE, 0);
	g_signal_connect_swapped(G_OBJECT(sel), "changed",
			G_CALLBACK(content_update), oui);
#endif

	gtk_widget_show_all(vbox);
	jam_dialog_set_contents(GTK_DIALOG(oui), vbox);

	copy = gtk_button_new_from_stock(GTK_STOCK_COPY);
	gtk_tooltips_set_tip(app.tooltips, copy,
			_("Copy online entry URL to clipboard"),
			_("Clicking here will place the URL of the entry currently "
			"being previewed in your clipboard, so you can link to it."));
	gtk_dialog_add_action_widget(GTK_DIALOG(oui),
			copy,              LJ_OFFLINE_COPY_URL);
	gtk_widget_show(copy);

	gtk_dialog_add_buttons(GTK_DIALOG(oui),
			GTK_STOCK_JUMP_TO, LJ_OFFLINE_JUMP_TO,
			GTK_STOCK_CANCEL,  GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK,      GTK_RESPONSE_OK,
			NULL);
	/* let the selection-changed watcher update the button sensitivities. */
	selection_changed_cb(gtk_tree_view_get_selection(GTK_TREE_VIEW(oui->list)),
			oui);
	geometry_tie(GTK_WIDGET(oui), GEOM_OFFLINE);
}

static void
offlineui_finalize(GObject *object) {
	GObjectClass *parent_class;
	OfflineUI *oui = OFFLINEUI(object);

	journal_store_free(oui->journalstore);
	g_object_unref(oui->pb_friends);
	g_object_unref(oui->pb_private);

	parent_class = g_type_class_peek_parent(G_OBJECT_GET_CLASS(object));
	parent_class->finalize(object);
}

static void
offlineui_class_init(gpointer klass, gpointer class_data) {
	GObjectClass *gclass = G_OBJECT_CLASS(klass);

	gclass->finalize = offlineui_finalize;
}

static GType
offlineui_get_type(void) {
	static GType new_type = 0;
	if (!new_type) {
		const GTypeInfo new_info = {
			sizeof(GtkDialogClass),
			NULL,
			NULL,
			(GClassInitFunc) offlineui_class_init,
			NULL,
			NULL,
			sizeof(OfflineUI),
			0,
			offlineui_init
		};
		new_type = g_type_register_static(GTK_TYPE_DIALOG,
				"OfflineUI", &new_info, 0);
	}
	return new_type;
}

static GtkWidget*
offlineui_new(void) {
	return GTK_WIDGET(g_object_new(offlineui_get_type(), NULL));
}

static gchar*
offline_current_entry_link(OfflineUI *oui) {
	/*gint itemid, anum;*/
	gchar *url;
	JamAccount *acc = oui->account;
	LJEntry *e = load_selected(oui);

	if (e == NULL)
		return NULL;

	if (! JAM_ACCOUNT_IS_LJ(acc))
		return NULL; // XXX: warning?

	/* XXX: until 1721 closes
	itemid = lj_entry_get_itemid(e);
	anum   = lj_entry_get_anum(e);

	url = g_strdup_printf("%s/users/%s/%d.html",
			jam_account_lj_get_server(acc)->url,
			jam_account_lj_get_user(acc)->username,
			itemid * 256 + anum); */
	{ // XXX: 1721
		struct tm time;
		lj_entry_get_time(e, &time);
		url = g_strdup_printf("%s/users/%s/%04d/%02d/%02d/",
				jam_account_lj_get_server(JAM_ACCOUNT_LJ(acc))->url,
				jam_account_lj_get_user(JAM_ACCOUNT_LJ(acc))->username,
				time.tm_year + 1900,
				time.tm_mon + 1,
				time.tm_mday);
	}
	
	return url;
}

static void
offline_jump_to_online(OfflineUI *oui) {
	gchar *url = offline_current_entry_link(oui);
	spawn_url(GTK_WINDOW(oui), url);
	g_free(url);
}

static void
offline_copy_link(OfflineUI *oui) {
	gchar *url = offline_current_entry_link(oui);
	if (url && *url != '\0')
		gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
				url, -1 /* determine length automatically */);
	g_free(url);
}

LJEntry*
offline_dlg_run(GtkWindow *parent, JamAccount *acc) {
	JournalStore *js = NULL;
	GtkWidget *oui;
	LJEntry *entry = NULL;
	gboolean run = TRUE;

	while (js == NULL) {
		js = journal_store_open(acc, FALSE, NULL);
		if (js == NULL)
			if (!check_entries(parent, acc))
				return NULL;
	}

	oui = offlineui_new();
	OFFLINEUI(oui)->journalstore = js;
	OFFLINEUI(oui)->account = acc;
	month_changed_cb(GTK_CALENDAR(OFFLINEUI(oui)->cal), OFFLINEUI(oui));
	day_selected_cb (GTK_CALENDAR(OFFLINEUI(oui)->cal), OFFLINEUI(oui));
	gtk_window_set_transient_for(GTK_WINDOW(oui), parent);

	while (run) {
		switch (gtk_dialog_run(GTK_DIALOG(oui))) {
			case LJ_OFFLINE_COPY_URL:
				offline_copy_link(OFFLINEUI(oui));
				break;
			case LJ_OFFLINE_JUMP_TO:
				offline_jump_to_online(OFFLINEUI(oui));
				break;
			case GTK_RESPONSE_OK:
				entry = load_selected(OFFLINEUI(oui));
				/* fallthrough */
			default:
				run = FALSE;
		}
	}
	
	gtk_widget_destroy(oui);

	return entry;
}

