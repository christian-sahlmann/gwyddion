/* @(#) $Id$ */

#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>

void
gwy_tools_crop_use(GwyDataWindow *data_window)
{
    GwyDataView *data_view;
    GwyDataViewLayer *select_layer;

    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    select_layer = (GwyDataViewLayer*)gwy_layer_select_new();
    gwy_data_view_set_top_layer(data_view, select_layer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

