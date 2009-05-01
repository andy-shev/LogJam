/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2005 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LOGJAM_PROGRESS_H__
#define __LOGJAM_PROGRESS_H__

#define PROGRESS_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), progress_window_get_type(), ProgressWindow))
typedef struct _ProgressWindow ProgressWindow;

typedef void (*ProgressWindowCancelFunc)(gpointer data);

GType progress_window_get_type(void);
GtkWidget* progress_window_new(GtkWindow *parent, const char *title);

void progress_window_set_title(ProgressWindow *pw, const char *title);
void progress_window_set_text(ProgressWindow *pw, const char *text);
void progress_window_pack(ProgressWindow *pw, GtkWidget *contents);
void progress_window_show_error(ProgressWindow *pw, const char *fmt, ...);
void progress_window_set_progress(ProgressWindow *pw, float frac);
void progress_window_set_cancel_cb(ProgressWindow *pw,
		ProgressWindowCancelFunc func, gpointer data);

#endif /* __LOGJAM_PROGRESS_H__ */

