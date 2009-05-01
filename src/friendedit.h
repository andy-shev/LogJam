/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef friendedit_h
#define friendedit_h

#include <livejournal/friends.h>

LJFriend* friend_edit_dlg_run(GtkWindow *parent, JamAccountLJ *acc, gboolean edit, LJFriend *f);

#endif /* friendedit_h */

