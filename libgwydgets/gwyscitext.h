/* @(#) $Id$ */

#ifndef __GWY_SCI_TEXT_H__
#define __GWY_SCI_TEXT_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_SCI_TEXT            (gwy_sci_text_get_type())
#define GWY_SCI_TEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SCI_TEXT, GwySciText))
#define GWY_SCI_TEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SCI_TEXT, GwySciText))
#define GWY_IS_SCI_TEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SCI_TEXT))
#define GWY_IS_SCI_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SCI_TEXT))
#define GWY_SCI_TEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SCI_TEXT, GwySciText))

typedef struct {
    GString *ent_text;
    GString *utf_text;
    
    PangoFontDescription *label_font;
    
} GwySciTextParams;

typedef struct {
    GtkVBox vbox;

    GdkGC *gc;
    GtkEntry *entry;
    GtkLabel *label;
    GtkCombo *entities;

    GwySciTextParams par;
    
} GwySciText;

typedef struct {
     GtkVBoxClass parent_class;
} GwySciTextClass;


GtkWidget* gwy_sci_text_new();

GType gwy_sci_text_get_type(void) G_GNUC_CONST;

gchar* gwy_sci_text_get_text(GwySciText *sci_text);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_SCI_TEXT_H__*/
