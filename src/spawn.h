/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __spawn_h__
#define __spawn_h__

#include <gtk/gtkwindow.h>
#include "conf.h" /* CommandList */

void spawn_url(GtkWindow *parent, const char *url);
extern const CommandList spawn_commands[];

#endif /* __spawn_h__ */
