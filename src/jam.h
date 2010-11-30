/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef jam_h
#define jam_h

#include <livejournal/livejournal.h>

#include "jamdoc.h"
#include "jamview.h"
#include "undo.h"

typedef struct {
	GtkWindow win; /* super class */

	GtkItemFactory *factory;

	/* menu items. */
	GtkWidget *mweb, *msubmitsep, *msubmit, *msaveserver;
	GtkWidget *mundo, *mredo;

	GtkWidget *userlabel;
	GtkWidget *baction; /* "action" button:
	                       submit / save changes */
	GtkWidget *bdelete; /* "delete" button */

#if not_yet
	GtkWidget *nb_entry; /* syncbook master */
	GtkWidget *nb_meta;  /* syncbook slave */
#endif
	JamDoc *doc;
	GtkWidget *view;
	JamAccount *account;

	gpointer preview; /* we only want one preview window per jam_win. */
} JamWin;

void jam_font_set(GtkWidget *w, gchar *font_name);
void jam_run(JamDoc *doc);
void jam_do_changeuser(JamWin *jw);
gboolean jam_confirm_lose_entry(JamWin *jw);

void jam_clear_entry(JamWin *jw);
void jam_open_entry(JamWin *jw);
void jam_open_draft(JamWin *jw);

gboolean jam_save_as_file(JamWin *jw);
gboolean jam_save_as_draft(JamWin *jw);
gboolean jam_save(JamWin *jw);

void jam_load_entry(JamWin *jw, LJEntry *entry);
void jam_submit_entry(JamWin *jw);
void jam_save_entry_server(JamWin *jw);

void jam_quit(JamWin *jw);
void jam_autosave_init(JamWin *jw);
void jam_autosave_stop(JamWin *jw);

JamDoc* jam_win_get_cur_doc(JamWin *jw);
JamView* jam_win_get_cur_view(JamWin *jw);

#endif /* jam_h */

