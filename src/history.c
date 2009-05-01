/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "gtk-all.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <livejournal/getevents.h>

#include "conf.h"
#include "network.h"
#include "spawn.h"
#include "history.h"

#if 0
typedef struct {
	GtkWidget *win;
	GtkWidget *daylabel;
	GtkWidget *evlist;
	GtkWidget *bbox;
	guint firstyear, lastyear;
	guint year, mon, day;

	/* because day is never greater than 31, we can use a bitfield. */
	guint32 *markmonths;
} history_calendar_dlg;
#define MARKMONTH(h, year, mon) (h)->markmonths[((year)-(h)->firstyear)*12+((mon)-1)]
#define MARKMONTH_CUR(h) MARKMONTH(h, h->year, h->mon)

/*typedef struct {
	GtkWidget *win;
	GtkWidget *esubject;
	GtkWidget *eentry;
	GtkWidget *eyear, *emon, *eday, *ehour, *emin;
	GtkWidget *metamgr, *secmgr;
	security_data security;
	int itemid;

	history_calendar_dlg *hcdlg;
} history_item_dlg;*/

static gboolean getdaycounts_run(history_calendar_dlg* hcdlg);
void popcalendar_run(GtkWidget *parent, guint date[], int markfirstyear, guint32 marks[]);

static void change_day(history_calendar_dlg *hcdlg);


static void hc_row_selected(GtkCList *list, gint row, gint col, 
		GdkEventButton *event, history_calendar_dlg *hcdlg);
static gint hc_list_click_cb(GtkCList *list, GdkEventButton *be, 
		history_calendar_dlg *hcdlg);
static void hc_edit_cb(GtkWidget *w, history_calendar_dlg *hcdlg);
//static void hc_web_cb(GtkWidget *w, history_calendar_dlg *hcdlg);

/* fixme static gint hi_dialog_run(GtkWidget *parent, int itemid);*/

static void
free_dlg_cb(GtkWidget *w, history_calendar_dlg *hcdlg) {
	if (hcdlg->markmonths) g_free(hcdlg->markmonths);
	g_free(hcdlg);
}

void set_date(history_calendar_dlg *hcdlg) {
	time_t ttime;
	struct tm tm, *ptm;
	char buf[200];

	memset(&tm, 0, sizeof(struct tm));
	tm.tm_year = hcdlg->year-1900; tm.tm_mon = hcdlg->mon-1; tm.tm_mday = hcdlg->day;

	/* convert to time_t and back, so the weekday and such get updated. */
	ttime = mktime(&tm);
	ptm = localtime(&ttime);

	strftime(buf, 199, "%A, %d %B %Y", ptm);
	gtk_label_set_text(GTK_LABEL(hcdlg->daylabel), buf);
	change_day(hcdlg);
}

void set_current_date(history_calendar_dlg *hcdlg) {
	time_t curtime;
	struct tm *ptm;

	curtime = time(NULL);
	ptm = localtime(&curtime);

	hcdlg->year = ptm->tm_year + 1900;
	hcdlg->mon = ptm->tm_mon + 1;
	hcdlg->day = ptm->tm_mday;

	set_date(hcdlg);
}

static void
popup_calendar(GtkWidget *w, history_calendar_dlg *hcdlg) {
	guint date[3];
	date[0] = hcdlg->year;
	date[1] = hcdlg->mon;
	date[2] = hcdlg->day;

	popcalendar_run(hcdlg->win, date, hcdlg->firstyear, hcdlg->markmonths);

	if (hcdlg->year != date[0] || hcdlg->mon != date[1] || hcdlg->day != date[2]) {
		hcdlg->year = date[0];
		hcdlg->mon = date[1];
		hcdlg->day = date[2];
		set_date(hcdlg);
	}
}

static GtkWidget*
make_contents(history_calendar_dlg *hcdlg) {
	GtkWidget *vbox, *button, *scrollwin;
	gchar *titles[] = { "Time", "Event" };

	button = gtk_button_new();
	hcdlg->daylabel = gtk_label_new("");
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(popup_calendar), hcdlg);
	gtk_container_add(GTK_CONTAINER(button), hcdlg->daylabel);

	scrollwin = gtk_scrolled_window_new (NULL, NULL); 
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW
		(scrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	hcdlg->evlist = gtk_clist_new_with_titles(2, titles);
	gtk_clist_set_selection_mode(GTK_CLIST(hcdlg->evlist),
			GTK_SELECTION_BROWSE);
	g_signal_connect(G_OBJECT(hcdlg->evlist), "select-row", 
			G_CALLBACK(hc_row_selected), hcdlg);
	g_signal_connect(G_OBJECT(hcdlg->evlist), "unselect-row", 
			G_CALLBACK(hc_row_selected), hcdlg);
	g_signal_connect(G_OBJECT(hcdlg->evlist), "button_press_event",
			G_CALLBACK(hc_list_click_cb), hcdlg);
	gtk_clist_column_titles_passive(GTK_CLIST(hcdlg->evlist));
	/* fixme gtk2 gtk_clist_set_column_width(GTK_CLIST(hcdlg->evlist), 0, 
			gdk_string_width(hcdlg->evlist->style->font, "00:00a"));*/
	titles[0] = NULL;
	titles[1] = "[select a day]";
	gtk_clist_append(GTK_CLIST(hcdlg->evlist), titles);
	gtk_clist_set_selectable(GTK_CLIST(hcdlg->evlist), 0, FALSE);
	gtk_container_add(GTK_CONTAINER(scrollwin), hcdlg->evlist);

	vbox = gtk_vbox_new(FALSE, 5); 
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scrollwin, TRUE, TRUE, 0);
	return vbox;
}

static GtkWidget*
make_buttonbox(history_calendar_dlg *hcdlg) {
	GtkWidget *box, *button;

	box = jam_dialog_buttonbox_new();
	button = gtk_button_new_with_label(" Edit... ");
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(hc_edit_cb), hcdlg);
	jam_dialog_buttonbox_add(box, button);

	/*button = web_button(hcdlg->win, "View...");
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(hc_web_cb), hcdlg);
	jam_dialog_buttonbox_add(box, button);*/
	return box;
}

void history_dialog(GtkWidget *mainwin) {
	history_calendar_dlg *hcdlg;

	hcdlg = g_new0(history_calendar_dlg, 1);

	hcdlg->win = jam_dialog_new(mainwin, "History Overview", 400, -1);
	g_signal_connect(G_OBJECT(hcdlg->win), "destroy",
			G_CALLBACK(free_dlg_cb), hcdlg);

	hcdlg->bbox = make_buttonbox(hcdlg);
	jam_dialog_set_contents_buttonbox(hcdlg->win,
			make_contents(hcdlg),
			hcdlg->bbox);

	jam_dialog_add_close(hcdlg->win);
	gtk_widget_set_sensitive(hcdlg->bbox, FALSE);
	gtk_widget_show(hcdlg->win);

	if (!getdaycounts_run(hcdlg))
		return;
	set_current_date(hcdlg);
}

static void
daycounts_hash_extents_cb(gpointer key, gpointer value, gpointer data) {
	history_calendar_dlg *hcdlg = data;
	int year;
	if (sscanf((char*)key, "%4d", &year) < 1)
		return;

	if (hcdlg->firstyear == 0)
		hcdlg->firstyear = year;

	if (year < hcdlg->firstyear)
		hcdlg->firstyear = year;

	if (year > hcdlg->lastyear)
		hcdlg->lastyear = year;
}

static void
daycounts_hash_cb(gpointer key, gpointer value, gpointer data) {
	history_calendar_dlg *hcdlg = data;
	char *date = key;
	char *str = value;
	int year, month, day;

	if (str[0] == 0 || str[0] == '0') return; /* no entries this date? */

	if (sscanf(date, "%4d-%2d-%2d", &year, &month, &day) < 3)
		return;

	if (hcdlg->firstyear == 0) 
		hcdlg->firstyear = year;

	MARKMONTH(hcdlg, year, month) |= (1L << day);
}

static gboolean
getdaycounts_run(history_calendar_dlg* hcdlg) {
	NetRequest *request;
	NetResult  *result;
	
	request = XXX("getdaycounts");
	
	result = net_request_run(hcdlg->win, "Loading calendar...", request);
	net_request_free(request);

	if (!net_result_succeeded(result)) {
		net_result_free(result);
		gtk_widget_destroy(hcdlg->win);
		return FALSE;
	}

	/* we must iterate through the hash once to find the range of the returned years. */
	g_hash_table_foreach(result, daycounts_hash_extents_cb, hcdlg);

	if (hcdlg->markmonths) g_free(hcdlg->markmonths);
	hcdlg->markmonths = g_new0(guint32, 
			12 * (hcdlg->lastyear - hcdlg->firstyear + 1));

	/* then read all of the values from the hash. */
	g_hash_table_foreach(result, daycounts_hash_cb, hcdlg);

	net_result_free(result);
	return TRUE;
}

static void
change_day(history_calendar_dlg *hcdlg) {
	int count, i, itemid;
	char key[50];
	char *str, *event;
	int hour, minute;
	gint row;
	char *append[2];

	NetRequest *request;
	NetResult  *result;
	
	if (hcdlg->firstyear == 0) return; /* this can happen if they cancelled the getdaycounts */

	request = XXX("getevents");

	net_request_seti(request,  "truncate",      50);
	net_request_seti(request,  "prefersubject", 1);
	net_request_seti(request,  "noprops",       1);
	net_request_copys(request, "selecttype",    "day");
	net_request_copys(request, "lineendings",   "dots");

	net_request_seti(request, "year", hcdlg->year);
	net_request_seti(request, "month", hcdlg->mon);
	net_request_seti(request, "day", hcdlg->day);

	result = net_request_run(hcdlg->win, "Loading day calendar...", request);
	net_request_free(request);

	if (!net_result_succeeded(result)) {
		net_result_free(result);
		return;
	}

	/* it would have been better if net_result_geti used a GError */
	str = net_result_get(result, "events_count");
	if (str == NULL) return;
	count = atoi(str);

	if (count == 0) {
		/* FIXME? no events this day; whoops.  can happen if they delete. */
		MARKMONTH_CUR(hcdlg) &= ~(1L << hcdlg->day);
		gtk_clist_clear(GTK_CLIST(hcdlg->evlist));
		append[0] = "";
		append[1] = "[no events]";
		gtk_clist_append(GTK_CLIST(hcdlg->evlist), append);
		gtk_clist_set_selectable(GTK_CLIST(hcdlg->evlist), 0, FALSE);
		net_result_free(result);
		return;
	}

	gtk_clist_freeze(GTK_CLIST(hcdlg->evlist));
	gtk_clist_clear(GTK_CLIST(hcdlg->evlist));

	for (i = 1; i < count+1; i++) {
		sprintf(key, "events_%d_event", i);
		event = net_result_get(result, key);

		sprintf(key, "events_%d_eventtime", i);
		str = net_result_get(result, key);
		sscanf(str+11, "%2d:%2d", &hour, &minute);

		sprintf(key, "events_%d_itemid", i);
		str = net_result_get(result, key);
		if (str == NULL) continue;
		itemid = atoi(str);

		sprintf(key, "%02d:%02d", hour, minute);
		append[0] = key;
		append[1] = event;
		row = gtk_clist_append(GTK_CLIST(hcdlg->evlist), append);
		gtk_clist_set_row_data(GTK_CLIST(hcdlg->evlist), row, GINT_TO_POINTER(itemid));
	}
	gtk_clist_thaw(GTK_CLIST(hcdlg->evlist));
	gtk_clist_set_column_width(GTK_CLIST(hcdlg->evlist), 0, 
			gtk_clist_optimal_column_width(GTK_CLIST(hcdlg->evlist), 0));

	net_result_free(result);
}

static void 
hc_row_selected(GtkCList *list,
				gint row, gint col,
				GdkEventButton *event, 
				history_calendar_dlg *hcdlg)
{
	if (hcdlg->bbox)
		gtk_widget_set_sensitive(hcdlg->bbox, (list->selection != NULL));
}

static int
hc_current_itemid(history_calendar_dlg *hcdlg) {
	int row;
	int itemid;

	if (GTK_CLIST(hcdlg->evlist)->selection == NULL) return -1;
	row = (int)GTK_CLIST(hcdlg->evlist)->selection->data;
	itemid = (int)gtk_clist_get_row_data(GTK_CLIST(hcdlg->evlist), row);
	return itemid;
}

static void
hc_show_current(history_calendar_dlg *hcdlg) {
	int itemid = hc_current_itemid(hcdlg);
	if (itemid < 0)
		return;

	/* fixme hi_dialog_run(hcdlg->win, itemid); */
}

static gint
hc_list_click_idle_cb(gpointer d) {
#if defined (G_OS_WIN32)
	gdk_threads_enter();
#endif
	hc_show_current(d);
#if defined (G_OS_WIN32)
	gdk_threads_leave();
#endif
	return FALSE;
}
	
static gint
hc_list_click_cb(GtkCList *list, GdkEventButton *be, 
		history_calendar_dlg *hcdlg) 
{
	/* if they double click, show the history item window */
	if (be->button == 1 && be->type == GDK_2BUTTON_PRESS) {
		gtk_idle_add(hc_list_click_idle_cb, hcdlg);
	}
	return 0;
}

static void
hc_edit_cb(GtkWidget *w, history_calendar_dlg *hcdlg) {
	hc_show_current(hcdlg);
}

/*static void
history_delete_selected(history_item_dlg *hidlg) {
	GHashTable *request, *result;
	
	request = XXX("editevent");

	g_hash_table_insert(request, g_strdup("itemid"),
			g_strdup_printf("%d", hidlg->itemid));

	g_hash_table_insert(request, g_strdup("event"), g_strdup(""));

	result = net_request_run(hidlg->win, "Deleting event...", request);
			
	hash_destroy(request);

	if (net_request_succeeded(result)) {
		gtk_widget_destroy(hidlg->win);
	}
	hash_destroy(result);
}*/

/*static void
hc_web_cb(GtkWidget *w, history_calendar_dlg *hcdlg) {
	char *spawn;

	spawn = g_strdup_printf("%s/talkread.bml?itemid=%d", 
			c_cur_server()->url,
			hc_current_itemid(hcdlg));
	spawn_url(spawn);
	g_free(spawn);
}*/

fixme 
static void
hi_load_metadata(history_item_dlg *hidlg, GHashTable *result) {
	GHashTable *metadata;
	int metacount, i;
	char key[50];

	metacount = atoi(g_hash_table_lookup(result, "prop_count"));

	if (metacount < 1)
		return;

	metadata = g_hash_table_new(g_str_hash, g_str_equal);

	for (i = 1; i < metacount + 1; i++) { /* sequence is 1-based! */
		char *name, *value;

		sprintf(key, "prop_%d_name", i);
		name = g_hash_table_lookup(result, key);
		sprintf(key, "prop_%d_value", i);
		value = g_hash_table_lookup(result, key);
		g_hash_table_insert(metadata, 
				g_strdup_printf("prop_%s", name), 
				g_strdup(value));
	}
	metamgr_load_from_request(METAMGR(hidlg->metamgr), metadata);
	hash_destroy(metadata);
}

static void
hi_load_security(history_item_dlg *hidlg, GHashTable *result) {
	char *sectext, *allowmask;

	sectext = g_hash_table_lookup(result, "events_1_security");
	if (sectext == NULL)
		return;

	allowmask = g_hash_table_lookup(result, "events_1_allowmask");
	security_load(&hidlg->security, sectext, allowmask);
}

void 
int_to_entry(GtkWidget *entry, int val) {
	char buf[100];
	sprintf(buf, "%02d", val);
	gtk_entry_set_text(GTK_ENTRY(entry), buf);
}

static void
hi_load_datetime(history_item_dlg *hidlg, GHashTable *result) {
	char *timestr;
	int year, mon, day, hour, min, sec;

	timestr = g_hash_table_lookup(result, "events_1_eventtime");
	sscanf(timestr, "%4d-%2d-%2d %2d:%2d:%2d",
		&year, &mon, &day, &hour, &min, &sec);
	int_to_entry(hidlg->eyear, year);
	int_to_entry(hidlg->emon,  mon);
	int_to_entry(hidlg->eday,  day);
	int_to_entry(hidlg->ehour, hour);
	int_to_entry(hidlg->emin,  min);
	/* int_to_entry(hidlg->esec,  sec); */
}

static GHashTable*
hi_request_run(history_item_dlg *hidlg, int itemid) {
	GHashTable *request, *result;

	request = XXX("getevents");

	g_hash_table_insert(request, g_strdup("selecttype"), g_strdup("one"));
	g_hash_table_insert(request, g_strdup("lineendings"), g_strdup("unix"));
	g_hash_table_insert(request, g_strdup("itemid"), g_strdup_printf("%d", itemid));

	result = net_request_run(hidlg->win, "Loading event...", request);
	net_request_destroy(request);

	if (!net_request_succeeded(result)) {
		net_result_free(result);
		return NULL;
	}
	return result;
}

static gboolean
hi_load_result(history_item_dlg *hidlg, GHashTable* result) {
	gint insert_point;
	char *value;

	value = g_hash_table_lookup(result, "events_count");
	if (value == NULL || strcmp(value, "1") != 0) {
		jam_messagebox(hidlg->win, "Error Loading Event",
				"Event not found.");
		net_result_free(result);
		return FALSE;
	}

	value = g_hash_table_lookup(result, "events_1_event");
	gtk_editable_delete_text(GTK_EDITABLE(hidlg->eentry), 0, -1);
	insert_point = 0;
	gtk_editable_insert_text(GTK_EDITABLE(hidlg->eentry), 
			value, strlen(value), &insert_point);
	/* fixme gtkspell gtkspell_check_all(GTK_TEXT(hidlg->eentry)); */
	gtk_editable_set_position(GTK_EDITABLE(hidlg->eentry), 0);

	/* subject */
	value = g_hash_table_lookup(result, "events_1_subject");
	if (value) 
		gtk_entry_set_text(GTK_ENTRY(hidlg->esubject), value);

	/* we grab the "official" itemid, too, to support "latest entry". */
	value = g_hash_table_lookup(result, "events_1_itemid");
	if (value) 
		hidlg->itemid = atoi(value);

	hi_load_datetime(hidlg, result);
	hi_load_security(hidlg, result);
	hi_load_metadata(hidlg, result);
	net_result_free(result);
	return TRUE;
}

static void
hi_save_cb(GtkWidget *w, history_item_dlg *hidlg) {
	GHashTable *request, *result;

	request = XXX("editevent");

	g_hash_table_insert(request, g_strdup("itemid"),
			g_strdup_printf("%d", hidlg->itemid));
	g_hash_table_insert(request, g_strdup("year"), 
			gtk_editable_get_chars(GTK_EDITABLE(hidlg->eyear), 0, -1));
	g_hash_table_insert(request, g_strdup("mon"),  
			gtk_editable_get_chars(GTK_EDITABLE(hidlg->emon), 0, -1));
	g_hash_table_insert(request, g_strdup("day"),  
			gtk_editable_get_chars(GTK_EDITABLE(hidlg->eday), 0, -1));
	g_hash_table_insert(request, g_strdup("hour"), 
			gtk_editable_get_chars(GTK_EDITABLE(hidlg->ehour), 0, -1));
	g_hash_table_insert(request, g_strdup("min"),  
			gtk_editable_get_chars(GTK_EDITABLE(hidlg->emin), 0, -1));

	g_hash_table_insert(request, g_strdup("subject"), 
			gtk_editable_get_chars(GTK_EDITABLE(hidlg->esubject), 0, -1));
	g_hash_table_insert(request, g_strdup("event"), 
			gtk_editable_get_chars(GTK_EDITABLE(hidlg->eentry), 0, -1));
	metamgr_append_to_request(METAMGR(hidlg->metamgr), request);
	security_append_to_request(&hidlg->security, request);

	result = net_request_run(hidlg->win, "Saving Item...", request);
	net_request_free(request);

	net_result_free(result);
}

static void
hi_delete_cb(GtkWidget *w, history_item_dlg *hidlg) {
	if (conf.options.confirmdelete)
		if (!jam_confirm(hidlg->win, "Delete?", "Delete this entry?"))
			return;

	history_delete_selected(hidlg);
}

static GtkWidget*
hi_dialog_date_hbox(history_item_dlg *hidlg) {
	GtkWidget *hbox;
	GtkWidget *label;
	int twocharwidth;

	hbox = gtk_hbox_new(FALSE, 0); {
		/* year */
		hidlg->eyear = gtk_entry_new_with_max_length(4);
		/* fixme gtk2 twocharwidth = gdk_string_width(hidlg->eyear->style->font, "00");*/
		twocharwidth = 20;
		gtk_box_pack_start(GTK_BOX(hbox), hidlg->eyear, FALSE, FALSE, 0);
		gtk_widget_set_usize(GTK_WIDGET(hidlg->eyear), twocharwidth*2 + 5, -1);
		/* dash */ 
		label = gtk_label_new("-"); gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
		/* month */
		hidlg->emon = gtk_entry_new_with_max_length(2);
		gtk_box_pack_start(GTK_BOX(hbox), hidlg->emon, FALSE, FALSE, 0);
		gtk_widget_set_usize(GTK_WIDGET(hidlg->emon), twocharwidth + 5, -1);
		/* dash */ 
		label = gtk_label_new("-"); gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
		/* day */
		hidlg->eday = gtk_entry_new_with_max_length(2);
		gtk_box_pack_start(GTK_BOX(hbox), hidlg->eday, FALSE, FALSE, 0);
		gtk_widget_set_usize(GTK_WIDGET(hidlg->eday), twocharwidth + 5, -1);
	}
	return hbox;
}

static GtkWidget*
hi_dialog_time_hbox(history_item_dlg *hidlg) {
	GtkWidget *hbox;
	GtkWidget *label;
	int twocharwidth;
	
	hbox = gtk_hbox_new(FALSE, 0); {
		/* hour */
		hidlg->ehour = gtk_entry_new_with_max_length(2);
		twocharwidth = gdk_string_width(hidlg->ehour->style->font, "00");
		gtk_box_pack_start(GTK_BOX(hbox), hidlg->ehour, FALSE, FALSE, 0);
		gtk_widget_set_usize(GTK_WIDGET(hidlg->ehour), twocharwidth + 5, -1);
		/* colon */ 
		label = gtk_label_new(":"); gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
		/* min */
		hidlg->emin = gtk_entry_new_with_max_length(2);
		gtk_box_pack_start(GTK_BOX(hbox), hidlg->emin, FALSE, FALSE, 0);
		gtk_widget_set_usize(GTK_WIDGET(hidlg->emin), twocharwidth + 5, -1);
	}
	return hbox;
}

static gint
hi_dialog_run(GtkWidget *parent, int itemid) {
	history_item_dlg hidlg_actual = { 0 };
	history_item_dlg *hidlg = &hidlg_actual;
	GtkWidget *vbox, *button;
	NetResult *result;

	hidlg->itemid = itemid;
	result = hi_request_run(hidlg, itemid);
	if (result == NULL) 
		return 0;

	hidlg->win = jam_dialog_new(parent, "History Item", 300, 300);
	geometry_tie(hidlg->win, GEOM_HISTORY_ITEM);
	g_signal_connect(G_OBJECT(hidlg->win), "destroy",
			G_CALLBACK(gtk_main_quit), NULL);

	vbox = gtk_vbox_new(FALSE, 5); {
		GtkWidget *hbox, *scroll;

		hbox = gtk_hbox_new(FALSE, 5); {
			gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Subject:"), 
					FALSE, FALSE, 0);
			hidlg->esubject = gtk_entry_new();

			gtk_widget_set_usize(hidlg->esubject, 100, -1); 
			/* usize is just the minimum size... */
			gtk_box_pack_start(GTK_BOX(hbox), hidlg->esubject, TRUE, TRUE, 0);

		}
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, 5); {
			gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Date:"), 
					FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(hbox), hi_dialog_date_hbox(hidlg),
					FALSE, FALSE, 0);

			gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Time:"), 
					FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(hbox), hi_dialog_time_hbox(hidlg),
					FALSE, FALSE, 0);
			
			hidlg->security.type = conf.defaultsecurity;
			hidlg->secmgr = secmgr_new(&hidlg->security);
			gtk_box_pack_end(GTK_BOX(hbox), hidlg->secmgr, FALSE, FALSE, 0);
			gtk_box_pack_end(GTK_BOX(hbox), gtk_label_new("Security:"), 
					FALSE, FALSE, 0);
		}
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		scroll = gtk_scrolled_window_new(NULL, NULL); {
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

			hidlg->eentry = gtk_text_new(NULL, NULL);
			/* fixme gtkspell gtkspell_attach(GTK_TEXT(hidlg->eentry)); */
			gtk_text_set_editable(GTK_TEXT(hidlg->eentry), TRUE);
			gtk_text_set_word_wrap(GTK_TEXT(hidlg->eentry), TRUE);
			gtk_container_add(GTK_CONTAINER(scroll), hidlg->eentry);
		}
		gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

		hidlg->metamgr = metamgr_new(GTK_BOX(vbox));
		gtk_box_pack_end(GTK_BOX(vbox), hidlg->metamgr, FALSE, FALSE, 0);
	}
	
	jam_dialog_set_contents(hidlg->win, vbox);

	button = gtk_button_new_with_label("  Save Changes  ");
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(hi_save_cb), (gpointer) hidlg);
	jam_dialog_add_button(hidlg->win, button);

	button = gtk_button_new_with_label("  Delete  ");
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(hi_delete_cb), (gpointer) hidlg);
	jam_dialog_add_button(hidlg->win, button);
	
	jam_dialog_add_close(hidlg->win);

	hi_load_result(hidlg, result);

	gtk_widget_realize(hidlg->win);
	gtk_widget_realize(hidlg->secmgr);
	gtk_widget_show(hidlg->win);

	gtk_main();

	return 0;
}
#endif

LJEntry*
history_load_itemid(GtkWindow *parent, JamAccount *acc, const char *usejournal, int itemid) {
	NetContext *ctx;
	LJGetEventsSingle *getevents;
	LJEntry *entry;

	if (!JAM_ACCOUNT_IS_LJ(acc)) {
		g_warning("XXX blogger: history for blogger\n");
		return NULL;
	}

	getevents = lj_getevents_single_new(jam_account_lj_get_user(JAM_ACCOUNT_LJ(acc)), usejournal, itemid);
	ctx = net_ctx_gtk_new(parent, _("Loading Entry"));
	if (!net_run_verb_ctx((LJVerb*)getevents, ctx, NULL)) {
		lj_getevents_single_free(getevents, TRUE);
		net_ctx_gtk_free(ctx);
		return NULL;
	}
	entry = getevents->entry;
	lj_getevents_single_free(getevents, FALSE);
	net_ctx_gtk_free(ctx);

	return entry;
}

LJEntry*
history_load_latest(GtkWindow *win, JamAccount *acc, const char *usejournal) {
	return history_load_itemid(win, acc, usejournal, -1);
}

