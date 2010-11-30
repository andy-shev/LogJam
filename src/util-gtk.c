/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <livejournal/livejournal.h>  /* lj_md5_hash */

#include "util-gtk.h"

gchar*
gettext_translate_func(const gchar *path, gpointer data) {
	/* gettext returns a const, but gtk wants nonconst.  *shrug*. */
	return (gchar*)gettext(path);
}

GtkWidget* jam_table_new(int rows, int cols) {
	GtkWidget *table;
	table = gtk_table_new(rows, cols, FALSE);
	return table;
}
GtkWidget* jam_table_label(GtkTable *table, int row, const char *text) {
	GtkWidget *label = gtk_label_new_with_mnemonic(text);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
	gtk_table_attach(table, GTK_WIDGET(label),
			0, 1, row, row+1, GTK_FILL, GTK_FILL, 6, 6);
	return label;
}
void jam_table_content(GtkTable *table, int row, GtkWidget *content) {
	gtk_table_attach(table, GTK_WIDGET(content),
			1, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
}
void jam_table_label_content_mne(GtkTable *table, int row, const char *text, GtkWidget *content, GtkWidget *mne) {
	GtkWidget *label;
	label = jam_table_label(table, row, text);
	jam_table_content(table, row, content);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), mne);
}
void jam_table_label_content(GtkTable *table, int row, const char *text, GtkWidget *content) {
	jam_table_label_content_mne(table, row, text, content, content);
}
void jam_table_fillrow(GtkTable *table, int row, GtkWidget *content) {
	gtk_table_attach(table, GTK_WIDGET(content),
			0, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 2, 2);
}

void
jam_spin_button_set(GtkSpinButton *w,
		gboolean numeric,
		gdouble min, gdouble max,
		gdouble step, gdouble page,
		gint digits) {
	g_assert(GTK_IS_SPIN_BUTTON(w));

	gtk_spin_button_set_numeric(w, numeric);
	gtk_spin_button_set_range(w, min, max);
	gtk_spin_button_set_increments(w, step, page);
	gtk_spin_button_set_digits(w, digits);
}

void
jam_win_set_size(GtkWindow *win, int width, int height) {
	if (width > 0) {
		if (height > 0) {
			gtk_window_set_default_size(win, width, height);
		} else {
			gtk_window_set_default_size(win, width, (int)(0.618*width));
		}
	} else if (height > 0) {
		gtk_window_set_default_size(win, (int)(1.618*height), height);
	}
}

void
jam_window_init(GtkWindow *win, GtkWindow *parent, const gchar *title, int width, int height) {
	if (parent)
		gtk_window_set_transient_for(GTK_WINDOW(win), parent);
	if (title)
		gtk_window_set_title(GTK_WINDOW(win), title);
	jam_win_set_size(GTK_WINDOW(win), width, height);
}

static void
jam_dialog_set_contents_container(GtkDialog *dlg, GtkWidget *container) {
	gtk_container_set_border_width(GTK_CONTAINER(container), 12);
	gtk_box_pack_start(GTK_BOX(dlg->vbox), container, TRUE, TRUE, 0);
	gtk_widget_show_all(dlg->vbox);
	if (GTK_IS_NOTEBOOK(container))
		gtk_dialog_set_has_separator(dlg, FALSE);
}

GtkWidget*
jam_dialog_set_contents(GtkDialog *dlg, GtkWidget *contents) {
	GtkWidget *vbox;
	if (GTK_IS_CONTAINER(contents)) {
		jam_dialog_set_contents_container(dlg, contents);
		return contents;
	}
	vbox = gtk_vbox_new(FALSE, 5);
	if (contents)
		gtk_box_pack_start(GTK_BOX(vbox), contents, TRUE, TRUE, 0);
	jam_dialog_set_contents_container(dlg, vbox);
	return vbox;
}

GtkWidget*
jam_dialog_buttonbox_new(void) {
	return gtk_hbox_new(FALSE, 5);
}
void
jam_dialog_buttonbox_add(GtkWidget *box, GtkWidget *button) {
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
}
GtkWidget*
jam_dialog_buttonbox_button_with_label(GtkWidget *box, const char *label) {
	GtkWidget *button;
	char buf[100];
	g_snprintf(buf, 100, "  %s  ", label);
	button = gtk_button_new_with_mnemonic(buf);
	jam_dialog_buttonbox_add(box, button);
	return button;
}
GtkWidget*
jam_dialog_buttonbox_button_from_stock(GtkWidget *box, const char *id) {
	GtkWidget *button = gtk_button_new_from_stock(id);
	jam_dialog_buttonbox_add(box, button);
	return button;
}

void
jam_dialog_set_contents_buttonbox(GtkWidget *dlg, GtkWidget *contents, GtkWidget *buttonbox) {
	GtkWidget *vbox, *hbox;;

	vbox = jam_dialog_set_contents(GTK_DIALOG(dlg), contents);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), buttonbox, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);
}

int
jam_confirm(GtkWindow *parent, const char *title, const char *msg) {
	GtkWidget *dlg;
	int res;

	dlg = gtk_message_dialog_new(GTK_WINDOW(parent), 0,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_YES_NO,
			msg);
	jam_window_init(GTK_WINDOW(dlg), parent, title, -1, -1);
	res = (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES);
	gtk_widget_destroy(dlg);
	return res;
}

void
gdkcolor_to_hex(GdkColor *color, char* buf) {
	g_snprintf(buf, 8, "#%02X%02X%02X",
			color->red >> 8,
			color->green >> 8,
			color->blue >> 8);
}

GtkWidget*
scroll_wrap(GtkWidget *w) {
	GtkWidget *scroll;

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll),
			GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(scroll), w);
	return scroll;
}

static void
geometry_save(Geometry *geom, GtkWindow *win, GtkPaned *paned) {
	if (win && GTK_WIDGET(win)->window) {
		gtk_window_get_position(win, &geom->x, &geom->y);
		gtk_window_get_size(win, &geom->width, &geom->height);
	}
	if (paned) {
		geom->panedpos = gtk_paned_get_position(paned);
	}
}

static void
geometry_load(Geometry *geom, GtkWindow *win, GtkPaned *paned) {
	if (win && geom->width > 0) {
		gtk_window_move(win, geom->x, geom->y);
		gtk_window_set_default_size(win, geom->width, geom->height);
	}
	if (paned && geom->panedpos > 0) {
		gtk_paned_set_position(paned, geom->panedpos);
	}
}
static gboolean
geometry_win_event(GtkWindow *win, GdkEvent *e, Geometry *geom) {
#ifdef G_OS_WIN32
	/* geometry is totally broken on win32--
	 * gtk_window_get_position() returns random values. */
#else
	geometry_save(geom, win, NULL);
#endif
	return FALSE;
}
static void
geometry_show_cb(GtkWindow *win, Geometry *geom) {
	geometry_load(geom, win, NULL);
}
static gboolean
geometry_paned_event(GtkPaned *paned, gpointer d, Geometry *geom) {
	geometry_save(geom, NULL, paned);
	return FALSE;
}
void
geometry_tie(GtkWidget *win, GeometryType g) {
	geometry_tie_full(g, GTK_WINDOW(win), NULL);
}

void
geometry_tie_full(GeometryType g, GtkWindow *win, GtkPaned *paned) {
	Geometry *geom = &conf.geometries[g];

	/* the reference point is at the top left corner of the window itself,
	 * ignoring window manager decorations. */
	gtk_window_set_gravity(win, GDK_GRAVITY_STATIC);

	/* load the existing geometry for this window */
	geometry_load(geom, win, paned);
	/* and track the new geometry */
	g_signal_connect(G_OBJECT(win), "configure-event",
			G_CALLBACK(geometry_win_event), geom);
	g_signal_connect(G_OBJECT(win), "show",
			G_CALLBACK(geometry_show_cb), geom);
	if (paned)
		g_signal_connect(G_OBJECT(paned), "notify::position",
				G_CALLBACK(geometry_paned_event), geom);
}

static void
forget_cb(GObject *obj, gboolean *state) {
	*state = !*state;
}

/* GLib unfortunately doesn't make up its mind over whether TRUE or
 * FALSE should denote comparison success, so its provided g_str_equal
 * is useless as a custom plugin to g_list_find_custom. */
static gboolean
_str_equal(gconstpointer v1, gconstpointer v2) {
	return !g_str_equal(v1, v2);
}

void
jam_message_va(GtkWindow *parent, MessageType type, gboolean forgettable,
		const char *title, const char *message, va_list ap) {
	GtkWidget *dlg;
	GtkWidget *forget_check;
	gint msgtype = 0, buttontype = 0;
	const gchar *mtitle = NULL;
	gint res;
	gboolean forget_state;

	gchar ourhash[120];

	char fullmsg[1024];

	if (app.cli)
		return;

	g_vsnprintf(fullmsg, 1024, message, ap);

	{ /* compute hash of this message */
		GString *id;
		id = g_string_new(title);
		g_string_append(id, message);
		lj_md5_hash(id->str, ourhash);
		g_string_free(id, TRUE);
	}

	/* do nothing if the user has asked not to view this message again */
	if (g_slist_find_custom(app.quiet_dlgs, ourhash, _str_equal))
		return;

	switch (type) {
		case JAM_MSG_INFO:
			msgtype = GTK_MESSAGE_INFO;
			buttontype = GTK_BUTTONS_OK;
			mtitle = (gchar*)title;
			break;
		case JAM_MSG_WARNING:
			msgtype = GTK_MESSAGE_WARNING;
			buttontype = GTK_BUTTONS_CLOSE;
			mtitle = title ? (gchar*)title : _("Warning");
			break;
		case JAM_MSG_ERROR:
			msgtype = GTK_MESSAGE_ERROR;
			buttontype = GTK_BUTTONS_CLOSE;
			mtitle = title ? (gchar*)title : _("Error");
			break;
	}

	/* TODO: switch to jam_dialogs, which are prettier */
	dlg = gtk_message_dialog_new(parent, 0, msgtype,
			buttontype,
			fullmsg);
	gtk_window_set_title(GTK_WINDOW(dlg), title);
	gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(parent));

	if (forgettable) {
		forget_state = FALSE;
		forget_check = gtk_check_button_new_with_label(_("Do not show again"));
		g_signal_connect(G_OBJECT(forget_check), "toggled",
				G_CALLBACK(forget_cb), &forget_state);
		gtk_box_set_homogeneous(GTK_BOX(GTK_DIALOG(dlg)->action_area),
				FALSE); /* XXX: this doesn't work :( */

		/* aggressively make this dialog less ugly */
		gtk_button_box_set_layout(GTK_BUTTON_BOX(GTK_DIALOG(dlg)->action_area),
				GTK_BUTTONBOX_EDGE);
		gtk_box_set_child_packing(GTK_BOX(GTK_DIALOG(dlg)->action_area),
				((GtkBoxChild*)GTK_BOX(GTK_DIALOG(dlg)->action_area)->
				 	children->data)->widget, FALSE, FALSE, 0, GTK_PACK_END);

		/* force our checkbox to *really* be first */
		gtk_container_add(GTK_CONTAINER((GTK_DIALOG(dlg)->action_area)),
				forget_check);
		gtk_box_reorder_child(GTK_BOX((GTK_DIALOG(dlg)->action_area)),
				forget_check, 0);

		gtk_widget_show_all(GTK_DIALOG(dlg)->action_area);
	}

	res = gtk_dialog_run(GTK_DIALOG(dlg));

	/* flag this dialog for oblivion if the user didn't like it */
	if (forgettable && forget_state)
		app.quiet_dlgs = g_slist_append(app.quiet_dlgs, g_strdup(ourhash));

	gtk_widget_destroy(dlg);
}

void
jam_message(GtkWindow *parent, MessageType type, gboolean forgettable,
		const char *title, const char *message, ...) {
	va_list ap;
	va_start(ap, message);
	jam_message_va(parent, type, forgettable, title, message, ap);
	va_end(ap);
}

void jam_warning(GtkWindow *parent, const char *message, ...) {
	va_list ap;

	va_start(ap, message);
	jam_message_va(parent, JAM_MSG_WARNING, FALSE, NULL, message, ap);
	va_end(ap);
}

/* utility thunk function */
void jam_messagebox(GtkWindow *parent, const char *title, const char *message) {
	jam_message(parent, JAM_MSG_INFO, FALSE, title, message);
}

/* text sort utility function for GtkTreeModels */
gint
text_sort_func(GtkTreeModel *model, GtkTreeIter  *a, GtkTreeIter  *b,
		gpointer data) {
	gchar *ta, *tb;
	gint ret;
	gtk_tree_model_get(model, a, 0, &ta, -1);
	gtk_tree_model_get(model, b, 0, &tb, -1);
	ret = g_ascii_strcasecmp(ta, tb);
	g_free(ta);
	g_free(tb);
	return ret;
}

void
jam_widget_set_visible(GtkWidget *w, gboolean visible) {
	if (visible)
		gtk_widget_show(w);
	else
		gtk_widget_hide(w);
}

void
jam_widget_set_font(GtkWidget *w, const gchar *font_name) {
	PangoFontDescription *font_desc;

	font_desc = pango_font_description_from_string(font_name);
	gtk_widget_modify_font(w, font_desc);
	pango_font_description_free(font_desc);
}

GtkWidget*
labelled_box_new_all(const char *caption, GtkWidget *w,
                     gboolean expand, GtkSizeGroup *sg, GtkWidget *mne) {
	GtkWidget *hbox, *l;

	l = gtk_label_new_with_mnemonic(caption);
	gtk_misc_set_alignment(GTK_MISC(l), 0, 0.5);
	if (sg)
		gtk_size_group_add_widget(sg, l);
	if (!mne)
		mne = w;
	if (mne)
		gtk_label_set_mnemonic_widget(GTK_LABEL(l), mne);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), l, FALSE, FALSE, 0);
	if (w)
		gtk_box_pack_start(GTK_BOX(hbox), w, expand, expand, 0);
	return hbox;
}

GtkWidget*
jam_form_label_new(const char *text) {
	GtkWidget *label;
	label = gtk_label_new(text);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	return label;
}

static void
selection_enable_cb(GtkTreeSelection *sel, GtkWidget *w) {
	gtk_widget_set_sensitive(w,
			gtk_tree_selection_get_selected(sel, NULL, NULL));
}
static void
jr_up_cb(JamReorderable *jr) {
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter, i2;
	GtkTreePath *path;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(jr->view));
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;
	path = gtk_tree_model_get_path(model, &iter);
	if (gtk_tree_path_prev(path)) {
		gtk_tree_model_get_iter(model, &i2, path);
		gtk_list_store_swap(jr->store, &iter, &i2);
	}
	gtk_tree_path_free(path);
}
static void
jr_down_cb(JamReorderable *jr) {
	GtkTreeSelection *sel;
	GtkTreeIter iter, i2;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(jr->view));
	if (!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return;
	i2 = iter;
	if (gtk_tree_model_iter_next(GTK_TREE_MODEL(jr->store), &i2))
		gtk_list_store_swap(jr->store, &iter, &i2);
}

void
jam_reorderable_make(JamReorderable* jr) {
	GtkWidget *hbox, *bbox;
	GtkWidget *button;
	GtkTreeSelection *sel;

	jr->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(jr->store));

	jr->box = hbox = gtk_hbox_new(FALSE, 6);
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(jr->view));
	gtk_box_pack_start(GTK_BOX(hbox), scroll_wrap(jr->view), TRUE, TRUE, 0);

	bbox = gtk_vbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(bbox), 6);

	jr->add = button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	jr->edit = button = gtk_button_new_from_stock(GTK_STOCK_PROPERTIES);
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(selection_enable_cb), button);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	jr->remove = button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(selection_enable_cb), button);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(selection_enable_cb), button);
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
			G_CALLBACK(jr_up_cb), jr);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(bbox), button, TRUE);

	button = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(selection_enable_cb), button);
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
			G_CALLBACK(jr_down_cb), jr);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(bbox), button, TRUE);

	gtk_box_pack_start(GTK_BOX(hbox), bbox, FALSE, FALSE, 0);

	g_signal_emit_by_name(G_OBJECT(sel), "changed");
}

/* picked up from gtk/gtkclipboard.c, to allow blocking clipboard requests
   that time out */
typedef struct
{
  GMainLoop *loop;
  gpointer data;
} WaitResults;

/*static void
clipboard_received_func (GtkClipboard     *clipboard,
             GtkSelectionData *selection_data,
             gpointer          data)
{
  WaitResults *results = data;

  if (selection_data->length >= 0)
    results->data = gtk_selection_data_copy (selection_data);

  g_main_loop_quit (results->loop);
}*/

static void
clipboard_text_received_func (GtkClipboard *clipboard,
                  const gchar  *text,
                  gpointer      data)
{
  WaitResults *results = data;

  results->data = g_strdup (text);
  g_main_loop_quit (results->loop);
}

/* adapted from gtk_clipboard_wait_for_text */
gchar *
jam_clipboard_wait_for_text_timeout (GtkClipboard *clipboard, gint timeout)
{
  WaitResults results;
  guint tag;

  g_return_val_if_fail (clipboard != NULL, NULL);
  g_return_val_if_fail (clipboard != NULL, NULL);

  results.data = NULL;
  results.loop = g_main_loop_new (NULL, TRUE);

  tag = g_timeout_add(timeout, (GSourceFunc)g_main_loop_quit, results.loop);

  gtk_clipboard_request_text (clipboard,
                  clipboard_text_received_func,
                  &results);

  if (g_main_loop_is_running (results.loop))
    {
      GDK_THREADS_LEAVE ();
      g_main_loop_run (results.loop);
      GDK_THREADS_ENTER ();
    }

  g_source_remove(tag);

  g_main_loop_unref (results.loop);

  return results.data;
}

