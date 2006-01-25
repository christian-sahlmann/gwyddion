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

#include "config.h"
#include <string.h>

#include <tiffio.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libprocess/stats.h>

#include "err.h"
#include "jpk.h"


static gboolean       module_register    (const gchar             *name);

static gint           jpkscan_detect     (const GwyFileDetectInfo *fileinfo,
                                          gboolean                 only_name);
static GwyContainer * jpkscan_load       (const gchar             *filename,
                                          GwyRunType               mode,
                                          GError                 **error);
static GwyContainer * jpkscan_load_tiff  (const gchar             *filename,
                                          GError                 **error);

static gboolean       tiff_check_version       (gint               macro,
                                                gint               micro,
                                                GError           **error);

static void           tiff_load_channel        (TIFF              *tif,
                                                GwyContainer      *container,
                                                gint               idx,
                                                gint               ilen,
                                                gint               jlen,
                                                gdouble            ulen,
                                                gdouble            vlen);

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


/* The module info. */
static GwyModuleInfo module_info =
{
  GWY_MODULE_ABI_VERSION,
  module_register,
  N_("Imports JPK image scans."),
  "Sven Neumann <neumann@jpk.com>",
  "0.3",
  "JPK Instruments AG",
  "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)


static gboolean
module_register (const gchar *name)
{
  gwy_file_func_register ("jpkscan",
                          N_("JPK image scans (.jpk)"),
                          (GwyFileDetectFunc)&jpkscan_detect,
                          (GwyFileLoadFunc)&jpkscan_load,
                          NULL,
                          NULL);

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
jpkscan_load (const gchar              *filename,
              G_GNUC_UNUSED GwyRunType  mode,
              GError                  **error)
{
  GwyContainer *container;

  gwy_debug ("Loading <%s>", filename);

  /*  Handling of custom tags was introduced with LibTIFF version 3.6.0  */
  if (! tiff_check_version (3, 6, error))
    return NULL;

  TIFFSetWarningHandler (tiff_warning);
  TIFFSetErrorHandler (tiff_error);

  container = jpkscan_load_tiff (filename, error);

  return container;
}

static GwyContainer *
jpkscan_load_tiff (const gchar  *filename,
                   GError      **error)
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
  if (! tif) {
      /* This can be I/O too, but it's hard to tell the difference. */
      err_FILE_TYPE (error, _("JPK scan"));
      return NULL;
  }

  /*  sanity check, grid dimensions must be present!  */
  if (! (tiff_get_custom_double (tif, JPK_TIFFTAG_Grid_uLength, &ulen) &&
         tiff_get_custom_double (tif, JPK_TIFFTAG_Grid_vLength, &vlen)))
    {
      TIFFClose (tif);
      g_set_error (error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                   _("File does not contain grid dimensions."));
      return NULL;
    }

  container = gwy_container_new ();

  /* FIXME: not yet, must sort file vs. channel
  tiff_load_meta (tif, container); */

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
tiff_check_version (gint     required_macro,
                    gint     required_micro,
                    GError **error)
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

      g_set_error (error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_SPECIFIC,
                   _("LibTIFF too old!\n\n"
                     "You are using %s. Please update to "
                     "libtiff version %d.%d or newer."),
                   version, required_macro, required_micro);
    }

  g_free (version);

  return result;
}

/* FIXME: this function could use some sort of failure indication, if the
 * file is damaged and no data field can be loaded, suspicionless caller can
 * return empty Container */
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
  GwySIUnit    *siunit;
  GString      *key;
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
          gwy_strequal(string, slot))
        {
          tiff_get_custom_string (tif, JPK_TIFFTAG_Scaling_Type(i), &string);
          g_return_if_fail (gwy_strequal(string, "LinearScaling"));

          tiff_get_custom_double (tif, JPK_TIFFTAG_Scaling_Multiply(i), &mult);
          tiff_get_custom_double (tif, JPK_TIFFTAG_Scaling_Offset(i), &offset);

          gwy_debug ("multipler: %g offset: %g", mult, offset);

          tiff_get_custom_string (tif, JPK_TIFFTAG_Encoder_Unit(i), &unit);

          break;
        }
    }

  /*  create a new data field  */

  dfield = gwy_data_field_new (ilen, jlen, ulen, vlen, FALSE);

  siunit = gwy_si_unit_new ("m");
  gwy_data_field_set_si_unit_xy (dfield, siunit);
  g_object_unref (siunit);

  if (unit)
    {
      siunit = gwy_si_unit_new (unit);
      gwy_data_field_set_si_unit_z (dfield, siunit);
      g_object_unref (siunit);
    }

  /*  read the scan data  */

  data = gwy_data_field_get_data (dfield);

  buffer = g_new (guchar, TIFFScanlineSize (tif));

  tiff_get_custom_boolean (tif, JPK_TIFFTAG_Grid_Reflect, &reflect);

  if (! reflect)
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
        data += ilen;
      else
        data -= ilen;
    }

  /*  add the GwyDataField to the container  */

  key = g_string_new ("");
  g_string_printf (key, "/%d/data", idx);
  gwy_container_set_object_by_name (container, key->str, dfield);
  g_object_unref (dfield);

  g_string_append (key, "/title");
  gwy_container_set_string_by_name (container, key->str, channel);
  g_string_free (key, TRUE);
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
      if (gwy_strequal(string, "contact"))
        {
          tiff_load_meta_double (tif, container,
                                 JPK_TIFFTAG_Feedback_Baseline, "V",
                                 "Feedback Baseline");
        }
      else if (gwy_strequal(string, "intermittent"))
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

/* TODO: pass the error message upstream, somehow */
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
jpkscan_meta_key (const gchar *desc)
{
  GQuark  quark;
  gchar  *key = g_strconcat ("/meta/", desc, NULL);

  quark = g_quark_from_string (key);

  g_free (key);

  return quark;
}

