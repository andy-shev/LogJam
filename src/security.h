/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __SECURITY_H__
#define __SECURITY_H__

#include <gtk/gtkoptionmenu.h>

#include <livejournal/livejournal.h>

#include "account.h"

/* A SecMgr is a widget which manages a security setting.
 * The LJSecurity object now lives in livejournal.[ch].
 */

#define SECMGR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), secmgr_get_type(), SecMgr))

typedef struct _SecMgr       SecMgr;

GType      secmgr_get_type(void);
GtkWidget* secmgr_new     (gboolean withcustom);
void       secmgr_security_set(SecMgr *secmgr, const LJSecurity *security);
void       secmgr_security_set_force(SecMgr *secmgr, const LJSecurity *security);
void       secmgr_security_get(SecMgr *secmgr, LJSecurity *security);
void       secmgr_set_account(SecMgr *sm, JamAccountLJ *account);

guint32    custom_security_dlg_run(GtkWindow *parent, guint32 mask, JamAccountLJ *acc);

#endif /* __SECURITY_H__ */
