/* logjam - a GTK client for LiveJournal.
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <livejournal/livejournal.h>
#include <livejournal/gettags.h>

#include "account.h"
#include "tags.h"

static void
tags_hash_list_cb (gpointer key, LJTag *t, GSList **list)
{
  *list = g_slist_append(*list, t);
}

static gboolean
load_tags (GtkWindow *parent, JamAccountLJ *acc, gchar *journal, GSList **l)
{
  LJGetTags *gettags;
  NetContext *ctx;

  ctx = (NetContext *) net_ctx_gtk_new (parent, _("Loading Tags"));
  gettags = lj_gettags_new (jam_account_lj_get_user (acc), journal);
  if (!net_run_verb_ctx ((LJVerb*) gettags, ctx, NULL)) {
      lj_gettags_free (gettags, TRUE);
      net_ctx_gtk_free (ctx);
      return FALSE;
  }

  g_hash_table_foreach (gettags->tags, (GHFunc) tags_hash_list_cb, l);

  lj_gettags_free (gettags, FALSE);
  net_ctx_gtk_free (ctx);

  return TRUE;
}

static void
tag_toggled (GtkCellRendererToggle *cell,
	     gchar                 *path_str,
	     gpointer               data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreeIter  iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gboolean tag;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &tag, -1);

  tag ^= 1;

  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, tag, -1);

  gtk_tree_path_free (path);
}

gboolean
create_tag_string (GtkTreeModel *model,
		   GtkTreePath  *path,
		   GtkTreeIter  *iter,
		   gpointer     data)
{
  gchar *tag;
  gboolean is_sel;
  gchar **tagstr = (gchar **) data;

  gtk_tree_model_get (model, iter, 0, &is_sel, -1);
  if (is_sel) {
      gchar *buf = *tagstr;
      gtk_tree_model_get (model, iter, 1, &tag, -1);
      *tagstr = g_strconcat (buf, tag, ", ", NULL);
      g_free (buf);
  }

  return FALSE;
}

static void
tags_set_fg(GtkTreeViewColumn *column, GtkCellRenderer *cell,
	    GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  GSList *prev = (GSList *) data;
  gchar *text;

  gtk_tree_model_get(model, iter, 1, &text, -1);
  if (prev && g_slist_find_custom(prev, text, (GCompareFunc) g_utf8_collate))
    g_object_set(G_OBJECT(cell), "foreground", "Red", NULL);
  else
    g_object_set(G_OBJECT(cell), "foreground", "Black", NULL);
  g_free(text);
}

GtkWidget *
taglist_create (GSList *l, GSList **head)
{
  GtkWidget *treeview;
  GtkListStore *store;
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GSList *prev = *head, *p, *q;

  /* create model */
  store = gtk_list_store_new (2, G_TYPE_BOOLEAN, G_TYPE_STRING);

	for (q = l; q; q = g_slist_next(q)) {
		LJTag *t = (LJTag *)q->data;
		gboolean check = FALSE;

      if (prev && (p = g_slist_find_custom(prev, t->tag, (GCompareFunc) g_utf8_collate)) != NULL) {
        check = TRUE;
        prev = g_slist_remove_link(prev, p);
        g_free(p->data);
        g_slist_free1(p);
      }
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
			  0, check,
			  1, t->tag,
			  -1);
	}

  /* rest of the typed tags */
  for (p = prev; p; p = g_slist_next(p)) {
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
		  0, TRUE,
		  1, p->data,
		  -1);
  }

  /* create treeview */
  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview), 1);

  /* add columns */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled",
		    G_CALLBACK (tag_toggled),
		    GTK_TREE_MODEL (store));
  column = gtk_tree_view_column_new_with_attributes ("#",
						     renderer, "active", 0,
						     NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Tag name"),
						     renderer, "text", 1,
						     NULL);

  gtk_tree_view_column_set_cell_data_func(column, renderer, tags_set_fg,
					  prev, NULL);

  gtk_tree_view_column_set_sort_column_id (column, 1);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

  /* update head of the prev list */
  *head = prev;
  g_object_unref (store);

  return treeview;
}

static GSList *
tags_split(gchar *typed) {
  GSList *head = NULL;
  gchar **result, **p;

  if (typed == NULL || *typed == '\0')
    return NULL;

  for (p = result = g_strsplit(typed, ",", 0); *p; p++) {
    gchar *token = g_strstrip(*p);
    if (*token)
      head = g_slist_prepend(head, g_utf8_strdown(token, -1));
  }
  g_strfreev(result);
  return head;
}

gchar *
tags_dialog (GtkWidget *win, JamAccountLJ *acc, gchar *journal, gchar *typed)
{
  GtkWidget *dlg, *sw, *tv;
  GSList *list = NULL;
  gchar *taglist = NULL;
  GSList *prev;

  if (acc == NULL) return NULL;

  load_tags (GTK_WINDOW (win), acc, journal, &list);

  dlg = gtk_dialog_new_with_buttons (_("Select tags"),
                                     GTK_WINDOW (win),
                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_STOCK_OK,
                                     GTK_RESPONSE_OK,
                                     GTK_STOCK_CLOSE,
                                     GTK_RESPONSE_CLOSE,
                                     NULL);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), sw, TRUE, TRUE, 0);

  prev = tags_split(typed);
  tv = taglist_create (list, &prev);
  gtk_container_add (GTK_CONTAINER (sw), tv);

  gtk_window_resize(GTK_WINDOW(dlg), 60, 210);
  gtk_widget_show_all (sw);

  if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK) {
      taglist = g_strdup ("");
      gtk_tree_model_foreach (gtk_tree_view_get_model(GTK_TREE_VIEW(tv)),
			      create_tag_string,
			      &taglist);
      if (g_ascii_strcasecmp (taglist, "") == 0) {
	  g_free (taglist);
	  taglist = NULL;
      }
  }

  gtk_widget_destroy (dlg);

  /* free rest of prev list */
  if (prev) {
    GSList *p;
    for (p = prev; p; p = g_slist_next(p)) {
      g_free(p->data);
    }
    g_slist_free(prev);
  }

  return taglist;
}
