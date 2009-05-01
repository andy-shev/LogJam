/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __throbber_h__
#define __throbber_h__

#define THROBBER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), throbber_get_type(), Throbber))
typedef struct _Throbber Throbber;

GType      throbber_get_type(void);
GtkWidget* throbber_new(void);

void       throbber_start(Throbber *t);
void       throbber_stop (Throbber *t);
void       throbber_reset(Throbber *t);

#endif /* __throbber_h__ */

