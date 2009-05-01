/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "gtk-all.h"

typedef struct {
	GtkWidget *win;
	guint *date;
	guint *marks;
	guint markfirstyear;
	gboolean closehack;
} calendar_data;

static void
popcalendar_set_marks(GtkCalendar *cal, guint32 marks) {
	int day;

	gtk_calendar_freeze(cal);
	gtk_calendar_clear_marks(cal);
	for (day = 0; day < 31; day++) {
		if (marks & (1L << day)) {
			gtk_calendar_mark_day(cal, day);
		}
	}
	gtk_calendar_thaw(cal);
}

static void 
day_selected(GtkWidget *cal, calendar_data *cdata) {
	/* don't close it when the day change was caused by a month change! */
	if (!cdata->closehack) {
		guint *date = cdata->date;
		gtk_calendar_get_date(GTK_CALENDAR(cal), &date[0], &date[1], &date[2]);
		date[1]++; /* month was zero-based. */
		gtk_widget_destroy(cdata->win);
	}
	cdata->closehack = FALSE;
}
static void 
month_changed(GtkWidget *cal, calendar_data *cdata) {
	guint year, mon, day;

	cdata->closehack = TRUE;
	gtk_calendar_get_date(GTK_CALENDAR(cal), &year, &mon, &day);
	if (year < cdata->markfirstyear)
		popcalendar_set_marks(GTK_CALENDAR(cal), 0);
	else 
		popcalendar_set_marks(GTK_CALENDAR(cal), 
				cdata->marks[(year-cdata->markfirstyear)*12 + mon]);
}

void 
popcalendar_run(GtkWidget *parent, guint date[], int markfirstyear, guint32 marks[]) {
	GtkWidget *win;
	GtkWidget *frame, *box;
	GtkWidget *cal;
	calendar_data cdata_actual = {0}, *cdata = &cdata_actual;

	win = gtk_window_new(GTK_WINDOW_POPUP);
	cdata->win = win;
	cdata->date = date;
	cdata->marks = marks;
	cdata->markfirstyear = markfirstyear;
	gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(parent));
	gtk_window_set_modal(GTK_WINDOW(win), TRUE);
	gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_MOUSE);
	g_signal_connect(G_OBJECT(win), "destroy",
			G_CALLBACK(gtk_main_quit), NULL);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);

	/*hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Select day:"),
			FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), gtk_button_new_with_label("x"),
			FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);*/

	cal = gtk_calendar_new();
	if (date[0] != 0) {
		gtk_calendar_select_month(GTK_CALENDAR(cal), date[1]-1, date[0]);
		gtk_calendar_select_day(GTK_CALENDAR(cal), date[2]);
	} else {
		gtk_calendar_get_date(GTK_CALENDAR(cal), &date[0], &date[1], &date[2]);
	}
	g_signal_connect(G_OBJECT(cal), "day-selected",
			G_CALLBACK(day_selected), cdata);
	g_signal_connect(G_OBJECT(cal), "month-changed",
			G_CALLBACK(month_changed), cdata);
	gtk_calendar_display_options(GTK_CALENDAR(cal), 
			GTK_CALENDAR_SHOW_HEADING |
			GTK_CALENDAR_SHOW_DAY_NAMES);
	popcalendar_set_marks(GTK_CALENDAR(cal), 
			marks[(date[0]-markfirstyear)*12 + date[1]-1]);

	gtk_box_pack_start(GTK_BOX(box), cal, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_container_add(GTK_CONTAINER(win), frame);

	gtk_widget_show_all(frame);
	gtk_widget_show(win);

	gtk_main();
}

