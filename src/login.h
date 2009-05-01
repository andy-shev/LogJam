/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef login_h
#define login_h

#include "account.h"

JamAccount* login_dlg_run(GtkWindow *parent, JamHost *host, JamAccount *acc);
gboolean    login_run(GtkWindow *parent, JamAccountLJ *acc);
gboolean    login_check_lastupdate(GtkWindow *parent, JamAccountLJ *acc);

#endif /* login_h */
