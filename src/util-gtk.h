/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2005 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __logjam_util_gtk_h__
#define __logjam_util_gtk_h__

#include "config.h"
#include "gtk-all.h"

#include "conf.h" /* for the "geometry" struct */

#define STACK(type, var) type actual_##var = {0}, *var = &actual_##var
#define UNREF_AND_NULL(x) if (x) { g_object_unref(G_OBJECT(x)); x = NULL; }

typedef enum {
	JAM_MSG_INFO,
	JAM_MSG_WARNING,
	JAM_MSG_ERROR
} MessageType;

gchar* gettext_translate_func(const gchar *path, gpointer data);

GtkWidget* jam_table_new(int rows, int cols);
GtkWidget* jam_table_label(GtkTable *table, int row, const char *text);
void       jam_table_content(GtkTable *table, int row, GtkWidget *content);
void       jam_table_label_content(GtkTable *table, int row, const char *text, GtkWidget *content);
void       jam_table_label_content_mne(GtkTable *table, int row, const char *text, GtkWidget *content, GtkWidget *mne);
void       jam_table_fillrow(GtkTable *table, int row, GtkWidget *content);
void       jam_spin_button_set(GtkSpinButton *w, gboolean numeric, gdouble range_low, gdouble range_high, gdouble increment_single, gdouble increment_page, gint digits);

/* if width or height <= 0, then we make up a reasonable size. */
void jam_win_set_size(GtkWindow *win, int width, int height);

GtkWidget* jam_dialog_set_contents(GtkDialog *dlg, GtkWidget *contents);

GtkWidget* jam_dialog_buttonbox_new(void);
void       jam_dialog_buttonbox_add(GtkWidget *box, GtkWidget *button);
GtkWidget* jam_dialog_buttonbox_button_with_label(GtkWidget *box, const char *label);
GtkWidget* jam_dialog_buttonbox_button_from_stock(GtkWidget *box, const char *id);
void       jam_dialog_set_contents_buttonbox(GtkWidget *dlg, GtkWidget *contents, GtkWidget *buttonbox);

int  jam_confirm(GtkWindow *parent, const char *title, const char *msg);
void jam_message(GtkWindow *parent, MessageType type, gboolean forgettable, const char *title, const char *message, ...);
void jam_messagebox(GtkWindow *parent, const char *title, const char *message);
void jam_warning(GtkWindow *parent, const char *msg, ...);

/*
gdk_color_parse() does exactly this:
void hex_to_gdkcolor(const char *buf, GdkColor *c);
*/
void gdkcolor_to_hex(GdkColor *color, char* buf);

/* wrap this widget in a scrollarea */
GtkWidget* scroll_wrap(GtkWidget *w);

/* tie a geometry struct to a window */
void geometry_tie(GtkWidget *win, GeometryType g);
void geometry_tie_full(GeometryType g, GtkWindow *win, GtkPaned *paned);

void jam_widget_set_visible(GtkWidget *w, gboolean visible);
void jam_widget_set_font(GtkWidget *w, const gchar *font_name);

/* produces an hbox that has a label and a widget, with optional
 * sizegroup, expansion setting, and mnemonic widget. */
GtkWidget* labelled_box_new_all(const char *caption, GtkWidget *w,
                                gboolean expand, GtkSizeGroup *sg,
                                GtkWidget *mne);
#define labelled_box_new_sg(c, w, sg) labelled_box_new_all(c, w, TRUE, sg, NULL)
#define labelled_box_new_expand(c, w, e) labelled_box_new_all(c, w, e, NULL, NULL)
#define labelled_box_new(c, w) labelled_box_new_all(c, w, TRUE, NULL, NULL)

/* like gtk_label_new, but appropriate for a form. */
GtkWidget* jam_form_label_new(const char *text);

typedef struct {
	GtkWidget *box;
	GtkWidget *view;
	GtkListStore *store;
	GtkWidget *add, *edit, *remove;
} JamReorderable;
void jam_reorderable_make(JamReorderable* jr);

gchar * jam_clipboard_wait_for_text_timeout (GtkClipboard *clipboard, gint timeout);

#endif /* __logjam_util_gtk_h__ */

