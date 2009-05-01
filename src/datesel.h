/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef datesel_h
#define datesel_h

#include <gtk/gtkoptionmenu.h>

#define DATESEL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), datesel_get_type(), DateSel))

typedef struct _DateSel      DateSel;

GType      datesel_get_type(void);
GtkWidget* datesel_new(void);

void     datesel_get_tm(DateSel *ds, struct tm *ptm);
void     datesel_set_tm(DateSel *ds, struct tm *ptm);
gboolean datesel_get_backdated(DateSel *ds);
void     datesel_set_backdated(DateSel *ds, gboolean backdated);

#endif /* datesel_h */

