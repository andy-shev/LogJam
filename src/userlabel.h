/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __JAM_USERLABEL_H__
#define __JAM_USERLABEL_H__

#define JAM_USER_LABEL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), jam_user_label_get_type(), JamUserLabel))
typedef struct _JamUserLabel      JamUserLabel;

GtkWidget* jam_user_label_new(void);
GType jam_user_label_get_type(void);

void jam_user_label_set_account(JamUserLabel *jul, JamAccount *acc);
void jam_user_label_set_journal(JamUserLabel *jul, const char *journalname);

#endif /* __JAM_USERLABEL_H__ */


