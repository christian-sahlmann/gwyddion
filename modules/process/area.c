#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h> 


#define  WITHIN(what,lim_a,lim_b)  (((what) > (lim_a)) && ((what) < (lim_b)))

#define DEBUG 1

enum {
      MASK_SELECT_DEV = 0,
      MASK_SELECT_CENTER
                   
};    

enum {
      MASK_PUT_XOR = 0,
      MASK_PUT_NOT,
      MASK_PUT_AND,
      MASK_PUT_OR 
};



/*================ tools controls ========================*/
typedef struct  
{
    double x,y,z;    
} gwy_vec;  

typedef struct  {
        
        GtkObject *EntryAngleMinus;
        GtkObject *EntryAnglePlus;
        GtkObject *EntryMaxDev;
        
        GtkWidget *EntryNVx;
        GtkWidget *EntryNVy;
        GtkWidget *EntryNVz;
        
                    
        GSList *RadioOR_group;
        GSList *RadioCentral_group;
        
        GtkObject *EntryPORx;
        GtkObject *EntryPORy;
        
        GtkWidget *LabelArea; 
        
      
        int x_res, y_res;
      
        int ref_x, ref_y;        /* pixelove XY souradnice bodu plochy */
        gwy_vec normal;         /* normal vector to the surface in XY-Z */
        
        double angle_min, angle_max, angle_dev;    
                
        int mask_put_mode;                                
        int mask_select_mode;
        
                                                              
} ToolControls;
     
   

static gboolean    module_register            (const gchar *name);
static gboolean    area                       (GwyDataWindow *data_window,
                                                GwyToolSwitchEvent reason); 

static GtkWidget* dialog_create    (GwyUnitoolState *state);
static void       dialog_update    (GwyUnitoolState *state,
                                    GwyUnitoolUpdateType reason);
                                    
static void       dialog_abandon   (GwyUnitoolState *state);
static void       dialog_apply   (GwyUnitoolState *state);
                                    
                                              
void on_ButtonRefPointSelect_released (GtkButton * button, GwyUnitoolState *state);

void on_ButtonNVfromPOR_pressed (GtkButton * button, GwyUnitoolState *state);

void on_ButtonSetMask_pressed (GtkButton * button, GwyUnitoolState *state);

void on_ButtonPORSelect_pressed (GtkButton * button, GwyUnitoolState *state);
                                               
void
on_RadioCentral_group_changed(GtkButton * button, GwyUnitoolState *state);
void
on_RadioOR_group_changed(GtkButton * button, GwyUnitoolState *state);
void
on_ButtonComputeArea_pressed (GtkButton * button, GwyUnitoolState *state);                                                                                                                                                                                                                                           

void
on_EntryPOR_changed (GtkButton * button, GwyUnitoolState *state);

gint which_radio_button (GSList *list);
void read_data_from_controls (ToolControls *c);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "surface_area",
    "Computes area of surface under mask",
    "Lukas Chvatal",
    "1.0",
    "LCh",
    "2005"
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)


static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo area_func_info = {
        "surface_area",
        "gwy_area",
        "Creates specific mask and compute area of surface under it",
        200,
        &area,
    };

    gwy_tool_func_register(name, &area_func_info);

    return TRUE;
}


static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    NULL,                          /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                          /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    NULL,                          /* apply action */
    NULL                           /* nonstandard response handler */
};


/* =============== vector functions ============================================ */
 
gwy_vec gwy_vec_cross (gwy_vec v1, gwy_vec v2)
{
    gwy_vec result;    
    result.z = v1.y*v2.z - v2.y*v1.z;
    result.y = v1.z*v2.x - v2.z*v1.x;   
    result.x = v1.x*v2.y - v2.x*v1.y;
    return result;
}    
double gwy_vec_inner (gwy_vec v1, gwy_vec v2)
{
   return  v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;  
    
}    
double gwy_vec_abs (gwy_vec v)
{
   return sqrt (gwy_vec_inner (v,v));
    
}    
double gwy_vec_cos (gwy_vec v1, gwy_vec v2)
{
  return (gwy_vec_inner (v1, v2)/
            (gwy_vec_abs(v1)* gwy_vec_abs(v2)) );   
}    
void  gwy_vec_normalize (gwy_vec *v)
{
    double vabs = gwy_vec_abs (*v);
    v->x /= vabs;
    v->y /= vabs;
    v->z /= vabs;
    
}    


static gboolean
area (GwyDataWindow *data_window,
    GwyToolSwitchEvent reason) 
{
    static const gchar *layer_name = "GwyLayerPointer";
    static GwyUnitoolState *state = NULL;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
        state->apply_doesnt_close =  TRUE;
        
        

    }       
    return gwy_unitool_use(state, data_window, reason);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
  GtkWidget *dialog;
  GtkWidget *dialog_vbox1;
  GtkWidget *table1;
  GtkWidget *labelPointOfReference;

  GtkWidget *RadioCentral;
  
  GtkWidget *RadioDev;

  GtkWidget *LabelDeg;

  GtkWidget *LabelAtMax;

  GtkWidget *LabelAtMaxDeg;
  GtkWidget *RadioOR;

  GtkWidget *temp;
 

  GtkWidget *RadioAND;
  GtkWidget *RadioNOT;
  GtkWidget *RadioXOR;
  GtkWidget *labelCriteria;
  GtkWidget *hseparator1;
  GtkWidget *ButtonComputeArea;  

  GtkWidget *ButtonNVfromPOR;
  GtkWidget *ButtonSetMask;
  GtkWidget *ButtonPORSelect;
  GtkWidget *label14;
  GtkWidget *hbuttonbox1;
  GtkWidget *dialog_action_area1;

   
  ToolControls *c = state->user_data;

  dialog = gtk_dialog_new_with_buttons(_("Read Value"), NULL, 0, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Compute area of 3D surface"));  

  gwy_unitool_dialog_add_button_hide(dialog);  

  dialog_vbox1 = GTK_DIALOG (dialog)->vbox;
  
  GtkWidget *frame = gwy_unitool_windowname_frame_create(state); 
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame, FALSE, FALSE, 0); 

  table1 = gtk_table_new (8, 5, FALSE);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), table1, TRUE, TRUE, 5);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 6);

/////////////////////////////// POR
  labelPointOfReference = gtk_label_new (_("Point of reference"));
  gtk_table_attach (GTK_TABLE (table1), labelPointOfReference, 0, 1, 0, 1, 
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (labelPointOfReference), 0, 0.5);

  c->EntryPORx = gtk_adjustment_new (100, 0, 999999, 1, 10, 10);
  temp = gtk_spin_button_new (GTK_ADJUSTMENT (c->EntryPORx), 1, 0);  
  gtk_table_attach (GTK_TABLE (table1), temp, 1, 2, 0, 1,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (temp), TRUE);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (temp), TRUE);  

  c->EntryPORy = gtk_adjustment_new (100, 0, 999999, 1, 10, 10);
  temp = gtk_spin_button_new (GTK_ADJUSTMENT (c->EntryPORy), 1, 0); 
  gtk_table_attach (GTK_TABLE (table1), temp, 2, 3, 0, 1,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (temp), TRUE);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (temp), TRUE);  

  label14 = gtk_label_new (_("[data pts.]"));
  gtk_table_attach (GTK_TABLE (table1), label14, 3, 4, 0, 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
///////////////////////// MASK SEL
  RadioCentral =
    gtk_radio_button_new_with_mnemonic (NULL,
					_("Tangent angle \ndirecting to PoR\n lies  in"));
  gtk_table_attach (GTK_TABLE (table1), RadioCentral, 0, 1, 2, 3,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  c->RadioCentral_group = NULL;
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (RadioCentral),
			      c->RadioCentral_group);
  c->RadioCentral_group =
    gtk_radio_button_get_group (GTK_RADIO_BUTTON (RadioCentral));


  RadioDev =    gtk_radio_button_new_with_mnemonic (NULL,	_("Deviation from\nnormal vector "));
  gtk_table_attach (GTK_TABLE (table1), RadioDev, 0, 1, 3, 4,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (RadioDev),
			      c->RadioCentral_group);
  c->RadioCentral_group =
    gtk_radio_button_get_group (GTK_RADIO_BUTTON (RadioDev));
/////////////////ANGLES
  c->EntryAngleMinus = gtk_adjustment_new (-30, -90, 90, 1, 10, 10);
  temp = gtk_spin_button_new (GTK_ADJUSTMENT (c->EntryAngleMinus), 1, 2); 
  gtk_table_attach (GTK_TABLE (table1), temp, 1, 2, 2, 3, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),(GtkAttachOptions) (0), 0, 0);
  gtk_spin_button_set_wrap        (temp, TRUE);
  
  c->EntryAnglePlus = gtk_adjustment_new (0, -90, 90, 1, 10, 10);
  temp = gtk_spin_button_new (GTK_ADJUSTMENT (c->EntryAnglePlus), 1, 2); 
  gtk_table_attach (GTK_TABLE (table1), temp, 2, 3, 2, 3, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),(GtkAttachOptions) (0), 0, 0);
  gtk_spin_button_set_wrap(temp, TRUE);
  

  LabelDeg = gtk_label_new (_("degrees"));
  gtk_table_attach (GTK_TABLE (table1), LabelDeg, 3, 4, 2, 3,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (LabelDeg, 30, -1);
  gtk_label_set_justify (GTK_LABEL (LabelDeg), GTK_JUSTIFY_CENTER);
/////////////////////////////////////  NORMAL
  
  c->EntryNVx =  gtk_adjustment_new (0, 0, 99, 1, 10, 10);
  temp = gtk_spin_button_new (GTK_ADJUSTMENT (c->EntryNVx), 1, 5);
  gtk_table_attach (GTK_TABLE (table1), temp, 1, 2, 3, 4,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  
  c->EntryNVy =  gtk_adjustment_new (0, 0, 99, 1, 10, 10);
  temp = gtk_spin_button_new (GTK_ADJUSTMENT (c->EntryNVy), 0.1, 5);
  gtk_table_attach (GTK_TABLE (table1), temp, 2, 3, 3, 4,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  
  c->EntryNVz =  gtk_adjustment_new (1, 0, 99, 1, 10, 10);
  temp = gtk_spin_button_new (GTK_ADJUSTMENT (c->EntryNVz), 1, 5);
  gtk_table_attach (GTK_TABLE (table1), temp, 3, 4, 3, 4,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  
///////////////////////////  MAX ANGLE
  LabelAtMax = gtk_label_new (_("is at max."));
  gtk_table_attach (GTK_TABLE (table1), LabelAtMax, 1, 2, 4, 5,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (LabelAtMax), 0, 0.5);

  c->EntryMaxDev = gtk_adjustment_new (3, 0, 180, 1, 10, 10);
  temp = gtk_spin_button_new (GTK_ADJUSTMENT (c->EntryMaxDev), 0.1, 3); 
  gtk_table_attach (GTK_TABLE (table1), temp, 2, 3, 4, 5, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),(GtkAttachOptions) (0), 0, 0);
  gtk_spin_button_set_wrap        (temp, TRUE);  

  LabelAtMaxDeg = gtk_label_new (_("degrees"));
  gtk_table_attach (GTK_TABLE (table1), LabelAtMaxDeg, 3, 4, 4, 5, (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (LabelAtMaxDeg), GTK_JUSTIFY_CENTER);

//////////////////////
  c->RadioOR_group = NULL;
  RadioOR = gtk_radio_button_new_with_mnemonic (NULL, _("OR"));
  gtk_table_attach (GTK_TABLE (table1), RadioOR, 0, 1, 5, 6,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (RadioOR), c->RadioOR_group);
  c->RadioOR_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (RadioOR));

  RadioAND = gtk_radio_button_new_with_mnemonic (NULL, _("AND"));
  gtk_table_attach (GTK_TABLE (table1), RadioAND, 1, 2, 5, 6,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (RadioAND), c->RadioOR_group);
  c->RadioOR_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (RadioAND));

  RadioNOT = gtk_radio_button_new_with_mnemonic (NULL, _("NOT"));
  gtk_table_attach (GTK_TABLE (table1), RadioNOT, 2, 3, 5, 6,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (RadioNOT), c->RadioOR_group);
  c->RadioOR_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (RadioNOT));

  RadioXOR = gtk_radio_button_new_with_mnemonic (NULL, _("XOR"));
  gtk_table_attach (GTK_TABLE (table1), RadioXOR, 3, 4, 5, 6,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);  		    
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (RadioXOR), c->RadioOR_group);
  c->RadioOR_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (RadioXOR));

  labelCriteria = gtk_label_new (_("Mask selection criteria"));
  gtk_table_attach (GTK_TABLE (table1), labelCriteria, 0, 3, 1, 2,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (labelCriteria), GTK_JUSTIFY_CENTER);
  gtk_misc_set_alignment (GTK_MISC (labelCriteria), 0, 0.5);
///////////////////////////////////// COMPUTE
  hseparator1 = gtk_hseparator_new ();
  gtk_table_attach (GTK_TABLE (table1), hseparator1, 0, 5, 6, 7,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

//////////////////////////////////
  ButtonNVfromPOR = gtk_button_new_with_mnemonic (_("At PoR"));
  gtk_widget_set_name (ButtonNVfromPOR, "ButtonNVfromPOR");
  gtk_widget_show (ButtonNVfromPOR);
  gtk_table_attach (GTK_TABLE (table1), ButtonNVfromPOR, 4, 5, 3, 4,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (ButtonNVfromPOR, 50, -1);

  ButtonSetMask = gtk_button_new_with_mnemonic (_("Set mask"));
  gtk_table_attach (GTK_TABLE (table1), ButtonSetMask, 4, 5, 5, 6,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);

  ButtonPORSelect = gtk_button_new_with_mnemonic (_("Select"));
  gtk_table_attach (GTK_TABLE (table1), ButtonPORSelect, 4, 5, 0, 1,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (ButtonPORSelect, 50, -1);



/////////////////////////// AREA
  ButtonComputeArea = gtk_button_new_with_mnemonic (_("ComputeArea"));
  gtk_table_attach (GTK_TABLE (table1), ButtonComputeArea, 3, 5, 7, 8,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);

  c->LabelArea = gtk_label_new (_("And the area is ..."));
  gtk_table_attach (GTK_TABLE (table1), c->LabelArea, 0, 3, 7, 8,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (c->LabelArea), GTK_JUSTIFY_CENTER);


  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), hbuttonbox1, FALSE, FALSE, 0);

  dialog_action_area1 = GTK_DIALOG (dialog)->action_area;
  gtk_widget_set_name (dialog_action_area1, "dialog_action_area1");
  gtk_widget_show (dialog_action_area1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1),
			     GTK_BUTTONBOX_END);

  g_signal_connect ((gpointer) c->EntryPORx, "value_changed",
		    G_CALLBACK (on_EntryPOR_changed), state);
  g_signal_connect ((gpointer) c->EntryPORy, "value_changed",
		    G_CALLBACK (on_EntryPOR_changed), state);

  g_signal_connect ((gpointer) ButtonNVfromPOR, "pressed",
		    G_CALLBACK (on_ButtonNVfromPOR_pressed), state);
  g_signal_connect ((gpointer) ButtonSetMask, "pressed",
		    G_CALLBACK (on_ButtonSetMask_pressed), state);
  g_signal_connect ((gpointer) ButtonPORSelect, "pressed",
		    G_CALLBACK (on_ButtonPORSelect_pressed), state);


  g_signal_connect ((gpointer) ButtonComputeArea, "pressed",
		    G_CALLBACK (on_ButtonComputeArea_pressed), state);


  return dialog;
}


static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble value, xy[2];
    gboolean is_visible, is_selected;


FILE* deb = fopen("update.txt","wa");
fprintf(deb,"1 \n");
    controls = (ToolControls*)state->user_data;
fprintf(deb,"2 \n");
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));    
fprintf(deb,"3 \n");
    is_visible = state->is_visible;
    is_selected = gwy_vector_layer_get_selection(state->layer, xy);
fprintf(deb,"%d %d \n", is_visible, is_selected);    
    if (!is_visible && !is_selected)
        return; 
           
    gtk_adjustment_set_value(controls->EntryPORx, gwy_data_field_rtoi(dfield, xy[0]));
    gtk_adjustment_set_value(controls->EntryPORy, gwy_data_field_rtoj(dfield, xy[1]));

fprintf(deb,"%lf %lf / %lf => %lf %lf, %lf %lf\n", xy[0], xy[1],state->coord_format->magnitude,
gwy_data_field_rtoj(dfield, xy[1]),
gwy_data_field_rtoi(dfield, xy[0]), gwy_data_field_get_xreal(dfield), gwy_data_field_get_yreal(dfield));   

fclose(deb);


}

static void
dialog_abandon(GwyUnitoolState *state)
{

    memset(state->user_data, 0, sizeof(ToolControls));
}
 

static void
dialog_apply(GwyUnitoolState *state)
{

}


void
on_ButtonNVfromPOR_pressed (GtkButton * button, GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    char str[20];
    gdouble value, xy[2];

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = gwy_container_get_object_by_name(data, "/0/data");

    read_data_from_controls (controls);

    controls->normal.x = -gwy_data_field_get_xder(dfield, controls->ref_x, controls->ref_y);
    controls->normal.y = -gwy_data_field_get_yder(dfield, controls->ref_x, controls->ref_y);
    controls->normal.z = 1;
    gwy_vec_normalize (&(controls->normal));

    gtk_adjustment_set_value(controls->EntryNVx, controls->normal.x);
    gtk_adjustment_set_value(controls->EntryNVy, controls->normal.y);
    gtk_adjustment_set_value(controls->EntryNVz, controls->normal.z);
    
}


void
on_ButtonSetMask_pressed (GtkButton * button, GwyUnitoolState *state)
{
  ToolControls *c = state->user_data;
  read_data_from_controls(c);    
                    
    GwyContainer *data;
    GObject *dfield;
    GwyDataViewLayer *layer;
    GwySIUnit *siunit;
    GwyDataField *mask=NULL;
    

FILE* deb = fopen("c:\\gwydev\\indent\\setmask.txt","wa");
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = gwy_container_get_object_by_name(data, "/0/data");
    gwy_container_gis_object_by_name(data, "/0/mask", (GObject**)&mask);
    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
   
    if (!mask) {
	printf("johoho\n");
            mask = GWY_DATA_FIELD(gwy_serializable_duplicate(dfield));
            siunit = GWY_SI_UNIT(gwy_si_unit_new(""));
            gwy_data_field_set_si_unit_z(mask, siunit);
            g_object_unref(siunit);
            gwy_container_set_object_by_name(data, "/0/mask", (GObject*)mask);
            g_object_unref(mask);
    }
 
      
    int mask_it;
    double cos_xy;
    double cos_max = cos(c->angle_dev);    

    double tan_min = tan(c->angle_min);
    double tan_max = tan(c->angle_max);
    double tan_xy;
           
    gint mask_xy;
    gwy_vec normal_xy;
    
    int x_pos, y_pos;          
           
    double temp, dr, dx, dy, dir_cos, dir_sin;
    
    
    for (x_pos = gwy_data_field_get_xres(dfield)-1; x_pos; x_pos--)
    {
        for (y_pos = gwy_data_field_get_yres(dfield)-1; y_pos; y_pos--)
        {
             switch(c->mask_select_mode)
             {
             case MASK_SELECT_CENTER:

                   if((c->ref_x == x_pos) && (c->ref_y == y_pos)) {
                     mask_it =0;                     
                     break;   
                   }    
                   
                   dx = x_pos - c->ref_x;
                   dy = y_pos - c->ref_y;
                   dr = sqrt(dx*dx+dy*dy);
                   dir_cos = -dx/dr;
                   dir_sin = -dy/dr;
                   
                   tan_xy = gwy_data_field_get_xder(dfield, x_pos, y_pos)*dir_cos +
                            gwy_data_field_get_yder(dfield, x_pos, y_pos)*dir_sin;                                      
                                      
                   if((tan_xy > tan_min) && (tan_xy < tan_max))
                   {
                            fprintf(deb,"%d %d %lf %lf %lf\n",x_pos,y_pos,dir_cos,dir_sin, tan_xy);
                            mask_it = 1;
                   }    
                   else
                            //fprintf(deb,"(0)%d %d %lf %lf %lf\n",x_pos,y_pos,dir_cos,dir_sin, tan_xy);
                            mask_it = 0;
                            
                   break; 
                        
             case MASK_SELECT_DEV:
                 
                   normal_xy.x = -gwy_data_field_get_xder(dfield, x_pos, y_pos);
                   normal_xy.y = -gwy_data_field_get_yder(dfield, x_pos, y_pos);
                   normal_xy.z = 1;
                                                         
                   cos_xy = gwy_vec_cos(c->normal, normal_xy);
//                 fprintf(deb,"%lf %lf %lf %lf \n", normal_xy.x,normal_xy.y,normal_xy.z, cos_xy);
                   
                   if (cos_xy > cos_max)
                      mask_it = 1;
                   else
                      mask_it = 0;
                   break;
             default:
                   mask_it = 0;
                   break;    
             }                         
             
             temp =    gwy_data_field_get_val (mask, x_pos, y_pos);                              
             mask_xy = temp;
             //fprintf(deb,"(%lf=%d; %d",temp,mask_xy,mask_it);
             switch(c->mask_put_mode)
             {
             case MASK_PUT_AND:   mask_it = mask_xy && mask_it; 
                                  break;
             case MASK_PUT_OR:    mask_it = mask_xy || mask_it; break;
             case MASK_PUT_NOT:   mask_it = !mask_it;                                      
                                  break;
             case MASK_PUT_XOR:   mask_it = (mask_xy || mask_it)==0? 1:0;  break;
             }        
             
             if(mask_it) 
                          gwy_data_field_set_val (mask, x_pos, y_pos, 1.0);
             else
                          gwy_data_field_set_val (mask, x_pos, y_pos, 0.0);

           
//                fprintf(deb,"[%d %d] %d) ", x_pos, y_pos,mask_it);
             
             
        }
        fprintf(deb,"\n");
    }        
    
fclose(deb);


    gwy_vector_layer_unselect(GWY_VECTOR_LAYER(layer));
    gwy_app_data_view_update(layer->parent);
}


void
on_EntryPOR_changed (GtkButton * button, GwyUnitoolState *state)
{              
    GwyContainer *data;
    GObject *dfield;
    GwyDataViewLayer *layer;  
    ToolControls* controls;

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = gwy_container_get_object_by_name(data, "/0/data");
    
    gint x_res =  gwy_data_field_get_xres(dfield);
    gint y_res =  gwy_data_field_get_yres(dfield);
    
//    FILE* deb = fopen("c:\\gwydev\\indent\\res.txt","wa");
//fprintf(deb,"%d %d\n", x_res,y_res);
//fclose(deb);
    
    if (gtk_adjustment_get_value(controls->EntryPORx) > x_res)
        gtk_adjustment_set_value(controls->EntryPORx, x_res);
    if (gtk_adjustment_get_value(controls->EntryPORy) > y_res)
        gtk_adjustment_set_value(controls->EntryPORy, y_res);       
   
}



void
on_ButtonPORSelect_pressed (GtkButton * button, GwyUnitoolState *state)
{
    
}

void
on_ButtonComputeArea_pressed (GtkButton * button, GwyUnitoolState *state)
{
  ToolControls *c = state->user_data;
  read_data_from_controls(c);
  
    int mask_it;
    double cos_xy, cos_max;    
    gint mask_xy;
                
    GwyContainer *data;
    GObject *dfield;
    GwyDataField *mask = NULL;
    GwyDataViewLayer *layer;
    ToolControls *controls;
    GwySIUnit *siunit;

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = gwy_container_get_object_by_name(data, "/0/data");
    gwy_container_gis_object_by_name(data, "/0/mask", (GObject**)&mask);

    if (!mask) {
        char area_str[10];
        sprintf(area_str, "No area selected");
        gtk_label_set_text(controls->LabelArea, area_str); 
        return;
    }               
         
    int masked;
    int x_pos_max = gwy_data_field_get_xres(dfield)-1;
    int y_pos_max = gwy_data_field_get_yres(dfield)-1;
    int x_pos, y_pos;
    double x_pos_real, y_pos_real;
    double dx = gwy_data_field_get_xreal(dfield)/(double)gwy_data_field_get_xres(dfield);
    double dy = gwy_data_field_get_yreal(dfield)/(double)gwy_data_field_get_yres(dfield);
    double y_tr, y_br, y_tl, y_bl;
    gwy_vec v1, v2, s;
    
    double ds;    
    double total_area = 0;
    double flat_area = 0;
              
FILE* deb = fopen("c:\\gwydev\\indent\\area.txt","w");
fprintf(deb, "%.12lf %.12lf\n",gwy_data_field_get_xreal(dfield), gwy_data_field_get_yreal(dfield));

              
    for (x_pos = 0, x_pos_real=0; x_pos < x_pos_max; x_pos++, x_pos_real+=dx )
    {
        for (y_pos = 0, y_pos_real=0; y_pos < y_pos_max; y_pos++, y_pos_real+=dy )
        {
                masked  = gwy_data_field_get_val(mask, x_pos, y_pos);
                if(masked)
                {
                y_tr = gwy_data_field_get_val(dfield,x_pos+1,y_pos);
                y_br = gwy_data_field_get_val(dfield,x_pos+1,y_pos+1);
                y_tl = gwy_data_field_get_val(dfield,x_pos,y_pos);
                y_bl = gwy_data_field_get_val(dfield,x_pos,y_pos+1);
                
                v1.x = dx;
                v1.y = 0;
                v1.z = y_tl - y_tr;
                
                v2.x = 0;
                v2.y = dy;
                v2.z = y_bl - y_tl;                
                
                s = gwy_vec_cross(v1,v2);
                
                ds = gwy_vec_abs(s);    
                
                v1.z = y_bl - y_br;                
                v2.z = y_br - y_tr;                
                
                s = gwy_vec_cross(v1,v2);
                ds += gwy_vec_abs(s);    
                
                total_area += ds;                
                flat_area += dx*dy;
                
                } // masked                            
        }                           
    }         
      
   
    total_area /= 2;
    total_area /= state->coord_format->magnitude;
    total_area /= state->coord_format->magnitude;  /// ^2
    
    flat_area /= state->coord_format->magnitude;
    flat_area /= state->coord_format->magnitude;  /// ^2
          
 //fprintf(deb, "%lf",state->coord_format->magnitude);
    char area_str[10];
    sprintf(area_str, "Area =  <b>%.3lf</b>  [%s^2] ...%.2lf\%\n"
                      "Mask = <b>%.3lf</b> [%s^2] ... 100\%", 
                      total_area, state->coord_format->units, total_area/flat_area, 
                      flat_area, state->coord_format->units);
    gtk_label_set_markup(controls->LabelArea, area_str);
   
}


gint which_radio_button (GSList *list)
{
   int i = 0;
    while(list && (GTK_WIDGET_STATE(list->data)!=GTK_STATE_ACTIVE))
    {        
        list = g_slist_next(list);
        i++;

    }       
    
    return i;
}    


void read_data_from_controls (ToolControls *c)
{
    
  c->mask_put_mode = which_radio_button(c->RadioOR_group);
  c->mask_select_mode = which_radio_button(c->RadioCentral_group);
  
  c->angle_min = gtk_adjustment_get_value(c->EntryAngleMinus)*G_PI/180.;
  c->angle_max = gtk_adjustment_get_value(c->EntryAnglePlus)*G_PI/180.;  
  c->angle_dev = gtk_adjustment_get_value(c->EntryMaxDev)*G_PI/180.;
  
  c->normal.x = gtk_adjustment_get_value(c->EntryNVx);
  c->normal.y = gtk_adjustment_get_value(c->EntryNVy);
  c->normal.z = gtk_adjustment_get_value(c->EntryNVz);
  
  c->ref_x = (int)gtk_adjustment_get_value(c->EntryPORx);
  c->ref_y = (int)gtk_adjustment_get_value(c->EntryPORy);  

}    

