/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __logjam_history_h__
#define __logjam_history_h__

#include "account.h"

LJEntry* history_recent_dialog_run(GtkWindow *parent, JamAccount *acc, const char *usejournal);
LJEntry* history_load_latest      (GtkWindow *parent, JamAccount *acc, const char *usejournal);
LJEntry* history_load_itemid      (GtkWindow *parent, JamAccount *acc, const char *usejournal, int itemid);

#endif /* __logjam_history_h__ */
