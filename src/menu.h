/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef menu_h
#define menu_h

#include "jam.h"

void menu_friends_manager(JamWin *jw);
GtkWidget* menu_make_bar(JamWin *jw);

void menu_new_doc(JamWin *jw);

#endif /* menu_h */
