/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LOGJAM_CHECKFRIENDS_H__
#define __LOGJAM_CHECKFRIENDS_H__

#include <glib-object.h>
#ifdef HAVE_GTK
#include "gtk-all.h"
#endif /* HAVE_GTK */

#include "account.h"

typedef enum {
	CF_DISABLED,
	CF_ON,
	CF_NEW
} CFState;

typedef struct _CFMgr CFMgr;
typedef struct _CFMgrClass CFMgrClass;

#define LOGJAM_TYPE_CFMGR (cfmgr_get_type())
#define LOGJAM_CFMGR(object) (G_TYPE_CHECK_INSTANCE_CAST((object), LOGJAM_TYPE_CFMGR, CFMgr))
#define LOGJAM_CFMGR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), LOGJAM_TYPE_CFMGR, CFMgrClass))

CFMgr* cfmgr_new(JamAccount *acc);
GType cfmgr_get_type(void);

JamAccountLJ* cfmgr_get_account(CFMgr *cfm);

void    cfmgr_set_mask(CFMgr *cfm, guint32 mask);

void    cfmgr_set_account(CFMgr *cfm, JamAccount *acc);
void    cfmgr_set_state(CFMgr *cfm, CFState state);
CFState cfmgr_get_state(CFMgr *cfm);

void cf_threshold_normalize(gint *threshold);

gboolean checkfriends_cli(JamAccountLJ *acc);
void     checkfriends_cli_purge(JamAccountLJ *acc);


#ifdef HAVE_GTK
typedef struct _CFFloat CFFloat;
CFFloat* cf_float_new(CFMgr *cfm);

void cf_float_decorate_refresh(void);

void cf_app_update_float(void);

#ifdef USE_DOCK
void cf_update_dock(CFMgr *cfm, GtkWindow* parent);
#endif
#endif /* HAVE_GTK */

#endif /* __LOGJAM_CHECKFRIENDS_H__ */
