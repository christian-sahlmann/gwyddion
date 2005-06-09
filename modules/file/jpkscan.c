/*
 *  Loader for JPK Image Scans.
 *  Copyright (C) 2005  JPK Instruments AG.
 *  Written by Sven Neumann <neumann@jpk.com>.
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
 */

#include <string.h>

#include <tiffio.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>

#include "jpk.h"


static gboolean       module_register    (const gchar             *name);

static gint           jpkscan_detect     (const GwyFileDetectInfo *fileinfo,
                                          gboolean                 only_name);
static GwyContainer * jpkscan_load       (const gchar             *filename);
static GwyContainer * jpkscan_load_tiff  (const gchar             *filename);

static gboolean       tiff_check_version       (gint               macro,
                                                gint               micro);

static void           tiff_load_channel        (TIFF               *tif,
                                                GwyContainer       *container,
                                                gint                idx,
                                                gint                ilen,
                                                gint                jlen,
                                                gdouble             ulen,
                                                gdouble             vlen);

static void           tiff_load_meta           (TIFF               *tif,
                                                GwyContainer       *container);
static void           tiff_load_meta_string    (TIFF               *tif,
                                                GwyContainer       *container,
                                                ttag_t              tag,
                                                const gchar        *name);
static void           tiff_load_meta_double    (TIFF               *tif,
                                                GwyContainer       *container,
                                                ttag_t              tag,
                                                const gchar        *unit,
                                                const gchar        *name);

static gboolean       tiff_get_custom_string   (TIFF               *tif,
                                                ttag_t              tag,
                                                const gchar       **value);
static gboolean       tiff_get_custom_boolean  (TIFF               *tif,
                                                ttag_t              tag,
                                                gboolean           *value);
static gint           tiff_get_custom_integer  (TIFF               *tif,
                                                ttag_t              tag,
                                                gint               *value);
static gboolean       tiff_get_custom_double   (TIFF               *tif,
                                                ttag_t              tag,
                                                gdouble            *value);

static void           tiff_warning             (const gchar        *module,
                                                const gchar        *format,
                                                va_list             args);
static void           tiff_error               (const gchar        *module,
                                                const gchar        *format,
                                                va_list             args);

static GQuark         jpkscan_data_key         (gint                idx);
static GQuark         jpkscan_meta_key         (const gchar        *desc);

static void           meta_store_double        (GwyContainer       *container,
                                                const gchar        *name,
                                                gdouble             value,
                                                const gchar        *unit);

static gint           jpkscan_dialog           (GwyContainer       *container,
                                                const gchar        *filename,
                                                gint                idx);
static void           jpkscan_error_dialog     (const gchar        *title,
                                                const gchar        *format,
                                                ...) G_GNUC_PRINTF(2, 3);
static gchar        * jpkscan_display_basename (const gchar       *filename);


/* The module info. */
static GwyModuleInfo module_info =
{
  GWY_MODULE_ABI_VERSION,
  module_register,
  N_("Imports JPK image scans."),
  "Sven Neumann <neumann@jpk.com>",
  "0.1",
  "JPK Instruments AG",
  "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)


static gboolean
module_register (const gchar *name)
{
  static GwyFileFuncInfo jpkscan_func_info =
  {
    "jpkscan",
    N_("JPK image scans (.jpk)"),
    (GwyFileDetectFunc) jpkscan_detect,
    (GwyFileLoadFunc)   jpkscan_load,
      NULL
  };

  gwy_file_func_register (name, &jpkscan_func_info);

  return TRUE;
}

static gint
jpkscan_detect (const GwyFileDetectInfo *fileinfo,
                gboolean                 only_name)
{
  gint score = 0;

  /*  Since JPK files are just standard TIFF files, we always
   *  look at the filename extension.
   */

  if (g_str_has_suffix (fileinfo->name_lowercase, ".jpk"))
    score += 50;

  if (only_name)
    return score;

  if (fileinfo->buffer_len > JPK_SCAN_MAGIC_SIZE
      && memcmp (fileinfo->buffer, JPK_SCAN_MAGIC, JPK_SCAN_MAGIC_SIZE) == 0)
    score += 50;

  return score;
}

static GwyContainer *
jpkscan_load (const gchar *filename)
{
  GwyContainer *container;
  GObject      *object;
  gint          idx = 0;

  gwy_debug ("Loading <%s>", filename);

  /*  Handling of custom tags was introduced with LibTIFF version 3.6.0  */
  if (! tiff_check_version (3, 6))
    return NULL;

  TIFFSetWarningHandler (tiff_warning);
  TIFFSetErrorHandler (tiff_error);

  container = jpkscan_load_tiff (filename);
  if (! container)
    return NULL;

  /*  if there's more than one channel, present a dialog  */
  if (gwy_container_contains (container, jpkscan_data_key (1)))
    {
      idx = jpkscan_dialog (container, filename, idx);
    }

  if (idx < 0)       /*  user cancelled loading                    */
    {
      g_object_unref (container);
      return NULL;
    }
  else if (idx > 0)  /*  rename the selected channel to "/0/data"  */
    {
      gwy_container_rename (container,
                            jpkscan_data_key (idx), jpkscan_data_key (0),
                            TRUE);
    }

  /*  add the name of the selected channel to the container meta data  */
  object = gwy_container_get_object (container, jpkscan_data_key (0));
  if (object)
    {
      const gchar *name = g_object_get_data (object, "channel-name");

      gwy_container_set_string (container,
                                jpkscan_meta_key ("Channel"), g_strdup (name));
    }

  /*  remove all other channels  */
  for (idx = 1;; idx++)
    {
      GQuark  key = jpkscan_data_key (idx);

      if (gwy_container_contains (container, key))
        gwy_container_remove (container, key);
      else
        break;
    }

  return container;
}

static GwyContainer *
jpkscan_load_tiff (const gchar *filename)
{
  GwyContainer *container = NULL;
  TIFF         *tif;
  gint          ilen;
  gint          jlen;
  gint          idx = 0;
  gushort       bps;
  gushort       photo;
  gushort       planar;
  gdouble       ulen, vlen;

  tif = TIFFOpen (filename, "r");

  if (! tif ||
      /*  sanity check, grid dimensions must be present!  */
      ! (tiff_get_custom_double (tif, JPK_TIFFTAG_Grid_uLength, &ulen) &&
         tiff_get_custom_double (tif, JPK_TIFFTAG_Grid_vLength, &vlen)))
    {
      if (tif)
        TIFFClose (tif);

      jpkscan_error_dialog (_("Cannot load JPK Scan!"),
                            _("Either the file you are trying to open is not "
                              "a JPK scan file, or it is somehow corrupt."));
      return NULL;
    }

  container = gwy_container_new ();

  tiff_load_meta (tif, container);

  gwy_debug ("ulen: %g vlen: %g", ulen, vlen);

  do
    {
      if (! TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &ilen))
        {
          g_warning ("Could not get image width, skipping");
          continue;
        }

      if (! TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &jlen))
        {
          g_warning ("Could not get image length, skipping");
          continue;
        }

      TIFFGetFieldDefaulted (tif, TIFFTAG_BITSPERSAMPLE, &bps);

      if (! TIFFGetField (tif, TIFFTAG_PHOTOMETRIC, &photo))
        {
          g_warning ("Could not get photometric tag, skipping");
          continue;
        }

      /*  we are only interested in 16bit grayscale  */
      switch (photo)
        {
        case PHOTOMETRIC_MINISBLACK:
        case PHOTOMETRIC_MINISWHITE:
          if (bps == 16)
            break;
        default:
          continue;
        }

      if (TIFFGetField (tif, TIFFTAG_PLANARCONFIG, &planar) &&
          planar != PLANARCONFIG_CONTIG)
        {
          g_warning ("Can only handle planar data, skipping");
          continue;
        }

      tiff_load_channel (tif, container, idx++, ilen, jlen, ulen, vlen);
    }
  while (TIFFReadDirectory (tif));

  TIFFClose (tif);

  return container;
}

static gboolean
tiff_check_version (gint required_macro,
                    gint required_micro)
{
  gchar    *version = g_strdup (TIFFGetVersion ());
  gchar    *ptr;
  gboolean  result  = TRUE;
  gint      major;
  gint      minor;
  gint      micro;

  ptr = strchr (version, '\n');
  if (ptr)
    *ptr = '\0';

  ptr = version;
  while (*ptr && !g_ascii_isdigit (*ptr))
    ptr++;

  if (sscanf (ptr, "%d.%d.%d", &major, &minor, &micro) != 3)
    {
      g_warning ("Cannot parse TIFF version, proceed with fingers crossed");
    }
  else if ((major < required_macro) ||
           (major == required_macro && minor < required_micro))
    {
      result = FALSE;

      jpkscan_error_dialog (_("LibTIFF too old!"),
                            _("You are using %s. Please update to "
                              "libtiff version %d.%d or newer."),
                            version, required_macro, required_micro);
    }

  g_free (version);

  return result;
}

static void
tiff_load_channel (TIFF         *tif,
                   GwyContainer *container,
                   gint          idx,
                   gint          ilen,
                   gint          jlen,
                   gdouble       ulen,
                   gdouble       vlen)
{
  GwyDataField *dfield;
  gdouble      *data;
  guchar       *buffer;
  gchar        *channel;
  const gchar  *name      = NULL;
  const gchar  *slot      = NULL;
  const gchar  *unit      = NULL;
  gboolean      retrace   = FALSE;
  gboolean      reflect   = FALSE;
  gdouble       mult      = 0.0;
  gdouble       offset    = 0.0;
  gint          num_slots = 0;
  gint          i, j;

  tiff_get_custom_string (tif, JPK_TIFFTAG_ChannelFancyName, &name);
  if (! name)
    tiff_get_custom_string (tif, JPK_TIFFTAG_Channel, &name);
  g_return_if_fail (name != NULL);

  tiff_get_custom_boolean (tif, JPK_TIFFTAG_Channel_retrace, &retrace);

  channel = g_strdup_printf ("%s%s", name, retrace ? " (retrace)" : "");

  gwy_debug ("channel: %s", channel);

  tiff_get_custom_integer (tif, JPK_TIFFTAG_NrOfSlots, &num_slots);
  g_return_if_fail (num_slots > 0);

  gwy_debug ("num_slots: %d", num_slots);

  /*  locate the default slot  */

  tiff_get_custom_string (tif, JPK_TIFFTAG_DefaultSlot, &slot);
  g_return_if_fail (slot != NULL);

  gwy_debug ("num_slots: %d, default slot: %s", num_slots, slot);

  for (i = 0; i < num_slots; i++)
    {
      const gchar *string;

      if (tiff_get_custom_string (tif, JPK_TIFFTAG_Slot_Name(i), &string) &&
          string                                                          &&
          strcmp (string, slot) == 0)
        {
          tiff_get_custom_string (tif, JPK_TIFFTAG_Scaling_Type(i), &string);
          g_return_if_fail (strcmp (string, "LinearScaling") == 0);

          tiff_get_custom_double (tif, JPK_TIFFTAG_Scaling_Multiply(i), &mult);
          tiff_get_custom_double (tif, JPK_TIFFTAG_Scaling_Offset(i), &offset);

          gwy_debug ("multipler: %g offset: %g", mult, offset);

          tiff_get_custom_string (tif, JPK_TIFFTAG_Encoder_Unit(i), &unit);

          break;
        }
    }

  /*  create a new data field  */

  dfield = gwy_data_field_new (ilen, jlen, ulen, vlen, FALSE);

  if (unit)
    {
      GwySIUnit *siunit = gwy_si_unit_new (unit);

      gwy_data_field_set_si_unit_z (dfield, siunit);
      g_object_unref (siunit);
    }

  /*  read the scan data  */

  data = gwy_data_field_get_data (dfield);

  buffer = g_new (guchar, TIFFScanlineSize (tif));

  tiff_get_custom_boolean (tif, JPK_TIFFTAG_Grid_Reflect, &reflect);

  if (reflect)
    data += (jlen - 1) * ilen;

  for (j = 0; j < jlen; j++)
    {
      const guint16 *src  = (const guint16 *) buffer;
      gdouble       *dest = data;

      TIFFReadScanline (tif, buffer, j, 0);

      for (i = 0; i < ilen; i++)
        {
          guint16 s = *src++;

          *dest++ = offset + mult * (gdouble) s;
        }

      if (reflect)
        data -= ilen;
      else
        data += ilen;
    }

  /*  add the GwyDataField to the container  */

  gwy_container_set_object (container, jpkscan_data_key (idx), dfield);
  g_object_unref (dfield);

  g_object_set_data_full (G_OBJECT (dfield),
                          "channel-name", channel,
                          (GDestroyNotify) g_free);
}

static void
tiff_load_meta (TIFF         *tif,
                GwyContainer *container)
{
  const gchar *string;
  gdouble      frequency;
  gdouble      value;

  tiff_load_meta_string (tif, container,
                         JPK_TIFFTAG_Name, "Name");
  tiff_load_meta_string (tif, container,
                         JPK_TIFFTAG_Comment, "Comment");
  tiff_load_meta_string (tif, container,
                         JPK_TIFFTAG_Sample, "Probe");
  tiff_load_meta_string (tif, container,
                         JPK_TIFFTAG_AccountName, "Account");

  tiff_load_meta_string (tif, container,
                         JPK_TIFFTAG_StartDate, "Time Start");
  tiff_load_meta_string (tif, container,
                         JPK_TIFFTAG_EndDate, "Time End");

  tiff_load_meta_double (tif, container,
                         JPK_TIFFTAG_Grid_x0, "m", "Origin X");
  tiff_load_meta_double (tif, container,
                         JPK_TIFFTAG_Grid_y0, "m", "Origin Y");
  tiff_load_meta_double (tif, container,
                         JPK_TIFFTAG_Grid_uLength, "m", "Size X");
  tiff_load_meta_double (tif, container,
                         JPK_TIFFTAG_Grid_vLength, "m", "Size Y");

  tiff_load_meta_double (tif, container,
                         JPK_TIFFTAG_Scanrate_Dutycycle, NULL, "Duty Cycle");

  tiff_load_meta_string (tif, container,
                         JPK_TIFFTAG_Feedback_Mode, "Feedback Mode");
  tiff_load_meta_double (tif, container,
                         JPK_TIFFTAG_Feedback_iGain, "Hz", "Feedback IGain");
  tiff_load_meta_double (tif, container,
                         JPK_TIFFTAG_Feedback_pGain, NULL, "Feedback PGain");
  tiff_load_meta_double (tif, container,
                         JPK_TIFFTAG_Feedback_Setpoint, "V",
                         "Feedback Setpoint");

  /*  some values need special treatment  */

  if (tiff_get_custom_double (tif,
                              JPK_TIFFTAG_Scanrate_Frequency, &frequency)  &&
      tiff_get_custom_double (tif, JPK_TIFFTAG_Scanrate_Dutycycle, &value) &&
      value > 0.0)
    {
      meta_store_double (container, "Scan Rate", frequency / value, "Hz");
    }

  if (tiff_get_custom_double (tif, JPK_TIFFTAG_Feedback_iGain, &value))
    meta_store_double (container, "Feedback IGain", fabs (value), "Hz");

  if (tiff_get_custom_double (tif, JPK_TIFFTAG_Feedback_pGain, &value))
    meta_store_double (container, "Feedback PGain", fabs (value), NULL);

  if (tiff_get_custom_string (tif, JPK_TIFFTAG_Feedback_Mode, &string))
    {
      if (strcmp (string, "contact") == 0)
        {
          tiff_load_meta_double (tif, container,
                                 JPK_TIFFTAG_Feedback_Baseline, "V",
                                 "Feedback Baseline");
        }
      else if (strcmp (string, "intermittent") == 0)
        {
           tiff_load_meta_double (tif, container,
                                 JPK_TIFFTAG_Feedback_Amplitude, "V",
                                 "Feedback Amplitude");
           tiff_load_meta_double (tif, container,
                                 JPK_TIFFTAG_Feedback_Frequency, "Hz",
                                 "Feedback Frequency");
           tiff_load_meta_double (tif, container,
                                 JPK_TIFFTAG_Feedback_Phaseshift, "deg",
                                 "Feedback Phaseshift");
        }
    }
}

static void
tiff_load_meta_string (TIFF         *tif,
                       GwyContainer *container,
                       ttag_t        tag,
                       const gchar  *name)
{
  const gchar *string;

  if (tiff_get_custom_string (tif, tag, &string))
    gwy_container_set_string (container,
                              jpkscan_meta_key (name), g_strdup (string));
}

static void
tiff_load_meta_double (TIFF         *tif,
                       GwyContainer *container,
                       ttag_t        tag,
                       const gchar  *unit,
                       const gchar  *name)
{
  gdouble value;

  if (tiff_get_custom_double (tif, tag, &value))
    meta_store_double (container, name, value, unit);
}

static gboolean
tiff_get_custom_string (TIFF         *tif,
                        ttag_t        tag,
                        const gchar **value)
{
  const gchar *s;
  gint         count;

  if (TIFFGetField (tif, tag, &count, &s))
    {
      *value = s;
      return TRUE;
    }
  else
    {
      *value = NULL;
      return FALSE;
    }
}

/*  reads what the TIFF spec calls SSHORT and interprets it as a boolean  */
static gboolean
tiff_get_custom_boolean (TIFF     *tif,
                         ttag_t    tag,
                         gboolean *value)
{
  gshort *s;
  gint    count;

  if (TIFFGetField (tif, tag, &count, &s))
    {
      *value = (*s != 0);
      return TRUE;
    }
  else
    {
      *value = FALSE;
      return FALSE;
    }
}

/*  reads what the TIFF spec calls SLONG  */
static gboolean
tiff_get_custom_integer (TIFF   *tif,
                         ttag_t  tag,
                         gint   *value)
{
  gint32 *l;
  gint    count;

  if (TIFFGetField (tif, tag, &count, &l))
    {
      *value = *l;
      return TRUE;
    }
  else
    {
      *value = 0;
      return FALSE;
    }
}

/*  reads what the TIFF spec calls DOUBLE  */
static gboolean
tiff_get_custom_double (TIFF    *tif,
                        ttag_t   tag,
                        gdouble *value)
{
  gdouble *d;
  gint     count;

  if (TIFFGetField (tif, tag, &count, &d))
    {
      *value = *d;
      return TRUE;
    }
  else
    {
      *value = 0.0;
      return FALSE;
    }
}

static void
tiff_warning (const gchar *module G_GNUC_UNUSED,
              const gchar *format G_GNUC_UNUSED,
              va_list      args   G_GNUC_UNUSED)
{
  /*  ignore  */
}

static void
tiff_error (const gchar *module G_GNUC_UNUSED,
            const gchar *format,
            va_list      args)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, format, args);
}

static void
meta_store_double (GwyContainer *container,
                   const gchar  *name,
                   gdouble       value,
                   const gchar  *unit)
{
  GwySIUnit        *siunit = gwy_si_unit_new (unit);
  GwySIValueFormat *format = gwy_si_unit_get_format (siunit,
                                                     GWY_SI_UNIT_FORMAT_MARKUP,
                                                     value, NULL);

  gwy_container_set_string (container, jpkscan_meta_key (name),
                            g_strdup_printf ("%5.3f %s",
                                             value / format->magnitude,
                                             format->units));
  g_object_unref (siunit);
}

static GQuark
jpkscan_data_key (gint idx)
{
  gchar  *key   = g_strdup_printf ("/%d/data", idx);
  GQuark  quark = g_quark_from_string (key);

  g_free (key);

  return quark;
}

static GQuark
jpkscan_meta_key (const gchar *desc)
{
  GQuark  quark;
  gchar  *key = g_strconcat ("/meta/", desc, NULL);

  quark = g_quark_from_string (key);

  g_free (key);

  return quark;
}

/*  dialog  */

static gint
jpkscan_dialog (GwyContainer *container,
                const gchar  *filename,
                gint          idx)
{
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *combo;
  gchar     *name;
  gchar     *text;
  gint       i;

  dialog = gtk_dialog_new_with_buttons (_("Open JPK Image"), 0, 0,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,   GTK_RESPONSE_OK,
                                        NULL);

  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  name = jpkscan_display_basename (filename);
  text = g_strdup_printf (_("Select a channel to load from\n"
                            "<b>%s</b>"), name);
  g_free (name);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label",      text,
                        "use-markup", TRUE,
                        "xalign",     0.0,
                        NULL);
  g_free (text);

  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("Ch_annel:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
  gtk_widget_show (combo);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

  for (i = 0;; i++)
    {
      GObject *object;
      GQuark   key = jpkscan_data_key (i);

      if (! gwy_container_contains (container, key))
        break;

      object = gwy_container_get_object (container, jpkscan_data_key (i));
      gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
                                 g_object_get_data (object, "channel-name"));
    }

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), idx);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    idx = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
  else
    idx = -1;

  gtk_widget_destroy (dialog);

  return idx;
}

static void
jpkscan_error_dialog (const gchar *title,
                      const gchar *format,
                      ...)
{
  GtkWidget *dialog;
  GtkWidget *box;
  GtkWidget *label;
  gchar     *message;
  va_list    args;

  dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK,
                                   title);

  /*  The following code could (and should) be replaced by a call to
   *  gtk_message_dialog_format_secondary_text() (requires GTK+ >= 2.6).
   */

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label",   message,
                        "xalign",  0.0,
                        "yalign",  0.0,
                        "wrap",    TRUE,
                        "justify", GTK_JUSTIFY_LEFT,
                        NULL);

  box = gtk_widget_get_parent (GTK_MESSAGE_DIALOG (dialog)->label);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  g_free (message);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static gchar *
jpkscan_display_basename (const gchar *filename)
{
  gchar  *basename;
  gchar  *display_name;
  GError *error = NULL;

  /*  The following code could (and should) be replaced by a call to
   *  g_filename_display_basename() (requires GLib >= 2.6).
   */

  g_return_val_if_fail (filename != NULL, NULL);

  basename = g_path_get_basename (filename);
  display_name = g_filename_to_utf8 (basename, -1, NULL, NULL, &error);
  g_free (basename);

  if (! display_name)
    {
      g_warning ("Error converting filename to UTF8: %s", error->message);
      g_error_free (error);

      return g_strdup (_("(invalid filename encoding)"));
    }

  return display_name;
}
