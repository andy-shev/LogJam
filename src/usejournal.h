/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __USEJOURNAL_H__
#define __USEJOURNAL_H__

GtkWidget* usejournal_build_menu(const char *defaultjournal, const char *currentjournal, GSList *journals, gpointer doc);

#endif /* __USEJOURNAL_H__ */
