/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef GROUPEDBOX_H
#define GROUPEDBOX_H

#define TYPE_GROUPEDBOX groupedbox_get_type()
#define GROUPEDBOX(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_GROUPEDBOX, GroupedBox))

typedef struct _GroupedBox GroupedBox;
typedef struct _GroupedBoxClass GroupedBoxClass;

/* +--------+
 * | header +
 * +-+------+
 * | |      |\
 * | | vbox |  body
 * | |      |/
 * +-+------+
 */
struct _GroupedBox {
	GtkVBox parent;
	GtkWidget *vbox;
	GtkWidget *header, *body; /* for foldbox subclass */
};

struct _GroupedBoxClass
{
	  GtkVBoxClass parent_class;
};

GType      groupedbox_get_type(void);

GtkWidget* groupedbox_new();
GtkWidget* groupedbox_new_with_text(const char *text);
void       groupedbox_set_header_widget(GroupedBox *b, GtkWidget *w);
void       groupedbox_set_header(GroupedBox *b, const char *title, gboolean bold);
void       groupedbox_pack(GroupedBox *b, GtkWidget *w, gboolean expand);
GType      groupedbox_get_type(void);

#endif /* GROUPEDBOX_H */

