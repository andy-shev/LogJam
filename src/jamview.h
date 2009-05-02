/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef _jam_view_
#define _jam_view_

#include "gtk-all.h"
#include "jamdoc.h"

typedef struct _JamView JamView;
typedef struct _JamViewClass JamViewClass;

/* these must match ACTION_VIEW_... in menu.c. */
/* these must match metas[] in jamview.c. */
typedef enum {
	JAM_VIEW_SUBJECT,
	JAM_VIEW_SECURITY,
	JAM_VIEW_MOOD,
	JAM_VIEW_PIC,
	JAM_VIEW_MUSIC,
	JAM_VIEW_LOCATION,
	JAM_VIEW_TAGS,
	JAM_VIEW_PREFORMATTED,
	JAM_VIEW_DATESEL,
	JAM_VIEW_COMMENTS,
	JAM_VIEW_META_COUNT
} JamViewMeta;
#define JAM_VIEW_META_FIRST JAM_VIEW_SECURITY
#define JAM_VIEW_META_LAST JAM_VIEW_COMMENTS

const char *jam_view_meta_to_name(JamViewMeta meta);
gboolean jam_view_meta_from_name(const char *name, JamViewMeta *meta);

#define JAM_TYPE_VIEW (jam_view_get_type())
#define JAM_VIEW(object) (G_TYPE_CHECK_INSTANCE_CAST((object), JAM_TYPE_VIEW, JamView))
#define JAM_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), JAM_TYPE_VIEW, JamViewClass))

GType jam_view_get_type(void);

GtkWidget* jam_view_new();
GObject*   jam_view_get_undomgr(JamView *view);
void jam_view_set_doc(JamView *view, JamDoc *doc);

void jam_view_settings_changed(JamView *view);

void jam_view_toggle_meta(JamView *view, JamViewMeta meta, gboolean show);
gboolean jam_view_get_meta_visible(JamView *view, JamViewMeta meta);

void jam_view_emit_conf(JamView *view);

#endif /* _jam_view_ */
