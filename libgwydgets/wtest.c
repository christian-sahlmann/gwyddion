/* testovaci program pro 3d widget
   (c) Martin Siler
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libdraw/gwypalette.h>
#include <libdraw/gwypalettedef.h>

#include <gtk/gtkgl.h>

#include "gwy3dview.h"
#include "gwyglmaterial.h"

#define MAGIC "GWYO"
#define MAGIC_SIZE (sizeof(MAGIC)-1)


/*****************************************
  Global data - to be used in menu handlers
******************************************/
/*Gwy3DWidget * g3w = NULL;*/
GtkItemFactory *global_item_factory = NULL;
/*******************************************/

static GwyContainer*
gwyfile_load(const gchar *filename)
{
    GObject *object;
    GError *err = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < MAGIC_SIZE
        || memcmp(buffer, MAGIC, MAGIC_SIZE)) {
        g_warning("File %s doesn't seem to be a .gwy file", filename);
        if (!gwy_file_abandon_contents(buffer, size, &err)) {
            g_critical("%s", err->message);
            g_clear_error(&err);
        }
        return NULL;
    }

    object = gwy_serializable_deserialize(buffer + MAGIC_SIZE,
                                          size - MAGIC_SIZE, &pos);
    if (!gwy_file_abandon_contents(buffer, size, &err)) {
        g_critical("%s", err->message);
        g_clear_error(&err);
    }
    if (!object) {
        g_warning("File %s deserialization failed", filename);
        return NULL;
    }
    if (!GWY_IS_CONTAINER(object)) {
        g_warning("File %s contains some strange object", filename);
        g_object_unref(object);
        return NULL;
    }

    return (GwyContainer*)object;
}


static void gwy3D_ChangeMaterial (gpointer g3w, guint mat, GtkMenuItem  *menuitem)
{
    GwyGLMaterial * mat_current = (GwyGLMaterial*) mat;
    GwyGLMaterial * mat_none = gwy_glmaterial_get_by_name(GWY_GLMATERIAL_NONE);

    gtk_widget_set_sensitive(
             gtk_item_factory_get_item (global_item_factory, "/Move light"),
             mat_current != mat_none ? TRUE : FALSE);
    if (mat_current == mat_none)
       gtk_check_menu_item_set_active(
             GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (
                 global_item_factory, "/Rotation")), TRUE);
   gwy_debug("material: %d %d", mat_current, (guint) menuitem);
   gwy_3d_view_set_material(g3w, mat_current);
}

static void gwy3D_ChangeMovementStatus (gpointer g3w, guint mv, GtkMenuItem  *menuitem)
{
    g_return_if_fail(GWY_IS_3D_VIEW(g3w));
    gwy_3d_view_set_status(g3w, (Gwy3DMovement)mv);
}

static void gwy3D_ChangeOrthogonal (gpointer g3w, guint action, GtkMenuItem  *menuitem)
{
    g_return_if_fail(GWY_IS_3D_VIEW(g3w));
    gwy_3d_view_set_orthographic(g3w, !gwy_3d_view_get_orthographic(g3w));
}

static void gwy3D_ChangeShowAxes (gpointer g3w, guint action, GtkMenuItem  *menuitem)
{
    gboolean show;

    g_return_if_fail(GWY_IS_3D_VIEW(g3w));
    show = !gwy_3d_view_get_show_axes(g3w);
    gwy_3d_view_set_show_axes(g3w, show);

    gtk_widget_set_sensitive(gtk_item_factory_get_item (global_item_factory, "/Show labels"),
                              show);

}

static void gwy3D_ChangeShowLabels (gpointer g3w, guint action, GtkMenuItem  *menuitem)
{
    g_return_if_fail(GWY_IS_3D_VIEW(g3w));
    gwy_3d_view_set_show_labels(g3w, !gwy_3d_view_get_show_labels(g3w));
}


static void gwy3D_ResetView (gpointer g3w, guint action, GtkMenuItem  *menuitem)
{
    gwy_3d_view_reset_view(g3w);
}

static void gwy3D_Export(gpointer g3w, guint action, GtkMenuItem  *menuitem)
{

   GdkPixbuf * pixbuf;

   /*GtkWidget *dialog;
   char *filename;

   dialog = gtk_file_chooser_dialog_new ("Export File",
                                      window,
                                      GTK_FILE_CHOOSER_ACTION_SAVE,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                      NULL);

   if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT)
   {
      gtk_widget_destroy (dialog);
      return;
   }

   filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
*/
   g_return_if_fail(GTK_WIDGET_REALIZED(g3w));
   pixbuf = gwy_3d_view_get_pixbuf(g3w, 0, 0);
   gdk_pixbuf_save ( pixbuf, "pokus.png", "png", NULL, NULL);
   g_object_unref(pixbuf);
/*   g_free (filename);
   gtk_widget_destroy (dialog);
*/
}
/* For popup menu. */
static gboolean
button_press_event_popup_menu (GtkWidget      *widget,
			       GdkEventButton *event,
			       gpointer        data)
{
  if (event->button == 3)
    {
      /* Popup menu. */
      gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL,
		      event->button, event->time);
      return TRUE;
    }

  return FALSE;
}

struct IF_IFE {GtkItemFactory *item_factory; GtkItemFactoryEntry *mats; Gwy3DView *gwy3d;};

void MaterialsMenu(const gchar *name, GwyGLMaterial *glmaterial,  gpointer user_data)
 {
     GtkItemFactoryEntry *mats = ((struct IF_IFE*) user_data)->mats; 
     GtkItemFactory *item_factory = ((struct IF_IFE*) user_data)->item_factory;
     Gwy3DView *gwy3d = ((struct IF_IFE*) user_data)->gwy3d;

     gwy_debug("%s %s", name, mats->path );
     mats->callback_action = (guint) glmaterial;
     strcpy(mats->path + 11, name);
     gtk_item_factory_create_item(item_factory, mats, gwy3d, 1);
 }

/* Creates the popup menu.*/
static GtkWidget *
gwy3D_create_popup_menu (GtkWidget* window, Gwy3DView *gwy3d)
{
  static GtkItemFactoryEntry menu_items[] = {
    { "/tear",         NULL,         NULL,           0, "<Tearoff>"},
    { "/_Rotation",    "<CTRL>R", gwy3D_ChangeMovementStatus, GWY_3D_ROTATION,      "<RadioItem>"},
    { "/_Scale",       "<CTRL>S", gwy3D_ChangeMovementStatus, GWY_3D_SCALE,         "/Rotation" },
    { "/Scale _z-axis","<CTRL>Z", gwy3D_ChangeMovementStatus, GWY_3D_DEFORMATION,   "/Rotation" },
    { "/Move _light",  "<CTRL>L", gwy3D_ChangeMovementStatus, GWY_3D_LIGHTMOVEMENT, "/Rotation" },
    { "/Reset _view",  NULL,      gwy3D_ResetView,                 1,               "<Item>" },
    { "/sep01",        NULL,       NULL,                           0,               "<Separator>"},
    { "/_Orthogonal projection", "<CTRL>O",gwy3D_ChangeOrthogonal, 1,               "<CheckItem>" },
    { "/Show _axes",   "<CTRL>A",         gwy3D_ChangeShowAxes,    1,               "<CheckItem>" },
    { "/Show la_bels", "<CTRL>B",       gwy3D_ChangeShowLabels,    1,               "<CheckItem>" },
    { "/sep02",        NULL,       NULL,                           0,               "<Separator>"},
    { "/_Materials",        NULL,         NULL,           0, "<Branch>" },
    { "/Materials/None",     NULL, gwy3D_ChangeMaterial, (guint) 1,     "<RadioItem>" },
    { "/Materials/sep",      NULL, NULL,                 0,                     "<Separator>" },
    { "/sep03",             NULL,         NULL,           0, "<Separator>" },
    { "/_Export...",       "<CTRL>E",    gwy3D_Export,   1 , "<StockItem>", GTK_STOCK_SAVE_AS },
    { "/_Quit",            "<CTRL>Q",    gtk_main_quit,  0, "<StockItem>", GTK_STOCK_QUIT },
  };
  static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

  GtkItemFactoryEntry mats;
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;
  struct IF_IFE matif_e;


  menu_items[12].callback_action = (guint) gwy_glmaterial_get_by_name(GWY_GLMATERIAL_NONE);
  /* Make an accelerator group (shortcut keys) */
  accel_group = gtk_accel_group_new ();

  /* Make an ItemFactory (that makes a menubar) */
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU, "<gwy3Dmain>", accel_group);

  /* This function generates the menu items. Pass the item factory,
     the number of items in the array, the array itself, and any
     callback data for the the menu items. */
  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, gwy3d);

  /* Attach the new accelerator group to the window. */
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

  gwy_debug("");
  mats.path = g_strdup("/Materials/                                                        ");
  mats.accelerator =  NULL;
  mats.callback = gwy3D_ChangeMaterial;
  mats.item_type = g_strdup("/Materials/None");
  matif_e.item_factory = item_factory;
  matif_e.mats = &mats;
  matif_e.gwy3d = gwy3d;
  gwy_glmaterial_foreach(MaterialsMenu, &matif_e);
  g_free(mats.path);
  g_free(mats.item_type);

  global_item_factory = item_factory;

  if (gwy_3d_view_get_orthographic(gwy3d))
  {
    gtk_check_menu_item_set_active(
           GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Orthogonal projection")),
           TRUE);
  }
  if (gwy_3d_view_get_show_labels(gwy3d))
  {
     gtk_check_menu_item_set_active(
           GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Show labels")),
           TRUE);
  }
  if (gwy_3d_view_get_show_axes(gwy3d))
  {
    gtk_check_menu_item_set_active(
           GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Show axes")),
           TRUE);
  } else
     gtk_widget_set_sensitive(
           gtk_item_factory_get_item (item_factory, "/Show labels"),
           FALSE);

  if (gwy3d->mat_current == gwy_glmaterial_get_by_name(GWY_GLMATERIAL_NONE))
     gtk_widget_set_sensitive(
           gtk_item_factory_get_item (item_factory, "/Move light"),
           FALSE);

  gtk_check_menu_item_set_active(
           GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory, "/Materials/None")),
           TRUE);

  /* Finally, return the actual menu bar created by the item factory. */
  return gtk_item_factory_get_widget (item_factory, "<gwy3Dmain>");

}


int
main (int argc, char *argv[])
{
  GtkWidget *window;

  GwyContainer * cont;
  GtkWidget *gwy3D;

  GtkWidget *vbox;
  GtkWidget *menu;

  /*GdkGLConfig *glconfig = NULL;
*/
  gtk_init (&argc, &argv);
/***********************************************************************************/
  gtk_gl_init (&argc, &argv);

/*********************************************************************************/
  if (argc <= 1) return -1;

  g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (window), "Gwy3D widget test");
  gtk_container_set_reallocate_redraws (GTK_CONTAINER (window), TRUE);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_exit), NULL);


  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show(vbox);

  gwy_widgets_type_init();
  gwy_glmaterial_setup_presets();
  gwy_palette_def_setup_presets();
  
  cont  = gwyfile_load(argv[1]);
  gwy3D = gwy_3d_view_new(cont);


  gtk_container_add (GTK_CONTAINER (vbox), gwy3D);
  gtk_widget_show (gwy3D);

  menu = gwy3D_create_popup_menu (window, GWY_3D_VIEW(gwy3D));

  /*g3w = GWY_3d_view(gwy3D);*/
  /* Signal handler */
  g_signal_connect_swapped(gwy3D, "button-press-event",
                           G_CALLBACK(button_press_event_popup_menu), menu);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
