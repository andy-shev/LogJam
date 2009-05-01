/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2005 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __html_preview_h__
#define __html_preview_h__

#include <gtkhtml/gtkhtml.h>
#include <livejournal/livejournal.h>
#include "jam.h"

typedef struct _HTMLPreview HTMLPreview;
typedef LJEntry* (*GetEntryFunc)(HTMLPreview *hp);

struct _HTMLPreview {
	GtkHTML      html; /* parent */
	GetEntryFunc get_entry;
	gpointer     get_entry_data;
};

typedef struct _PreviewUI PreviewUI;
struct _PreviewUI {
	GtkWidget *win;
	HTMLPreview *html;
	JamWin *jw;
};

GtkWidget *html_preview_new(GetEntryFunc get_entry, gpointer get_entry_data);
void preview_ui_show(JamWin *jw);
void preview_update(HTMLPreview *hp);

#endif /* __html_preview_h__ */
