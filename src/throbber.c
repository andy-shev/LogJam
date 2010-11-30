/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <string.h>

#ifdef HAVE_LIBRSVG
  #include <librsvg/rsvg.h>
  #include "logo-svg.h"
#else
  #include "icons.h"
#endif

#include "throbber.h"
#include "util.h"

struct _Throbber {
	GtkImage   parent;
	guint      tag;

#ifdef HAVE_LIBRSVG
	int        angle;
#else
	int        state;
	GdkPixbuf *pb[THROBBER_COUNT];
#endif
};


#ifdef HAVE_LIBRSVG
/* delay in ms between frames of the throbber */
#define THROBBER_FRAME_DELAY 20
static void update_svg(Throbber *t);

void
throbber_reset(Throbber *t) {
	t->angle = 0;
	update_svg(t);
}

static void
throbber_finalize(GObject *object) {
	GObjectClass *parent_class;

	throbber_stop(THROBBER(object));

	parent_class = g_type_class_peek_parent(G_OBJECT_GET_CLASS(object));
	parent_class->finalize(object);
}

static void
size_cb(gint *width, gint *height, gpointer data) {
	*width = *height = 48;
}

static void
update_svg(Throbber *t) {
	RsvgHandle *handle;
	GdkPixbuf *pb;
	char *transform;
	GError *err = NULL;

	handle = rsvg_handle_new();
	rsvg_handle_set_size_callback(handle, size_cb, NULL, NULL);

	if (!rsvg_handle_write(handle,
				(unsigned char*)logo_svg_data_1, sizeof(logo_svg_data_1)-1,
				&err)) {
		g_print("error: %s\n", err->message);
		g_error_free(err);
		return;
	}

	transform = g_strdup_printf(
			"transform=\"translate(%d, %d) rotate(%d) translate(%d, %d)\"\n",
			logo_svg_translate2_x, logo_svg_translate2_y,
			t->angle,
			logo_svg_translate1_x, logo_svg_translate1_y);
	rsvg_handle_write(handle, (unsigned char*)transform, strlen(transform), NULL);
	g_free(transform);

	if (!rsvg_handle_write(handle,
				(unsigned char*)logo_svg_data_2, sizeof(logo_svg_data_2)-1,
				&err)) {
		g_print("error: %s\n", err->message);
		g_error_free(err);
		return;
	}
	if (!rsvg_handle_close(handle, &err)) {
		g_print("error: %s\n", err->message);
		g_error_free(err);
		return;
	}

	pb = rsvg_handle_get_pixbuf(handle);

	gtk_image_set_from_pixbuf(GTK_IMAGE(t), pb);
	g_object_unref(G_OBJECT(pb));
	rsvg_handle_free(handle);
}

static gboolean
throbber_cb(Throbber *t) {
	t->angle += 3;
	update_svg(t);
	return TRUE;
}

#else
/* we're doing the image-based throbber. */

/* delay in ms between frames of the throbber */
#define THROBBER_FRAME_DELAY 400

void
throbber_reset(Throbber *t) {
	icons_load_throbber(t->pb);
	gtk_image_set_from_pixbuf(GTK_IMAGE(t), t->pb[0]);
}

static gboolean
throbber_cb(Throbber *t) {
	t->state = (t->state + 1) % THROBBER_COUNT;
	gtk_image_set_from_pixbuf(GTK_IMAGE(t), t->pb[t->state]);
	return TRUE;
}

static void
throbber_finalize(GObject *object) {
	GObjectClass *parent_class;
	int i;

	throbber_stop(THROBBER(object));
	for (i = 0; i < THROBBER_COUNT; i++)
		g_object_unref(G_OBJECT(THROBBER(object)->pb[i]));

	parent_class = g_type_class_peek_parent(G_OBJECT_GET_CLASS(object));
	parent_class->finalize(object);
}


#endif

static void
throbber_init(Throbber *t) {
	throbber_reset(t);
}

void
throbber_start(Throbber *t) {
	if (t->tag)
		return;
	t->tag = gtk_timeout_add(THROBBER_FRAME_DELAY,
			(GSourceFunc)throbber_cb, t);
}
void
throbber_stop(Throbber *t) {
	if (!t->tag)
		return;
	gtk_timeout_remove(t->tag);
	t->tag = 0;
}

static void
throbber_class_init(gpointer klass, gpointer class_data) {
	GObjectClass *gclass = G_OBJECT_CLASS(klass);

	gclass->finalize = throbber_finalize;
}

GType
throbber_get_type(void) {
	static GType throbber_type = 0;
	if (!throbber_type) {
		static const GTypeInfo throbber_info = {
			sizeof (GtkImageClass),
			NULL,
			NULL,
			(GClassInitFunc) throbber_class_init,
			NULL,
			NULL,
			sizeof (Throbber),
			0,
			(GInstanceInitFunc) throbber_init,
		};
		throbber_type = g_type_register_static(GTK_TYPE_IMAGE, "Throbber",
				&throbber_info, 0);
	}
	return throbber_type;
}

GtkWidget*
throbber_new(void) {
	Throbber *throbber =
		THROBBER(g_object_new(throbber_get_type(), NULL));
	return GTK_WIDGET(throbber);
}

