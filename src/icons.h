/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef icons_h
#define icons_h

void       icons_initialize(void);
GdkPixbuf* icons_rarrow_pixbuf(void);
GdkPixbuf* icons_larrow_pixbuf(void);
GdkPixbuf* icons_lrarrow_pixbuf(void);

#ifndef HAVE_LIBRSVG
/* how many throbber images we have... */
#define THROBBER_COUNT 8
void       icons_load_throbber(GdkPixbuf *pbs[]);
#endif /* HAVE_LIBRSVG */

#endif /* icons_h */
