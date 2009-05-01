/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef tools_h
#define tools_h

#include "jamdoc.h"

void tools_html_escape      (GtkWindow *win, JamDoc *doc);
void tools_remove_linebreaks(GtkWindow *win, JamDoc *doc);
void tools_insert_file      (GtkWindow *win, JamDoc *doc);
void tools_insert_command_output
                            (GtkWindow *win, JamDoc *doc);
void tools_validate_xml     (GtkWindow *win, JamDoc *doc);
void tools_ljcut            (GtkWindow *win, JamDoc *doc);

#endif /* tools_h */
