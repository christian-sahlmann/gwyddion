/*
 *  Copyright (C) 2008 Jan Horak
 *  E-mail: xhorak@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 *
 *  Description: This file contains pygwy console module.
 */


#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include "pygwy-console.h"
#include "pygwy.h"
#include "config.h"

#ifdef HAVE_GTKSOURCEVIEW
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcelanguagemanager.h>
#endif

extern gchar pygwy_plugin_dir_name[];

static PygwyConsoleSetup *s_console_setup = NULL;
static void       pygwy_on_console_save_as_file      (GtkToolButton *btn, gpointer user_data);
static void       pygwy_console_run                  (GwyContainer *data,
                                                      GwyRunType run,
                                                      const gchar *name);
static void             pygwy_on_console_command_execute(GtkEntry *entry, 
                                                         gpointer user_data);
static gboolean         pygwy_on_console_close(GtkWidget *widget, 
                                               GdkEvent *event, 
                                               gpointer user_data);

void
pygwy_register_console()
{

    if (gwy_process_func_register(N_("pygwy_console"),
                                  pygwy_console_run,
                                  N_("/Pygwy console"),
                                  NULL,
                                  GWY_RUN_IMMEDIATE,
                                  GWY_MENU_FLAG_DATA,
                                  N_("Python wrapper console")) ) {

    }
}

char *
pygwy_console_run_command(gchar *cmd, int mode)
{
   if (!cmd) {
      g_warning("No command.");
      return NULL;
   }

   if (!s_console_setup) {
      g_warning("Console setup structure is not defined!");
      return NULL;
   }
   // store _stderr_redir location
   pygwy_run_string(cmd, 
         mode, 
         s_console_setup->dictionary, 
         s_console_setup->dictionary);
   pygwy_run_string("_stderr_redir_pos = _stderr_redir.tell()\n"
                    "_stderr_redir.seek(0)\n"
                    "_stderr_redir_string = _stderr_redir.read(_stderr_redir_pos)\n"
                    "_stderr_redir.seek(0)",
         Py_file_input, 
         s_console_setup->dictionary,
         s_console_setup->dictionary);

   return PyString_AsString( PyDict_GetItemString(
            s_console_setup->dictionary, 
            "_stderr_redir_string") );
}

static void
pygwy_console_append(gchar *msg)
{
   GtkTextBuffer *console_buf;
   GtkTextIter start_iter, end_iter;
   GString *output;
   GtkTextMark *end_mark;

   if (!msg) {
      g_warning("No message to append.");
      return;
   }
   if (!s_console_setup) {
      g_warning("Console setup structure is not defined!");
      return;
   }
   // read string which contain last command output
   console_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(s_console_setup->console_output));
   gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(console_buf), &start_iter, &end_iter);

   // get output widget content
   output = g_string_new (gtk_text_buffer_get_text(console_buf, &start_iter, &end_iter, FALSE));

   // append input line
   output = g_string_append(output, msg);
   gtk_text_buffer_set_text (GTK_TEXT_BUFFER (console_buf), output->str, -1);
   g_string_free(output, TRUE);

   // scroll to end
   gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(console_buf), &end_iter);
   end_mark = gtk_text_buffer_create_mark(console_buf, "cursor", &end_iter, FALSE);
   g_object_ref(end_mark);
   gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(s_console_setup->console_output), 
         end_mark, 0.0, FALSE, 0.0, 0.0);
   g_object_unref(end_mark);

}

static void
pygwy_on_console_run_file(GtkToolButton *btn, gpointer user_data)
{
   GtkTextIter start_iter, end_iter;
   char *output, *file_info_line;

   GtkTextBuffer *console_file_buf = 
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(s_console_setup->console_file_content));

   file_info_line = g_strdup_printf(">>> Running file content of below textfield\n");

   pygwy_console_append(file_info_line);

   gtk_text_buffer_get_bounds(console_file_buf, &start_iter, &end_iter);
   output = pygwy_console_run_command(
         gtk_text_buffer_get_text(console_file_buf, &start_iter, &end_iter, FALSE), 
         Py_file_input);
   pygwy_console_append(output);

   // get output widget content
   gtk_text_buffer_get_text(console_file_buf, &start_iter, &end_iter, FALSE);

}


static void
pygwy_on_console_open_file(GtkToolButton *btn, gpointer user_data)
{
   GtkWidget *file_chooser;
   GtkFileFilter *filter = gtk_file_filter_new();
   GtkTextBuffer *console_file_buf;
   gtk_file_filter_add_mime_type(filter, "text/x-python");

   file_chooser = gtk_file_chooser_dialog_new(N_("Open Python script"), 
         NULL, 
         GTK_FILE_CHOOSER_ACTION_OPEN,
         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
         NULL);
   gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(file_chooser), filter);
   if (gtk_dialog_run (GTK_DIALOG (file_chooser)) == GTK_RESPONSE_ACCEPT)
   {
      char *file_content;
      GError *err = NULL;
      

      if (s_console_setup->script_filename) {
         g_free(s_console_setup->script_filename);
      }

      s_console_setup->script_filename = gtk_file_chooser_get_filename (
         GTK_FILE_CHOOSER (file_chooser));
      if (!g_file_get_contents(s_console_setup->script_filename,
                            &file_content,
                            NULL,
                            &err)) {
         g_warning("Cannot read content of file '%s'", s_console_setup->script_filename);
         g_free(s_console_setup->script_filename);
         return;
      }
      
      // read string which contain last command output
      console_file_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(s_console_setup->console_file_content));

      // append input line
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (console_file_buf), file_content, -1);

      g_free(file_content);
   }
   gtk_widget_destroy (GTK_WIDGET(file_chooser));
}

static void
pygwy_on_console_save_file(GtkToolButton *btn, gpointer user_data)
{
   GtkTextBuffer *buf;
   GtkTextIter start_iter, end_iter;
   GString *output;
   FILE *f;

   if (s_console_setup->script_filename == NULL) {
      pygwy_on_console_save_as_file(btn, user_data);
   } else {
      buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(s_console_setup->console_file_content));
      gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(buf), &start_iter, &end_iter);
      output = g_string_new (gtk_text_buffer_get_text(buf, &start_iter, &end_iter, FALSE));
      f = fopen(s_console_setup->script_filename, "w");
      fwrite(output->str, 1, output->len, f);
      fclose(f);
   }
}   

static void
pygwy_on_console_save_as_file(GtkToolButton *btn, gpointer user_data)
{
   GtkWidget *dialog;

   dialog = gtk_file_chooser_dialog_new ("Save File as",
				      NULL,
				      GTK_FILE_CHOOSER_ACTION_SAVE,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      NULL);
   gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

   //gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), default_folder_for_saving);
   gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "Untitled document");

   if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
      s_console_setup->script_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      pygwy_on_console_save_file(btn, user_data);
   }
   gtk_widget_destroy (dialog);
}   


static void 
pygwy_console_create_gui()
{
   GtkWidget *console_win, *vbox1, *console_scrolledwin, *file_scrolledwin, *vpaned, *frame;
   GtkWidget *entry_input, *button_bar, *button_open, *button_run, *button_save, *button_save_as;
   PangoFontDescription *font_desc;
   GtkAccelGroup *accel_group;
   GtkTooltips *button_bar_tips;
   



#ifdef HAVE_GTKSOURCEVIEW     
   GtkSourceLanguageManager *manager;
#endif

   // create static structure;
   s_console_setup = g_malloc(sizeof(PygwyConsoleSetup));
   // create GUI
   console_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title (GTK_WINDOW (console_win), "Pygwy Console");

   vbox1 = gtk_vbox_new (FALSE, 0);
   gtk_container_add (GTK_CONTAINER (console_win), vbox1);

   // buttons
   button_open = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_OPEN));
   button_save = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_SAVE));
   button_save_as = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_SAVE_AS));
   button_run = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_EXECUTE));
   button_bar_tips = gtk_tooltips_new ();
   gtk_tooltips_set_tip (GTK_TOOLTIPS (button_bar_tips), 
                         button_open, N_("Open script in Python language (Ctrl-O)"), "");
   gtk_tooltips_set_tip (GTK_TOOLTIPS (button_bar_tips), 
                         button_save, N_("Save script (Ctrl-S)"), "");
   gtk_tooltips_set_tip (GTK_TOOLTIPS (button_bar_tips), 
                         button_run, N_("Execute script (Ctrl-E)"), "");
   accel_group = gtk_accel_group_new ();
   gtk_widget_add_accelerator (button_run, "clicked", accel_group,
                              GDK_E, (GdkModifierType) GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator (button_open, "clicked", accel_group,
                              GDK_O, (GdkModifierType) GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator (button_save, "clicked", accel_group,
                              GDK_S, (GdkModifierType) GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
     gtk_window_add_accel_group(GTK_WINDOW (console_win), accel_group);



   button_bar = gtk_toolbar_new();
   gtk_toolbar_insert(GTK_TOOLBAR(button_bar), GTK_TOOL_ITEM(button_run), 0);
   gtk_toolbar_insert(GTK_TOOLBAR(button_bar), GTK_TOOL_ITEM(button_save_as), 0);
   gtk_toolbar_insert(GTK_TOOLBAR(button_bar), GTK_TOOL_ITEM(button_save), 0);
   gtk_toolbar_insert(GTK_TOOLBAR(button_bar), GTK_TOOL_ITEM(button_open), 0);
   gtk_box_pack_start(GTK_BOX(vbox1), button_bar, FALSE, FALSE, 0);
   gtk_toolbar_set_style(GTK_TOOLBAR(button_bar), GTK_TOOLBAR_BOTH);

   // window
   vpaned = gtk_vpaned_new();
   gtk_box_pack_start (GTK_BOX (vbox1), vpaned, TRUE, TRUE, 0);
   file_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
   gtk_paned_pack1(GTK_PANED(vpaned), file_scrolledwin, TRUE, FALSE);
   //gtk_box_pack_start (GTK_BOX (vbox1), file_scrolledwin, TRUE, TRUE, 0);
   gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (file_scrolledwin), GTK_SHADOW_IN);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_scrolledwin), 
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
   console_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(console_scrolledwin), 
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
   gtk_paned_pack2(GTK_PANED(vpaned), console_scrolledwin, TRUE, TRUE);
   //gtk_box_pack_start (GTK_BOX (vbox1), console_scrolledwin, TRUE, TRUE, 0);
   gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (console_scrolledwin), GTK_SHADOW_IN);


   // console output
   s_console_setup->console_output = gtk_text_view_new ();
   gtk_container_add (GTK_CONTAINER (console_scrolledwin), s_console_setup->console_output);
   gtk_text_view_set_editable (GTK_TEXT_VIEW (s_console_setup->console_output), FALSE);

   // file buffer
#ifdef HAVE_GTKSOURCEVIEW  
   s_console_setup->console_file_content = gtk_source_view_new();
   gtk_source_view_set_show_line_numbers(
        GTK_SOURCE_VIEW(s_console_setup->console_file_content), TRUE);
   gtk_source_view_set_auto_indent(
        GTK_SOURCE_VIEW(s_console_setup->console_file_content), TRUE);
   manager = gtk_source_language_manager_get_default();

   gtk_source_buffer_set_language(
        GTK_SOURCE_BUFFER(
            gtk_text_view_get_buffer(
                 GTK_TEXT_VIEW(s_console_setup->console_file_content)
        )),
        gtk_source_language_manager_get_language(manager, "python")
   );
   gtk_source_buffer_set_highlight_syntax(
            GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(
                 GTK_TEXT_VIEW(s_console_setup->console_file_content))), TRUE);

#else
   s_console_setup->console_file_content = gtk_text_view_new();
#endif
   // set font
   font_desc = pango_font_description_from_string("Monospace 8");
   gtk_widget_modify_font(s_console_setup->console_file_content, font_desc);
   gtk_widget_modify_font(s_console_setup->console_output, font_desc);
   pango_font_description_free(font_desc);

   gtk_container_add (GTK_CONTAINER (file_scrolledwin), s_console_setup->console_file_content);
   gtk_text_view_set_editable (GTK_TEXT_VIEW (s_console_setup->console_file_content), TRUE);
   frame = gtk_frame_new(N_("Command"));
   entry_input = gtk_entry_new ();
   gtk_container_add(GTK_CONTAINER(frame), entry_input);
   gtk_box_pack_start (GTK_BOX (vbox1), frame, FALSE, FALSE, 0);
   gtk_entry_set_invisible_char (GTK_ENTRY (entry_input), 9679);
   gtk_widget_grab_focus(GTK_WIDGET(entry_input));
   gtk_paned_set_position(GTK_PANED(vpaned), 300);

   // entry widget on ENTER
   g_signal_connect ((gpointer) entry_input, "activate",
         G_CALLBACK (pygwy_on_console_command_execute),
         NULL);
   // open script signal connect
   g_signal_connect ((gpointer) button_open, "clicked",
         G_CALLBACK (pygwy_on_console_open_file),
         NULL);
   g_signal_connect ((gpointer) button_run, "clicked",
         G_CALLBACK (pygwy_on_console_run_file),
         NULL);
   g_signal_connect ((gpointer) button_save, "clicked",
         G_CALLBACK (pygwy_on_console_save_file),
         NULL);
   g_signal_connect ((gpointer) button_save_as, "clicked",
         G_CALLBACK (pygwy_on_console_save_as_file),
         NULL);

   // connect on window close()
   g_signal_connect ((gpointer) console_win, "delete_event",
         G_CALLBACK (pygwy_on_console_close),
         NULL);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(s_console_setup->console_output), GTK_WRAP_WORD_CHAR);
   gtk_window_resize(GTK_WINDOW(console_win), 600, 500);
   gtk_widget_show_all(console_win);
}

static void
pygwy_console_run(GwyContainer *data, GwyRunType run, const gchar *name)
{
    PyObject *d; //, *py_container;
    gchar *plugin_dir_name = NULL;
    
    pygwy_initialize();
    pygwy_console_create_gui();
    s_console_setup->script_filename = NULL;
    // create new environment    
    d = create_environment("__console__", FALSE);
    if (!d) {
        g_warning("Cannot create copy of Python dictionary.");
        return;
    }

    // do NOT create container named 'data' to allow access the container from python
    // py_container = pygobject_new((GObject*)data);
    //if (!py_container) {
    //    g_warning("Variable 'gwy.data' was not inicialized.");
    //}
    //PyDict_SetItemString(s_pygwy_dict, "data", py_container);
    
    // redirect stdout & stderr to temporary file
    pygwy_run_string("import sys, gwy, tempfile\n"
                     "from gwy import *\n"
                     "_stderr_redir = tempfile.TemporaryFile()\n"
                     "sys.stderr = _stderr_redir\n"
                     "sys.stdout = _stderr_redir\n",
                     Py_file_input,
                     d,
                     d);
    gwy_find_self_dir("data");

    // add .gwyddion/pygwy to sys.path
    plugin_dir_name = g_build_filename(gwy_get_user_dir(),
                                       pygwy_plugin_dir_name,
                                       NULL);
    pygwy_add_sys_path(d, plugin_dir_name);
    g_free(plugin_dir_name);
    // add /usr/local/share/gwyddion/pygwy to sys.path
    plugin_dir_name = g_build_filename(gwy_find_self_dir("data"),
                                       pygwy_plugin_dir_name,
                                       NULL);
    pygwy_add_sys_path(d, plugin_dir_name);
    g_free(plugin_dir_name);

    // store values for closing console
    s_console_setup->std_err = PyDict_GetItemString(d, "_stderr_redir");
    Py_INCREF(s_console_setup->std_err);
    s_console_setup->dictionary = d;
}

static void
pygwy_on_console_command_execute(GtkEntry *entry, gpointer user_data)
{
    gchar *input_line;
    GString *output;

    input_line = g_strconcat(">>> ", gtk_entry_get_text(entry), "\n", NULL);
    output = g_string_new(input_line);
    output = g_string_append(output,  
          pygwy_console_run_command((gchar*) gtk_entry_get_text(GTK_ENTRY(entry)), Py_single_input) );

    pygwy_console_append((gchar *)output->str);
    g_string_free(output, TRUE);

    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
}

static gboolean
pygwy_on_console_close(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    //Py_DECREF(module);
    //Py_DECREF(py_container); //FIXME
    Py_DECREF(s_console_setup->std_err);
    destroy_environment(s_console_setup->dictionary, FALSE);
    g_free(s_console_setup);
    return FALSE;
}

