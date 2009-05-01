/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LOGJAM_SETTINGS_H__
#define __LOGJAM_SETTINGS_H__

#include "checkfriends.h"

/* this enum should match the notebook created in run_settings_dialog() */
typedef enum {
	SETTINGS_PAGE_UI = 0,
#ifndef G_OS_WIN32
	SETTINGS_PAGE_MUSIC,
	SETTINGS_PAGE_SYSTEM,
#endif
	SETTINGS_PAGE_CF,
	SETTINGS_PAGE_DEBUG
} SettingsPage;

void settings_run(JamWin *jw);
void settings_cf_run(CFMgr *cfm);

#endif /* __LOGJAM_SETTINGS_H__ */
