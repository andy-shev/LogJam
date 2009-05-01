/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"
#include "gtk-all.h"

#include "util.h"
#include "throbber.h"
#include "progress.h"

struct _ProgressWindow {
	GtkWindow win;
	GtkWidget *parent, *box, *clientbox;
	GtkWidget *titlelabel, *label, *progress;
	gboolean showing_error;
	GtkWidget *throbber;

	ProgressWindowCancelFunc cancel_cb;
	gpointer cancel_cb_data;
};

void 
progress_window_show_error(ProgressWindow *pw, const char *fmt, ...) {
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	g_vsnprintf(buf, 1024, fmt, ap);
	va_end(ap);

	if (pw) {
		progress_window_set_title(pw, _("Error"));
		gtk_label_set_text(GTK_LABEL(pw->label), buf);

		throbber_stop(THROBBER(pw->throbber));
		gtk_image_set_from_stock(GTK_IMAGE(pw->throbber),
				GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
		gtk_widget_hide(pw->progress);
		if (pw->clientbox) gtk_widget_hide(pw->clientbox);
		pw->showing_error = TRUE;
		gtk_main();
	} else {
		g_print(_("Error: %s\n"), buf);
	}
}

static void 
cancel_cb(ProgressWindow *pw) {
	if (pw->showing_error)
		gtk_main_quit();
	else if (pw->cancel_cb)
		pw->cancel_cb(pw->cancel_cb_data);
}

static void
destroy_cb(GtkWidget *w, ProgressWindow *pw) {
	throbber_stop(THROBBER(pw->throbber));
	if (pw->parent)
		g_signal_handlers_disconnect_matched(pw->parent,
				G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, pw);
}

static gboolean
delete_event_cb(GtkWidget *w, GdkEvent *e, ProgressWindow *pw) {
	cancel_cb(pw);
	return TRUE; /* don't destroy us. */
}

static void
reposition(ProgressWindow *pw, gint w, gint h) {
	gint ox, oy, px, py, x, y;

	if (!pw->parent)
		return;
	gdk_window_get_origin(pw->parent->window, &px, &py);
	if (w == -1)
		w = GTK_WIDGET(pw)->allocation.width;
	if (h == -1)
		h = GTK_WIDGET(pw)->allocation.height;
	x = px + (pw->parent->allocation.width  - w) / 2;
	y = py + (pw->parent->allocation.height - h) / 2;

	gdk_window_get_origin(GTK_WIDGET(pw)->window, &ox, &oy);
	if (x != ox || y != oy) {
		gtk_window_move(GTK_WINDOW(pw), x, y);
	}
}

static gboolean
parent_configure_cb(GtkWidget *w, GdkEvent *e, ProgressWindow *pw) {	 
	reposition(pw, -1, -1);
	return FALSE;	 
}

static gboolean
configure_cb(GtkWidget *w, GdkEventConfigure *e, ProgressWindow *pw) {	 
	if (GTK_WIDGET(pw)->allocation.width != e->width ||
		GTK_WIDGET(pw)->allocation.height != e->height)
		reposition(pw, e->width, e->height);
	return FALSE;	 
}	 


static void
progress_window_set_parent(ProgressWindow *pw, GtkWindow *parent) {
	pw->parent = GTK_WIDGET(parent);
	gtk_window_set_transient_for(GTK_WINDOW(pw), parent);
	g_signal_connect(G_OBJECT(parent), "configure-event",
			G_CALLBACK(parent_configure_cb), pw);
}

static void 
progress_window_init(ProgressWindow *pw) {
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox, *align, *bbox, *button, *lbox;

	gtk_window_set_decorated(GTK_WINDOW(pw), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(pw), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_modal(GTK_WINDOW(pw), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(pw), 250, -1);
	g_signal_connect(G_OBJECT(pw), "destroy", G_CALLBACK(destroy_cb), pw);
	g_signal_connect(G_OBJECT(pw), "delete-event",
			G_CALLBACK(delete_event_cb), pw);
	gtk_window_set_position(GTK_WINDOW(pw), GTK_WIN_POS_CENTER_ON_PARENT);
	g_signal_connect(G_OBJECT(pw), "configure-event",
			G_CALLBACK(configure_cb), pw);

	vbox = gtk_vbox_new(FALSE, 6); 
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

	hbox = gtk_hbox_new(FALSE, 6); 

	pw->throbber = throbber_new();
	align = gtk_alignment_new(0.5, 0, 0, 0);
	gtk_container_add(GTK_CONTAINER(align), pw->throbber);
	gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);
	throbber_start(THROBBER(pw->throbber));

	pw->box = lbox = gtk_vbox_new(FALSE, 6);
	pw->titlelabel = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(lbox), pw->titlelabel, TRUE, TRUE, 0);

	pw->label = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(pw->label), TRUE);
	gtk_box_pack_start(GTK_BOX(lbox), pw->label, TRUE, TRUE, 0);

	pw->progress = gtk_progress_bar_new();
	gtk_box_pack_end(GTK_BOX(lbox), pw->progress, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), lbox, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
			G_CALLBACK(cancel_cb), pw);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	frame = gtk_frame_new(NULL);	 
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);	 
	gtk_container_add(GTK_CONTAINER(frame), vbox);	 
	gtk_widget_show_all(frame);	 
	gtk_widget_hide(pw->progress);
	gtk_container_add(GTK_CONTAINER(pw), frame);

	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);
}

void
progress_window_pack(ProgressWindow *pw, GtkWidget *contents) {
	if (!pw->clientbox) {
		pw->clientbox = gtk_vbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(pw->box), pw->clientbox, TRUE, TRUE, 0);
		gtk_widget_show(pw->clientbox);
	}

	gtk_box_pack_start(GTK_BOX(pw->clientbox), contents, FALSE, FALSE, 0);
}

void
progress_window_set_title(ProgressWindow *pw, const char *title) {
	char *t;
	gtk_window_set_title(GTK_WINDOW(pw), title);
	t = title ? g_strdup_printf("<b>%s</b>", title) : g_strdup("");
	gtk_label_set_markup(GTK_LABEL(pw->titlelabel), t);
	g_free(t);
}

void
progress_window_set_text(ProgressWindow *pw, const char *text) {
	gtk_label_set_text(GTK_LABEL(pw->label), text);
}

void
progress_window_set_progress(ProgressWindow *pw, float frac) {
	if (frac >= 0)
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pw->progress), frac);
	
	if (frac > 0) {
		gtk_widget_show(pw->progress);
	}/* else {
		gtk_widget_hide(pw->progress);
	}*/
}

void progress_window_set_cancel_cb(ProgressWindow *pw,
		ProgressWindowCancelFunc func, gpointer data) {
	pw->cancel_cb = func;
	pw->cancel_cb_data = data;
}

GtkWidget* 
progress_window_new(GtkWindow *parent, const char *title) {
	ProgressWindow *pw = PROGRESS_WINDOW(g_object_new(progress_window_get_type(), NULL));
	if (parent)
		progress_window_set_parent(pw, parent);
	progress_window_set_title(pw, title);
	return GTK_WIDGET(pw);
}

GType
progress_window_get_type(void) {
	static GType pw_type = 0;
	if (!pw_type) {
		static const GTypeInfo pw_info = {
			sizeof (GtkWindowClass),
			NULL,
			NULL,
			NULL, /*(GClassInitFunc) progress_window_class_init,*/
			NULL,
			NULL,
			sizeof (ProgressWindow),
			0,
			(GInstanceInitFunc) progress_window_init,
		};
		pw_type = g_type_register_static(GTK_TYPE_WINDOW, "ProgressWindow",
				&pw_info, 0);
	}
	return pw_type;
}

