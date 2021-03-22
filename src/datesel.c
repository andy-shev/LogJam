/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <stdlib.h>
#include <string.h>
#include <livejournal/livejournal.h>

#include "datesel.h"
#include "util-gtk.h"

struct _DateSel {
	GtkButton parent;
	GtkWidget *label;

	guint timeout_id;

	struct tm date;
	gboolean backdated;
};

static void datesel_init(DateSel *ds);
static void datesel_class_init(GtkButtonClass *class);
static void datesel_dialog_run(DateSel *ds);

static void timeout_enable(DateSel *ds, gboolean usenow);

/* gtk stuff */
GObjectClass *parent_class = NULL;

GType
datesel_get_type(void) {
	static GType ds_type = 0;
	if (!ds_type) {
		static const GTypeInfo ds_info = {
			sizeof(GtkButtonClass),
			NULL,
			NULL,
			(GClassInitFunc)datesel_class_init,
			NULL,
			NULL,
			sizeof(DateSel),
			0,
			(GInstanceInitFunc) datesel_init,
		};
		ds_type = g_type_register_static(GTK_TYPE_BUTTON, "DateSel",
				&ds_info, 0);
	}
	return ds_type;
}

static void
datesel_finalize(GObject *obj) {
	timeout_enable(DATESEL(obj), FALSE);
	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
datesel_class_init(GtkButtonClass *class) {
	GObjectClass *oclass = G_OBJECT_CLASS(class);
	parent_class = g_type_class_peek_parent(class);
	oclass->finalize = datesel_finalize;
}

static void
update_display(DateSel *ds) {
	char *ljdate = lj_tm_to_ljdate_noseconds(&ds->date);
	gtk_label_set_text(GTK_LABEL(ds->label), ljdate);
	g_free(ljdate);
}

static gboolean
timeout_cb(DateSel *ds) {
	time_t curtime_time_t = time(NULL);
	struct tm *curtime = localtime(&curtime_time_t);
	ds->date = *curtime;
	update_display(ds);
	return TRUE;
}

static void
timeout_enable(DateSel *ds, gboolean usenow) {
	if (usenow && !ds->timeout_id) {
		timeout_cb(ds);
		ds->timeout_id = g_timeout_add(10*1000, /* every 10 sec. */
				(GSourceFunc)timeout_cb, ds);
	} else if (!usenow && ds->timeout_id) {
		g_source_remove(ds->timeout_id);
		ds->timeout_id = 0;
	}
}

static void
datesel_init(DateSel *ds) {
	ds->label = gtk_label_new("meow");
	gtk_container_add(GTK_CONTAINER(ds), ds->label);
	gtk_button_set_relief(GTK_BUTTON(ds), GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(ds), "clicked",
			G_CALLBACK(datesel_dialog_run), NULL);

	ds->timeout_id = 0;
	timeout_enable(ds, TRUE);
}

GtkWidget*
datesel_new(void) {
	DateSel *ds = DATESEL(g_object_new(datesel_get_type(), NULL));

	return GTK_WIDGET(ds);
}

void
datesel_get_tm(DateSel *ds, struct tm *ptm) {
	if (ds->timeout_id)
		memset(ptm, 0, sizeof(struct tm));
	else
		*ptm = ds->date;
}

void
datesel_set_tm(DateSel *ds, struct tm *ptm) {
	if (ptm && ptm->tm_year) {
		ds->date = *ptm;
		timeout_enable(ds, FALSE);
	} else {
		timeout_enable(ds, TRUE);
	}
	update_display(ds);
}

gboolean
datesel_get_backdated(DateSel *ds) {
	return ds->backdated;
}

void
datesel_set_backdated(DateSel *ds, gboolean backdated) {
	ds->backdated = backdated;
}

static void
usenow_cb(GtkToggleButton *cb, GtkWidget *box) {
	if (gtk_toggle_button_get_active(cb))
		gtk_widget_hide(box);
	else
		gtk_widget_show(box);
}

static void
backdated_cb(GtkToggleButton *cb, DateSel *ds) {
	ds->backdated = gtk_toggle_button_get_active(cb);
}

static gboolean
minute_output_cb(GtkSpinButton *s) {
	char buf[5];
	g_snprintf(buf, 5, "%02d", (int)gtk_spin_button_get_value(s));
	if (strcmp(buf, gtk_entry_get_text(GTK_ENTRY(s))) != 0)
		gtk_entry_set_text(GTK_ENTRY(s), buf);
	return TRUE;
}

static void
datesel_dialog_run(DateSel *ds) {
	GtkWidget *dlg, *vbox;
	GtkWidget *datebox, *cal;
	GtkAdjustment *houradj, *minadj;
	GtkWidget *hbox, *label, *hourspin, *minspin;
	GtkWidget *check, *backdated;
	guint year, month, day;

	dlg = gtk_dialog_new_with_buttons(_("Select Date"),
			GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(ds))),
			GTK_DIALOG_MODAL,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);

	datebox = gtk_vbox_new(FALSE, 5);
	cal = gtk_calendar_new();
	gtk_calendar_select_month(GTK_CALENDAR(cal), ds->date.tm_mon, ds->date.tm_year+1900);
	gtk_calendar_select_day(GTK_CALENDAR(cal), ds->date.tm_mday);
	gtk_box_pack_start(GTK_BOX(datebox), cal, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new_with_mnemonic(_("_Time:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	houradj = (GtkAdjustment*)gtk_adjustment_new(ds->date.tm_hour, 0, 23, 1, 4, 0);
	hourspin = gtk_spin_button_new(houradj, 1.0, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), hourspin);

	minadj = (GtkAdjustment*)gtk_adjustment_new(ds->date.tm_min, 0, 59, 1, 10, 0);
	minspin = gtk_spin_button_new(minadj, 1.0, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(minspin), TRUE);
	g_signal_connect(G_OBJECT(minspin), "output",
			G_CALLBACK(minute_output_cb), NULL);

	if (gtk_widget_get_direction(hbox) == GTK_TEXT_DIR_RTL) {
		/* even though we're right-to-left, hours still should
		 * be on the left of minutes. */
		gtk_box_pack_start(GTK_BOX(hbox), minspin, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(":"),
				FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), hourspin, FALSE, FALSE, 0);
	} else {
		gtk_box_pack_start(GTK_BOX(hbox), hourspin, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(":"),
				FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), minspin, FALSE, FALSE, 0);
	}

	gtk_box_pack_start(GTK_BOX(datebox), hbox, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_mnemonic(_("Use _current date/time"));
	g_signal_connect(G_OBJECT(check), "toggled",
			G_CALLBACK(usenow_cb), datebox);

	backdated = gtk_check_button_new_with_mnemonic(_("Entry is _backdated"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(backdated), ds->backdated);
	g_signal_connect(G_OBJECT(backdated), "toggled",
			G_CALLBACK(backdated_cb), ds);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), datebox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), backdated, FALSE, FALSE, 0);

	jam_dialog_set_contents(GTK_DIALOG(dlg), vbox);

	/* set it active here so it can hide the contents if necessary. */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), ds->timeout_id != 0);

	gtk_dialog_run(GTK_DIALOG(dlg));

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check))) {
		timeout_enable(ds, TRUE);
	} else {
		timeout_enable(ds, FALSE);
		gtk_calendar_get_date(GTK_CALENDAR(cal), &year, &month, &day);
		ds->date.tm_year = year-1900;
		ds->date.tm_mon = month;
		ds->date.tm_mday = day;
		ds->date.tm_hour = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(hourspin));
		ds->date.tm_min = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(minspin));
	}
	update_display(ds);
	gtk_widget_destroy(dlg);
}
