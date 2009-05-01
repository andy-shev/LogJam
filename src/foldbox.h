/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef FOLDBOX_H
#define FOLDBOX_H

#include "groupedbox.h"

#define FOLDBOX_TYPE foldbox_get_type()
#define FOLDBOX(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), FOLDBOX_TYPE, FoldBox))

typedef enum {
	ARROW_LOC_NONE  = 0,
	ARROW_LOC_START = 1,
	ARROW_LOC_END   = 2
} FoldBoxArrowLocation;

typedef struct _FoldBox FoldBox;

struct _FoldBox {
	GroupedBox           parent;
	GtkWidget           *hbox;
	GtkWidget           *arrow, *heading;
	gboolean             unfolded;
	FoldBoxArrowLocation arrow_location;
};

GType foldbox_get_type(void);

GtkWidget* foldbox_new();
GtkWidget* foldbox_new_with_text(const char *text, gboolean unfolded, FoldBoxArrowLocation arrow_location);
void foldbox_set_heading_text(FoldBox *fb, const char *text);
void foldbox_toggle_cb(GtkWidget* w, GdkEventButton *ev, FoldBox *fb);
void foldbox_refresh(FoldBox *fb);

void foldbox_refresh_arrow(FoldBox *fb);

GType foldbox_get_type(void);

#endif /* FOLDBOX_H */
