/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 * Ported to LogJam by Ari Pollak <ari@debian.org> in May 2003
 *
 * This file is based on gedit-undo-manager.c from gEdit 2.2.1.
 * Original authors:
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 */

#include "config.h"

#include "gtk-all.h"
#include <stdlib.h>
#include <string.h>

#include "undo.h"

typedef struct _UndoAction  		UndoAction;
typedef struct _UndoInsertAction	UndoInsertAction;
typedef struct _UndoDeleteAction	UndoDeleteAction;

typedef enum {
	UNDO_ACTION_INSERT,
	UNDO_ACTION_DELETE
} UndoActionType;

/* 
 * We use offsets instead of GtkTextIters because the last ones
 * require to much memory in this context without giving us any advantage.
 */ 

struct _UndoInsertAction
{
	gint   pos; 
	gchar *text;
	gint   length;
	gint   chars;
};

struct _UndoDeleteAction
{
	gint   start;
	gint   end;
	gchar *text;
};

struct _UndoAction
{
	UndoActionType action_type;
	
	union {
		UndoInsertAction	insert;
		UndoDeleteAction	delete;
	} action;

	GObject *object;

	gboolean mergeable;

	gint order_in_group;
};

struct _UndoMgrPrivate
{
	GList*		widgets; /* queue of all widgets which are attached */
	
	GList*		actions;
	gint 		next_redo;	

	gint 		actions_in_current_group;
	
	gboolean 	can_undo;
	gboolean	can_redo;

	gint		running_not_undoable_actions;

	gint		num_of_groups;
};

enum {
	CAN_UNDO,
	CAN_REDO,
	LAST_SIGNAL
};

static void undomgr_class_init 		(UndoMgrClass 	*klass);
static void undomgr_init				(UndoMgr 	*um);
static void undomgr_finalize			(GObject 		*object);

static void undomgr_textbuffer_insert_text_handler	(GtkTextBuffer *buffer, GtkTextIter *pos,
												const gchar *text, gint length, 
												UndoMgr *um);
static void undomgr_textbuffer_delete_range_handler 	(GtkTextBuffer *buffer, GtkTextIter *start,
                        		      		 GtkTextIter *end, UndoMgr *um);
static void undomgr_textbuffer_begin_user_action_handler (GtkTextBuffer *buffer, UndoMgr *um);
static void undomgr_textbuffer_end_user_action_handler   (GtkTextBuffer *buffer, UndoMgr *um);

static void undomgr_free_action_list		(UndoMgr *um);

static void undomgr_free_widget			(UndoMgr *um, GtkWidget *w);
static void undomgr_free_widget_list		(UndoMgr *um);

static void undomgr_add_textbuffer_action	(UndoMgr *um, 
												UndoAction undo_action,
												GtkTextBuffer *buffer);

static gboolean undomgr_merge_textbuffer_action		(UndoMgr *um, 
												UndoAction *undo_action,
												GtkTextBuffer *buffer);

static void undomgr_free_first_n_actions 	(UndoMgr *um, gint n);
static void undomgr_check_list_size		(UndoMgr *um);



static GObjectClass 	*parent_class 				= NULL;
static guint			undomgr_signals [LAST_SIGNAL] 	= { 0 };

GType
undomgr_get_type (void)
{
	static GType undomgr_type = 0;

	if (undomgr_type == 0)
	{
		static const GTypeInfo our_info =
			{
				sizeof (UndoMgrClass),
				NULL,		/* base_init */
				NULL,		/* base_finalize */
				(GClassInitFunc) undomgr_class_init,
				NULL,           /* class_finalize */
				NULL,           /* class_data */
				sizeof (UndoMgr),
				0,              /* n_preallocs */
				(GInstanceInitFunc) undomgr_init
			};

			undomgr_type = g_type_register_static (G_TYPE_OBJECT,
									"UndoMgr",
									&our_info,
									0);
    	}

	return undomgr_type;
}

static void
undomgr_class_init (UndoMgrClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

  	parent_class = g_type_class_peek_parent (klass);

  	object_class->finalize = undomgr_finalize;

	klass->can_undo 	= NULL;
	klass->can_redo 	= NULL;
	
	undomgr_signals[CAN_UNDO] =
   		g_signal_new ("can_undo",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UndoMgrClass, can_undo),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_BOOLEAN);

	undomgr_signals[CAN_REDO] =
   		g_signal_new ("can_redo",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UndoMgrClass, can_redo),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_BOOLEAN);

}

static void
undomgr_init (UndoMgr *um)
{
	um->priv = g_new0 (UndoMgrPrivate, 1);

	um->priv->actions = NULL;
	um->priv->widgets = NULL;
	um->priv->next_redo = 0;

	um->priv->can_undo = FALSE;
	um->priv->can_redo = FALSE;

	um->priv->running_not_undoable_actions = 0;

	um->priv->num_of_groups = 0;
}

static void
undomgr_finalize (GObject *object)
{
	UndoMgr *um;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_UNDOMGR (object));
	
   	um = UNDOMGR (object);

	g_return_if_fail (um->priv != NULL);

	if (um->priv->actions != NULL)
		undomgr_free_action_list (um);

	if (um->priv->actions != NULL)
		undomgr_free_widget_list (um);

	g_free (um->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GObject*
undomgr_new ()
{
 	UndoMgr *um;

	um = UNDOMGR (g_object_new (TYPE_UNDOMGR, NULL));

	g_return_val_if_fail (um->priv != NULL, NULL);

	return G_OBJECT(um);
}

void
undomgr_attach (UndoMgr *um, GtkWidget *widget) {

	/* Return if the widget is already in the list */
	if(g_list_find(um->priv->widgets, widget) != NULL)
		return;
	
	/* FIXME for all widget types! */

	if(GTK_IS_TEXT_VIEW(widget)) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));

		g_signal_connect (G_OBJECT (buffer), "insert_text",
			  G_CALLBACK (undomgr_textbuffer_insert_text_handler), 
			  um);

		g_signal_connect (G_OBJECT (buffer), "delete_range",
			  G_CALLBACK (undomgr_textbuffer_delete_range_handler), 
			  um);

		g_signal_connect (G_OBJECT (buffer), "begin_user_action",
			  G_CALLBACK (undomgr_textbuffer_begin_user_action_handler), 
			  um);

		g_signal_connect (G_OBJECT (buffer), "end_user_action",
			  G_CALLBACK (undomgr_textbuffer_end_user_action_handler), 
			  um);
	
		um->priv->widgets = g_list_append(um->priv->widgets, widget);
		/*g_print("Added widget textbuffer\n");*/
	} else {
		g_printerr("Unable to handle widget type in undomgr_attach.\n Widget: %s\n", G_OBJECT_TYPE_NAME(G_OBJECT(widget)));
	}	
}

void undomgr_detach(UndoMgr *um, GtkWidget *widget) {
	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);
	g_return_if_fail (um->priv->widgets != NULL);
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (g_list_find(um->priv->widgets, widget));

	undomgr_reset(um);
	undomgr_free_widget(um, widget);
	um->priv->widgets = g_list_remove(um->priv->widgets, widget);
}

void 
undomgr_begin_not_undoable_action (UndoMgr *um)
{
	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);

	++um->priv->running_not_undoable_actions;
}

static void 
undomgr_end_not_undoable_action_internal (UndoMgr *um)
{
	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);

	g_return_if_fail (um->priv->running_not_undoable_actions > 0);
	
	--um->priv->running_not_undoable_actions;
}

void 
undomgr_end_not_undoable_action (UndoMgr *um)
{
	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);

	undomgr_end_not_undoable_action_internal (um);

	if (um->priv->running_not_undoable_actions == 0)
	{	
		undomgr_free_action_list (um);
	
		um->priv->next_redo = -1;	

		if (um->priv->can_undo)
		{
			um->priv->can_undo = FALSE;
			g_signal_emit (G_OBJECT (um), undomgr_signals [CAN_UNDO], 0, FALSE);
		}

		if (um->priv->can_redo)
		{
			um->priv->can_redo = FALSE;
			g_signal_emit (G_OBJECT (um), undomgr_signals [CAN_REDO], 0, FALSE);
		}
	}
}


gboolean
undomgr_can_undo (const UndoMgr *um)
{
	g_return_val_if_fail (IS_UNDOMGR (um), FALSE);
	g_return_val_if_fail (um->priv != NULL, FALSE);

	return um->priv->can_undo;
}

gboolean 
undomgr_can_redo (const UndoMgr *um)
{
	g_return_val_if_fail (IS_UNDOMGR (um), FALSE);
	g_return_val_if_fail (um->priv != NULL, FALSE);

	return um->priv->can_redo;
}

void 
undomgr_undo (UndoMgr *um)
{
	UndoAction *undo_action;

	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);
	g_return_if_fail (um->priv->can_undo);
	
	undomgr_begin_not_undoable_action (um);

	do
	{
		++um->priv->next_redo;
		
		undo_action = g_list_nth_data (um->priv->actions, um->priv->next_redo);
		g_return_if_fail (undo_action != NULL);

		/* FIXME for more widget types! */
		if(GTK_IS_TEXT_BUFFER(undo_action->object)) {
			GtkTextBuffer *buffer = GTK_TEXT_BUFFER(undo_action->object);
			GtkTextIter start, end;

			switch (undo_action->action_type)
			{
				case UNDO_ACTION_DELETE:
					gtk_text_buffer_get_iter_at_offset(
										buffer,
										&start,
										undo_action->action.delete.start);
					
					gtk_text_buffer_place_cursor(buffer, &start);

					gtk_text_buffer_insert(
						buffer,
						&start,
						undo_action->action.delete.text,
						(int)strlen(undo_action->action.delete.text));
					
					break;

				case UNDO_ACTION_INSERT:
					gtk_text_buffer_get_iter_at_offset(
											buffer,
											&start,
											undo_action->action.insert.pos);
					gtk_text_buffer_get_iter_at_offset(
									buffer,
									&end,
									undo_action->action.insert.pos +
										undo_action->action.insert.chars);

					gtk_text_buffer_delete(buffer, &start, &end);

					gtk_text_buffer_place_cursor(buffer, &start);

					break;

				default:
					g_warning ("This should not happen.");
					return;
			}
		} else {
			g_warning("Don't know how to handle action.");
		}
	} while (undo_action->order_in_group > 1);

	undomgr_end_not_undoable_action_internal (um);
	
	if (!um->priv->can_redo)
	{
		um->priv->can_redo = TRUE;
		g_signal_emit (G_OBJECT (um), undomgr_signals [CAN_REDO], 0, TRUE);
	}

	if (um->priv->next_redo >= (gint)(g_list_length (um->priv->actions) - 1))
	{
		um->priv->can_undo = FALSE;
		g_signal_emit (G_OBJECT (um), undomgr_signals [CAN_UNDO], 0, FALSE);
	}
}

void 
undomgr_redo (UndoMgr *um)
{
	UndoAction *undo_action;

	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);
	g_return_if_fail (um->priv->can_redo);
	
	undo_action = g_list_nth_data (um->priv->actions, um->priv->next_redo);
	g_return_if_fail (undo_action != NULL);

	undomgr_begin_not_undoable_action (um);

	do
	{
		/* FIXME for multiple widget types! */
		if(GTK_IS_TEXT_BUFFER(undo_action->object)) {
			GtkTextBuffer *buffer = GTK_TEXT_BUFFER(undo_action->object);
			GtkTextIter start, end;

			switch (undo_action->action_type)
			{
				case UNDO_ACTION_DELETE:
					gtk_text_buffer_get_iter_at_offset(
											buffer,
											&start,
											undo_action->action.delete.start);
					gtk_text_buffer_get_iter_at_offset(
											buffer,
											&end,
											undo_action->action.delete.end);

					gtk_text_buffer_delete(buffer, &start, &end);

					gtk_text_buffer_place_cursor(buffer, &start);

					break;
				
				case UNDO_ACTION_INSERT:
					gtk_text_buffer_get_iter_at_offset(
											buffer,
											&start,
											undo_action->action.insert.pos);
					
					gtk_text_buffer_place_cursor(buffer, &start);
					
					gtk_text_buffer_insert(buffer,
											&start,
											undo_action->action.insert.text,
											undo_action->action.insert.length);
					
					break;

				default:
					g_warning ("This should not happen.");
					return;
			}
		} else {
			g_warning("Redoing unknown widget type.");
		}
		--um->priv->next_redo;

		if (um->priv->next_redo < 0)
			undo_action = NULL;
		else
			undo_action = g_list_nth_data (um->priv->actions, um->priv->next_redo);
		
	} while ((undo_action != NULL) && (undo_action->order_in_group > 1));

	undomgr_end_not_undoable_action_internal (um);

	if (um->priv->next_redo < 0)
	{
		um->priv->can_redo = FALSE;
		g_signal_emit (G_OBJECT (um), undomgr_signals [CAN_REDO], 0, FALSE);
	}

	if (!um->priv->can_undo)
	{
		um->priv->can_undo = TRUE;
		g_signal_emit (G_OBJECT (um), undomgr_signals [CAN_UNDO], 0, TRUE);
	}

}

static void 
undomgr_free_action_list (UndoMgr *um)
{
	gint n, len;
	
	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);

	if (um->priv->actions == NULL)
	{
		return;
	}
	len = g_list_length (um->priv->actions);
	
	for (n = 0; n < len; n++)
	{
		UndoAction *undo_action = 
			(UndoAction *)(g_list_nth_data (um->priv->actions, n));

		/* gedit_debug (DEBUG_UNDO, "Free action (type %s) %d/%d", 
				(undo_action->action_type == GEDIT_UNDO_ACTION_INSERT) ? "insert":
				"delete", n, len); */

		if (undo_action->action_type == UNDO_ACTION_INSERT)
			g_free (undo_action->action.insert.text);
		else if (undo_action->action_type == UNDO_ACTION_DELETE)
			g_free (undo_action->action.delete.text);
		else
			g_return_if_fail (FALSE);

		if (undo_action->order_in_group == 1)
			--um->priv->num_of_groups;

		g_free (undo_action);
	}

	g_list_free (um->priv->actions);
	um->priv->actions = NULL;	
}

static void
undomgr_free_widget(UndoMgr *um, GtkWidget *w)
{
	/* FIXME for all widget types! */
	if(GTK_IS_TEXT_VIEW(w)) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));

		g_signal_handlers_disconnect_by_func (
		  G_OBJECT (buffer),
		  G_CALLBACK (undomgr_textbuffer_delete_range_handler), 
		  um);

		g_signal_handlers_disconnect_by_func (
		  G_OBJECT (buffer),
		  G_CALLBACK (undomgr_textbuffer_insert_text_handler), 
		  um);

		g_signal_handlers_disconnect_by_func (
		  G_OBJECT (buffer),
		  G_CALLBACK (undomgr_textbuffer_begin_user_action_handler), 
		  um);

		g_signal_handlers_disconnect_by_func (
		  G_OBJECT (buffer),
		  G_CALLBACK (undomgr_textbuffer_end_user_action_handler), 
		  um);
	} else {
		g_printerr("Unable to handle widget type in undomgr_free_widget_list.\n Widget: %s\n", G_OBJECT_TYPE_NAME(G_OBJECT(w)));
	}
}

static void 
undomgr_free_widget_list (UndoMgr *um)
{
	gint n, len;
	
	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);
	g_return_if_fail (um->priv->widgets != NULL);
	
	len = g_list_length (um->priv->actions);
	for(n = 0; n < len; n++) {
		GtkWidget *w = (GtkWidget *)(g_list_nth_data (um->priv->actions, n));

		undomgr_free_widget(um, w);
	}	
		
	g_list_free (um->priv->actions);
	um->priv->actions = NULL;
}

static void 
undomgr_textbuffer_insert_text_handler (GtkTextBuffer *buffer,
					GtkTextIter *pos,
					const gchar *text, gint length, UndoMgr *um)
{
	UndoAction undo_action;
	
	if (um->priv->running_not_undoable_actions > 0)
		return;

	g_return_if_fail (strlen (text) == (guint)length);
	
	undo_action.action_type = UNDO_ACTION_INSERT;

	undo_action.action.insert.pos    = gtk_text_iter_get_offset (pos);
	undo_action.action.insert.text   = (gchar*) text;
	undo_action.action.insert.length = length;
	undo_action.action.insert.chars  = g_utf8_strlen (text, length);

	if ((undo_action.action.insert.chars > 1) || (g_utf8_get_char (text) == '\n'))

	       	undo_action.mergeable = FALSE;
	else
		undo_action.mergeable = TRUE;

	undomgr_add_textbuffer_action (um, undo_action, buffer);
}

static void 
undomgr_textbuffer_delete_range_handler (GtkTextBuffer *buffer,
									GtkTextIter *start,
									GtkTextIter *end, UndoMgr *um)
{
	UndoAction undo_action;

	if (um->priv->running_not_undoable_actions > 0)
		return;

	undo_action.action_type = UNDO_ACTION_DELETE;

	gtk_text_iter_order (start, end);

	undo_action.action.delete.start  = gtk_text_iter_get_offset (start);
	undo_action.action.delete.end    = gtk_text_iter_get_offset (end);

	undo_action.action.delete.text   = gtk_text_buffer_get_slice (
										buffer,
										start,
										end,
										TRUE);

	if (((undo_action.action.delete.end - undo_action.action.delete.start) > 1) ||
	     (g_utf8_get_char (undo_action.action.delete.text  ) == '\n'))
	       	undo_action.mergeable = FALSE;
	else
		undo_action.mergeable = TRUE;

	/* g_print ("START: %d\n", undo_action.action.delete.start);
	g_print ("END: %d\n", undo_action.action.delete.end);
	g_print ("TEXT: %s\n", undo_action.action.delete.text); */

	undomgr_add_textbuffer_action (um, undo_action, buffer);

	g_free (undo_action.action.delete.text);
}

static void 
undomgr_textbuffer_begin_user_action_handler (GtkTextBuffer *buffer,
													UndoMgr *um)
{
	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);

	if (um->priv->running_not_undoable_actions > 0)
		return;

	um->priv->actions_in_current_group = 0;
}

static void
undomgr_textbuffer_end_user_action_handler (GtkTextBuffer *buffer,
												UndoMgr *um)
{
	if (um->priv->running_not_undoable_actions > 0)
		return;

	/* TODO: is it needed ? */
}

/* FIXME: change prototype to use UndoAction *undo_action : Paolo */
static void
undomgr_add_textbuffer_action (UndoMgr *um, UndoAction undo_action,
							GtkTextBuffer *buffer)
{
	UndoAction* action;
	
	g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

	if (um->priv->next_redo >= 0)
	{
		undomgr_free_first_n_actions	(um, um->priv->next_redo + 1);
	}

	um->priv->next_redo = -1;

	if (!undomgr_merge_textbuffer_action (um, &undo_action, buffer))
	{
		action = g_new (UndoAction, 1);
		*action = undo_action;

		action->object = G_OBJECT(buffer);

		if (action->action_type == UNDO_ACTION_INSERT)
			action->action.insert.text = g_strdup (undo_action.action.insert.text);
		else if (action->action_type == UNDO_ACTION_DELETE)
			action->action.delete.text = g_strdup (undo_action.action.delete.text); 
		else
		{
			g_free (action);
			g_return_if_fail (FALSE);
		}
		
		++um->priv->actions_in_current_group;
		action->order_in_group = um->priv->actions_in_current_group;

		if (action->order_in_group == 1)
			++um->priv->num_of_groups;
	
		um->priv->actions = g_list_prepend (um->priv->actions, action);
	}
	
	undomgr_check_list_size (um);

	if (!um->priv->can_undo)
	{
		um->priv->can_undo = TRUE;
		g_signal_emit (G_OBJECT (um), undomgr_signals [CAN_UNDO], 0, TRUE);
	}

	if (um->priv->can_redo)
	{
		um->priv->can_redo = FALSE;
		g_signal_emit (G_OBJECT (um), undomgr_signals [CAN_REDO], 0, FALSE);
	}
}

static void 
undomgr_free_first_n_actions (UndoMgr *um, gint n)
{
	gint i;
	
	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);

	if (um->priv->actions == NULL)
		return;
	
	for (i = 0; i < n; i++)
	{
		UndoAction *undo_action = 
			(UndoAction *)(g_list_first (um->priv->actions)->data);
	
		if (undo_action->action_type == UNDO_ACTION_INSERT)
			g_free (undo_action->action.insert.text);
		else if (undo_action->action_type == UNDO_ACTION_DELETE)
			g_free (undo_action->action.delete.text);
		else
			g_return_if_fail (FALSE);

		if (undo_action->order_in_group == 1)
			--um->priv->num_of_groups;

		g_free (undo_action);

		um->priv->actions = g_list_delete_link (um->priv->actions, um->priv->actions);

		if (um->priv->actions == NULL) 
			return;
	}
}

static void 
undomgr_check_list_size (UndoMgr *um)
{
	gint undo_levels;
	
	g_return_if_fail (IS_UNDOMGR (um));
	g_return_if_fail (um->priv != NULL);
	
	/* FIXME: should this be a preference? */
	undo_levels = 25;
	
	if (undo_levels < 1)
		return;

	if (um->priv->num_of_groups > undo_levels)
	{
		UndoAction *undo_action;
		GList* last;
		
		last = g_list_last (um->priv->actions);
		undo_action = (UndoAction*) last->data;
			
		do
		{
			if (undo_action->action_type == UNDO_ACTION_INSERT)
				g_free (undo_action->action.insert.text);
			else if (undo_action->action_type == UNDO_ACTION_DELETE)
				g_free (undo_action->action.delete.text);
			else
				g_return_if_fail (FALSE);

			if (undo_action->order_in_group == 1)
				--um->priv->num_of_groups;

			g_free (undo_action);

			um->priv->actions = g_list_delete_link (um->priv->actions, last);
			g_return_if_fail (um->priv->actions != NULL); 

			last = g_list_last (um->priv->actions);
			undo_action = (UndoAction*) last->data;

		} while ((undo_action->order_in_group > 1) || 
			 (um->priv->num_of_groups > undo_levels));
	}	
}

/**
 * undomgr_merge_textbuffer_action:
 * @um: an #UndoMgr 
 * @undo_action:
 * @buffer:
 * 
 * This function tries to merge the undo action at the top of
 * the stack with a new undo action. So when we undo for example
 * typing, we can undo the whole word and not each letter by itself
 * 
 * Return Value: TRUE is merge was sucessful, FALSE otherwise
 **/
static gboolean 
undomgr_merge_textbuffer_action (UndoMgr *um, UndoAction *undo_action,
							GtkTextBuffer *buffer)
{
	UndoAction *last_action;

	g_return_val_if_fail (GTK_IS_TEXT_BUFFER(buffer), FALSE);
	g_return_val_if_fail (IS_UNDOMGR (um), FALSE);
	g_return_val_if_fail (um->priv != NULL, FALSE);
	
	if (um->priv->actions == NULL)
		return FALSE;

	last_action = (UndoAction*) g_list_nth_data (um->priv->actions, 0);

	if (!last_action->mergeable)
		return FALSE;

	if ((!undo_action->mergeable) ||
	    (undo_action->action_type != last_action->action_type))
	{
		last_action->mergeable = FALSE;
		return FALSE;
	}

	if (undo_action->action_type == UNDO_ACTION_DELETE)
	{				
		if ((last_action->action.delete.start != undo_action->action.delete.start) &&
			(last_action->action.delete.start != undo_action->action.delete.end))
		{
			last_action->mergeable = FALSE;
			return FALSE;
		}
	
		if (last_action->action.delete.start == undo_action->action.delete.start)
		{
			gchar *str;
			
#define L  (last_action->action.delete.end - last_action->action.delete.start - 1)
#define g_utf8_get_char_at(p,i) g_utf8_get_char(g_utf8_offset_to_pointer((p),(i)))
		
			/* Deleted with the delete key */
			if ((g_utf8_get_char (undo_action->action.delete.text) != ' ') &&
				(g_utf8_get_char (undo_action->action.delete.text) != '\t') &&
                           ((g_utf8_get_char_at (last_action->action.delete.text, L) == ' ') ||
				(g_utf8_get_char_at (last_action->action.delete.text, L)  == '\t')))
			{
				last_action->mergeable = FALSE;
				return FALSE;
			}
		
			str = g_strdup_printf ("%s%s",
					last_action->action.delete.text, 
					undo_action->action.delete.text);
			
			g_free (last_action->action.delete.text);
			last_action->action.delete.end += 
				(undo_action->action.delete.end - 
				 undo_action->action.delete.start);
			last_action->action.delete.text = str;
		}
		else
		{
			gchar *str;
		
			/* Deleted with the backspace key */
			if ((g_utf8_get_char (undo_action->action.delete.text) != ' ') &&
				(g_utf8_get_char (undo_action->action.delete.text) != '\t') &&
                           ((g_utf8_get_char (last_action->action.delete.text) == ' ') ||
				(g_utf8_get_char (last_action->action.delete.text) == '\t')))
			{
				last_action->mergeable = FALSE;
				return FALSE;
			}

			str = g_strdup_printf ("%s%s",
					undo_action->action.delete.text, 
					last_action->action.delete.text);
		
			g_free (last_action->action.delete.text);
			last_action->action.delete.start = undo_action->action.delete.start;
			last_action->action.delete.text = str;
		}

		return TRUE;
	}
	else if (undo_action->action_type == UNDO_ACTION_INSERT)
	{
		gchar* str;
	
#define I (last_action->action.insert.chars - 1)
		
		if ((undo_action->action.insert.pos != 
	     	(last_action->action.insert.pos + last_action->action.insert.chars)) ||
			((g_utf8_get_char (undo_action->action.insert.text) != ' ') &&
			(g_utf8_get_char (undo_action->action.insert.text) != '\t') &&
			((g_utf8_get_char_at (last_action->action.insert.text, I) == ' ') ||
			(g_utf8_get_char_at (last_action->action.insert.text, I) == '\t')))
			)
		{
			last_action->mergeable = FALSE;
			return FALSE;
		}

		str = g_strdup_printf ("%s%s", last_action->action.insert.text, 
				undo_action->action.insert.text);
		
		g_free (last_action->action.insert.text);
		last_action->action.insert.length += undo_action->action.insert.length;
		last_action->action.insert.text = str;
		last_action->action.insert.chars += undo_action->action.insert.chars;

		return TRUE;
	} else {
		g_warning ("Unknown action inside undo merge encountered");
		return FALSE;
	}
}

void
undomgr_reset (UndoMgr *um) {
	undomgr_free_action_list(um);
	
	um->priv->can_undo = FALSE;
	g_signal_emit(G_OBJECT(um), undomgr_signals[CAN_UNDO], 0, FALSE);
	
	um->priv->can_redo = FALSE;
	g_signal_emit(G_OBJECT(um), undomgr_signals[CAN_REDO], 0, FALSE);
	
}
