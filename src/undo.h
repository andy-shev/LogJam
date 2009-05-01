/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 * Ported to LogJam by Ari Pollak <ari@debian.org>
 * 
 * This file is based on gedit-undo-manager.h from gEdit 2.2.1.
 * Original authors:
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi 
 */

#ifndef __UNDOMGR_H__
#define __UNDOMGR_H__

#define TYPE_UNDOMGR				(undomgr_get_type ())
#define UNDOMGR(obj)				(GTK_CHECK_CAST ((obj), TYPE_UNDOMGR, UndoMgr))
#define UNDOMGR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), TYPE_UNDOMGR, UndoMgrClass))
#define IS_UNDOMGR(obj)			(GTK_CHECK_TYPE ((obj), TYPE_UNDOMGR))
#define IS_UNDOMGR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), TYPE_UNDOMGR))
#define UNDOMGR_GET_CLASS(obj)		(GTK_CHECK_GET_CLASS ((obj), TYPE_UNDOMGR, UndoMgrClass))


typedef struct _UndoMgrPrivate		UndoMgrPrivate;

typedef struct 
{
	GObject base;
	
	UndoMgrPrivate *priv;
} UndoMgr;

typedef struct
{
	GObjectClass parent_class;

	/* Signals */
	void (*can_undo)( UndoMgr *um, gboolean can_undo);
   	void (*can_redo)( UndoMgr *um, gboolean can_redo);
} UndoMgrClass;

GType        	undomgr_get_type	(void) G_GNUC_CONST;

GObject*		undomgr_new		(void);
void			undomgr_attach		(UndoMgr *um, GtkWidget *widget);
void			undomgr_detach		(UndoMgr *um, GtkWidget *widget);

/* Clears the stack of undo actions: */
void			undomgr_reset		(UndoMgr *um);

gboolean		undomgr_can_undo	(const UndoMgr *um);
gboolean		undomgr_can_redo 	(const UndoMgr *um);

void			undomgr_undo 	(UndoMgr *um);
void			undomgr_redo 	(UndoMgr *um);

void	undomgr_begin_not_undoable_action 	(UndoMgr *um);
void	undomgr_end_not_undoable_action 	(UndoMgr *um);

#endif /* __UNDOMGR_H__ */
