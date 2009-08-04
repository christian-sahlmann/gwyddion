#ifndef MAC_INTEGRATION_H
#define MAC_INTEGRATION_H

#include <gtk/gtk.h>

void gwy_osx_init_handler(int *argc);
void gwy_osx_remove_handler(void);
void gwy_osx_open_files(void);
void gwy_osx_set_locale(void);

void gwy_osx_get_menu_from_widget(GtkWidget *container);
#endif
