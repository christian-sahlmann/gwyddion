/* @(#) $Id$ */

#include <string.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkimagemenuitem.h>

#include <libprocess/simplefft.h>
#include "gwydgets.h"

GtkWidget*
gwy_fft_output_menu(GCallback callback,
                              gpointer cbdata,
                              GwyFFTOutputType current)
{
    static const GwyOptionMenuEntry entries[] = {
        { "Real + Imaginary",  GWY_FFT_OUTPUT_REAL_IMG,  },
        { "Module + Phase",    GWY_FFT_OUTPUT_MOD_PHASE, },
        { "Real",              GWY_FFT_OUTPUT_REAL,      },
        { "Imaginary",         GWY_FFT_OUTPUT_IMG,       },
        { "Module",            GWY_FFT_OUTPUT_MOD,       },
        { "Phase",             GWY_FFT_OUTPUT_PHASE,     },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "fft-output-type", callback, cbdata,
                                  current);
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
