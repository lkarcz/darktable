/*
    This file is part of darktable,
    copyright (c) 2012 Jose Carlos Garcia Sogo.
	based on keywords.c

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "common/darktable.h"
#include "common/debug.h"
#include "common/tags.h"
#include "common/collection.h"
#include "common/utility.h"
#include "control/conf.h"
#include "control/control.h"

#include "libs/lib.h"

DT_MODULE(1)

typedef struct dt_lib_folders_t
{
  GtkTreeStore *store;
  GtkTreeView *view;
}
dt_lib_folders_t;


/* callback for drag and drop */
/*static void _lib_keywords_drag_data_received_callback(GtkWidget *w,
					  GdkDragContext *dctx,
					  guint x,
					  guint y,
					  GtkSelectionData *data,
					  guint info,
					  guint time,
					  gpointer user_data);
*/
/* set the data for drag and drop, eg the treeview path of drag source */
/*static void _lib_keywords_drag_data_get_callback(GtkWidget *w,
						 GdkDragContext *dctx,
						 GtkSelectionData *data,
						 guint info,
						 guint time,
						 gpointer user_data);
*/
/* add keyword to collection rules */
/*static void _lib_keywords_add_collection_rule(GtkTreeView *view, GtkTreePath *tp,
					      GtkTreeViewColumn *tvc, gpointer user_data);
*/
const char* name()
{
  return _("folders");
}


uint32_t views()
{
  return DT_VIEW_LIGHTTABLE;
}

uint32_t container()
{
  return DT_UI_CONTAINER_PANEL_LEFT_CENTER;
}

int position()
{
  return 410;
}

void init_key_accels(dt_lib_module_t *self)
{
  //  dt_accel_register_lib(self, NC_("accel", "scan for devices"), 0, 0);
}

void connect_key_accels(dt_lib_module_t *self)
{
  /*  dt_lib_import_t *d = (dt_lib_import_t*)self->data;

  dt_accel_connect_button_lib(self, "scan for devices",
                              GTK_WIDGET(d->scan_devices)); */
}

static int
_count_images(const char *path)
{
  sqlite3_stmt *stmt = NULL;
  gchar query[1024] = {0};
  int count = 0;

  snprintf(query, 1024, "select count(id) from images where film_id in (select id from film_rolls where folder like '%s%s')", path, "%");
  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), query, -1, &stmt, NULL);
  if (sqlite3_step(stmt) == SQLITE_ROW)
    count = sqlite3_column_int(stmt, 0); 
  sqlite3_finalize(stmt);

  return count;
}

static GtkTreeStore *
_folder_tree (sqlite3_stmt *stmt)
{
  GtkTreeStore *store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
  //GtkTreeStore *store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_INT);

  GVolumeMonitor *gv_monitor;
  gv_monitor = g_volume_monitor_get ();

  GList *gv_list, *gd_list;
  gd_list = g_volume_monitor_get_connected_drives(gv_monitor);
  gv_list = g_volume_monitor_get_volumes(gv_monitor);

  if(gv_list && gd_list)
   fprintf(stderr, g_volume_get_name(g_list_nth_data(gv_list,0)));
   g_drive_get_name(g_list_nth_data(gd_list,0));

  // initialize the model with the paths

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    if(strchr((const char *)sqlite3_column_text(stmt, 2),'/')==0)
    {
      // Do nothing here
    }
    else
    {
      int level = 0;
      char *value;
      GtkTreeIter current,iter;
      char *path = g_strdup((char *)sqlite3_column_text(stmt, 2));
      char *pch = strtok((char *)sqlite3_column_text(stmt, 2),"/");
      while (pch != NULL) 
      {
        gboolean found=FALSE;
        int children = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store),level>0?&current:NULL);
        /* find child with name, if not found create and continue */
        for (int k=0;k<children;k++)
        {
          if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, level>0?&current:NULL, k))
          {
            gtk_tree_model_get (GTK_TREE_MODEL(store), &iter, 0, &value, -1);
        
            if (strcmp(value, pch)==0)
            {
              current = iter;
              found = TRUE;
              break;
            }
          }
        }

        /* lets add new path and assign current */
        if (!found)
        {
          const char *pth = g_strndup (path, strstr(path, pch)-path);
          const char *pth2 = g_strconcat(pth, pch, NULL);
          //strstr(path, pch)[0]='\0';

          int count = _count_images(pth2);
          gtk_tree_store_insert(store, &iter, level>0?&current:NULL,0);
          gtk_tree_store_set(store, &iter, 0, pch, 1, pth2, 2, count, -1);
          //gtk_tree_store_set(store, &iter, 0, pch, 1, count, -1);
          current = iter;
        }

        level++;
        pch = strtok(NULL, "/");
      } 
    }  
  }

  return (store);
}


static GtkTreeModel *
_create_filtered_tree (GtkTreeModel *model, gchar *mount_path)
{
  GtkTreeModel *filter;
  GtkTreePath  *path;
    
  /* Create path to set as virtual root */
  
  GtkTreeIter current, iter;
  char *pch = strtok(mount_path, "/");
  char *value;
  int level = 0;
  gboolean found = FALSE;

  while (pch != NULL)
  {
    found=FALSE;
    int children = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model),level>0?&current:NULL);
    /* find child with name, if not found create and continue */
    for (int k=0;k<children;k++)
    {
      if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(model), &iter, level>0?&current:NULL, k))
      {
        gtk_tree_model_get (GTK_TREE_MODEL(model), &iter, 0, &value, -1);
        
        if (strcmp(value, pch)==0)
        {
          current = iter;
          found = TRUE;
          break;
        }
      }
    }

    if (!found) break;
    level++;
    pch = strtok(NULL, "/");
  }

  if (!found) return NULL;

  path = gtk_tree_model_get_path (GTK_TREE_MODEL(model), &iter);

  /* Create filter and set virtual root */
  filter = gtk_tree_model_filter_new (model, path);
  gtk_tree_path_free (path);

  return (filter);
                             
  /* Create treeview with model */
//  treeview = gtk_tree_view_new_with_model (filter);
                                      
  /* Create display components of tree view */
//  create_treeview_display (GTK_TREE_VIEW(treeview));
                                               
//  return (treeview); 
}

static GtkTreeView *
_create_treeview_display (GtkTreeModel *model)
{
  GtkTreeView *treeview;
  GtkCellRenderer *renderer, *renderer2;

  treeview = GTK_TREE_VIEW(gtk_tree_view_new ());

  renderer = gtk_cell_renderer_text_new ();
  renderer2 = gtk_cell_renderer_text_new ();
/*  GtkTreeViewColumn *column1, *column2;
  GtkCellRenderer *renderer, *renderer2;

  renderer = gtk_cell_renderer_text_new ();
  renderer2 = gtk_cell_renderer_text_new ();
 
  column1 = gtk_tree_view_column_new_with_attributes ("", renderer,
                                            "sizing", GTK_TREE_VIEW_COLUMN_FIXED,
                                            "expand", TRUE,
                                            NULL);

  column2 = gtk_tree_view_column_new_with_attributes("", renderer2,
                                           "sizing", GTK_TREE_VIEW_COLUMN_FIXED,
                                           "min-width", 10,
                                           NULL);
  gtk_tree_view_insert_column (tree, column1, 0);
  gtk_tree_view_insert_column (tree, column2, 1); */

  gtk_tree_view_insert_column_with_attributes(treeview, -1, "", renderer,
                                               "text", 0,
                                                NULL);

  gtk_tree_view_insert_column_with_attributes(treeview, -1, "", renderer2,
                                               "text", 2,
                                                NULL);

  gtk_tree_view_set_level_indentation (treeview, 1);

  gtk_tree_view_set_headers_visible(treeview, FALSE);

  gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(model));

  /* free store, treeview has its own storage now */
  g_object_unref(model);

  return (treeview);
}

static void _lib_folders_string_from_path(char *dest,size_t ds, 
					   GtkTreeModel *model, 
					   GtkTreePath *path)
{
  g_assert(model!=NULL);
  g_assert(path!=NULL);

  GList *components = NULL;
  GtkTreePath *wp = gtk_tree_path_copy(path);
  GtkTreeIter iter;

  /* get components of path */
  while (1)
  {
    GValue value;
    memset(&value,0,sizeof(GValue));

    /* get iter from path, break out if fail */
    if (!gtk_tree_model_get_iter(model, &iter, wp))
      break;

    /* add component to begin of list */
    gtk_tree_model_get_value(model, &iter, 0, &value);
    if ( !(gtk_tree_path_get_depth(wp) == 0))
    {
      components = g_list_insert(components, 
				 g_strdup(g_value_get_string(&value)), 
				 0);
    }
    g_value_unset(&value);

    /* get parent of current path break out if we are at root */
//    if (!gtk_tree_path_up(wp) || gtk_tree_path_get_depth(wp) == 0)
    if (!gtk_tree_path_up(wp))
      break;
  }

  /* build the tag string from components */
  int dcs = 0;

  if(g_list_length(components) == 0) 
    dcs += g_snprintf(dest+dcs, ds-dcs," ");
  else
    dcs += g_snprintf(dest+dcs, ds-dcs,"/");
		      
  for(int i=0;i<g_list_length(components);i++) 
  {
    dcs += g_snprintf(dest+dcs, ds-dcs,
		      "%s%s",
		      (gchar *)g_list_nth_data(components, i),
		      (i < g_list_length(components)-1) ? "/" : "%");
  }
  
  /* free data */
  gtk_tree_path_free(wp);
  

}

static void _lib_folders_update_collection(GtkTreeView *view, GtkTreePath *tp)
{
  
  char folder[1024]={0};
  _lib_folders_string_from_path(folder, 1024, gtk_tree_view_get_model(view), tp);
  
  gchar *complete_query = NULL;

  complete_query = dt_util_dstrcat(complete_query, "film_id in (select id from film_rolls where folder like '%s')", folder);
  
  dt_collection_set_extended_where(darktable.collection, complete_query);

  dt_collection_set_query_flags(darktable.collection, (dt_collection_get_query_flags (darktable.collection) | COLLECTION_QUERY_USE_WHERE_EXT));

//  dt_collection_set_query_flags(darktable.collection, (dt_collection_get_query_flags (darktable.collection) & ~COLLECTION_QUERY_USE_LIMIT));

  dt_collection_set_filter_flags (darktable.collection, (dt_collection_get_filter_flags (darktable.collection) & ~COLLECTION_FILTER_FILM_ID));

  dt_collection_update(darktable.collection);

  g_free(complete_query);


  // remove from selected images where not in this query.
  sqlite3_stmt *stmt = NULL;
  const gchar *cquery = dt_collection_get_query(darktable.collection);
  complete_query = NULL;
  if(cquery && cquery[0] != '\0')
  {
    complete_query = dt_util_dstrcat(complete_query, "delete from selected_images where imgid not in (%s)", cquery);
    DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), complete_query, -1, &stmt, NULL);
    DT_DEBUG_SQLITE3_BIND_INT(stmt, 1, 0);
    DT_DEBUG_SQLITE3_BIND_INT(stmt, 2, -1);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    /* free allocated strings */
    g_free(complete_query);
  }

  dt_control_queue_redraw_center();
   
  /* raise signal of collection change, only if this is an orginal */
  if (!darktable.collection->clone)
    dt_control_signal_raise(darktable.signals, DT_SIGNAL_COLLECTION_CHANGED);
}

static void
tree_row_activated (GtkTreeView *view, GtkTreePath *path, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = NULL;
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  if(!gtk_tree_selection_get_selected(selection, &model, &iter)) return;
  gchar *text;
  gtk_tree_model_get (model, &iter, 1, &text, -1);
//loop trhough all selected rows
  //entry_key_press();
 
  _lib_folders_update_collection(view, path);

  dt_control_queue_redraw_center(); 
}

void gui_init(dt_lib_module_t *self)
{
  /* initialize ui widgets */
  dt_lib_folders_t *d = (dt_lib_folders_t *)g_malloc(sizeof(dt_lib_folders_t));
  memset(d,0,sizeof(dt_lib_folders_t));
  self->data = (void *)d;
  self->widget = gtk_vbox_new(FALSE, 5);

  GtkTreeStore *store = gtk_tree_store_new(1, G_TYPE_STRING);

  /* intialize the tree store */
  GtkBox *box_tree; 
  char query[1024]; 
  sqlite3_stmt *stmt; 
  snprintf(query, 1024, "select * from film_rolls"); 
  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), query, -1, &stmt, NULL); 
                      
  //Populate the tree  
  store = _folder_tree (stmt); 
  sqlite3_finalize(stmt); 
  
  /* set the UI */
  GtkTreeView *tree; 
  box_tree = GTK_BOX(gtk_vbox_new(FALSE,5)); 
           
  GVolumeMonitor *gv_monitor;
  gv_monitor = g_volume_monitor_get ();

  GList *gm_list;
  gm_list = g_volume_monitor_get_mounts(gv_monitor);

  GtkWidget *button;
  // Add a button for local filesystem, to keep UI consistency
  button = gtk_button_new_with_label (_("Local HDD"));
  gtk_container_add(GTK_CONTAINER(box_tree), GTK_WIDGET(button));

  tree = _create_treeview_display(GTK_TREE_MODEL(store));
  gtk_container_add(GTK_CONTAINER(box_tree), GTK_WIDGET(tree));

  GtkTreeModel *model;

  for (int i=0;i<g_list_length(gm_list);i++)
  {
    GMount *mount;
    GFile *file;

    mount = (GMount *)g_list_nth_data(gm_list, i);
    file = g_mount_get_root(mount);
    
    /* TODO: We should only add devices in which we already have filmrolls imported */
    /* TODO: Pack a gtkbox and a gtktreeview together with a button, to show that part of the tree */

    model = _create_filtered_tree (GTK_TREE_MODEL(store), g_file_get_path(file));

    if (!model)
    {
      button = gtk_button_new_with_label (g_mount_get_name(mount));
      gtk_container_add(GTK_CONTAINER(box_tree), GTK_WIDGET(button));

      tree = _create_treeview_display(GTK_TREE_MODEL(model));

      gtk_container_add(GTK_CONTAINER(box_tree), GTK_WIDGET(tree));
    }
  }

  gtk_box_pack_start(GTK_BOX(self->widget), GTK_WIDGET(box_tree), TRUE, TRUE, 0); 
  g_signal_connect(G_OBJECT (tree), "row-activated", G_CALLBACK (tree_row_activated), d); 
         
  
  gtk_widget_show_all(GTK_WIDGET(d->view));
  
}

void gui_cleanup(dt_lib_module_t *self)
{ 
  // dt_lib_import_t *d = (dt_lib_import_t*)self->data;

  /* cleanup mem */
  g_free(self->data);
  self->data = NULL;
}


