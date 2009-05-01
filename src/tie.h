/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef _TIE_H_
#define _TIE_H_

GtkWidget* tie_toggle(GtkToggleButton *toggle, gboolean *data);
void tie_text  (GtkEntry *entry, char **data);
void tie_combo (GtkCombo *combo, char **data);

#endif /* _TIE_H_ */
