/* $Id$
 * Copyright (C) 2004 Rok Zitko
 * E-mail: rok.zitko@ijs.si
 *
 * Based on nanoscope.c, Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 * E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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

/* TO DO:
 * - multiple images / selection dialog
 * - constant height or current
 * - saving
*/

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAGIC_TXT "[Parameter]"
#define MAGIC_SIZE (sizeof(MAGIC_TXT)-1)

static gboolean    module_register            (const gchar *name);
static gint        createc_detect             (const gchar *filename,
					       gboolean only_name);
static GwyContainer* createc_load             (const gchar *filename);
static GHashTable* read_hash                  (gchar *buffer);
static gchar*      next_line                  (gchar **buffer);
static GwyDataField* hash_to_data_field       (GHashTable *hash, 
					       gchar *buffer);
static gboolean    read_binary_data           (gint n,
					       gdouble *data,
					       gchar *buffer,
					       gint bpp);
static void store_metadata                    (GwyContainer *data, 
					       GHashTable *hash);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "createc",
    N_("Import Createc data files."),
    "Rok Zitko <rok.zitko@ijs.si>",
    "0.1",
    "Rok Zitko",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo createc_func_info = {
        "createc",
        N_("Createc files (.dat)"),
        (GwyFileDetectFunc)&createc_detect,
	(GwyFileLoadFunc)&createc_load,
        NULL
    };

    gwy_file_func_register(name, &createc_func_info);

    return TRUE;
}

static gint
createc_detect(const gchar *filename, 
	       gboolean only_name)
{
   gint score = 0;
   FILE *fh;
   gchar magic[MAGIC_SIZE];

   if (strstr(filename,".dat")) 
     score += 20;
   
   if (only_name) return score;
   
   if (!(fh = fopen(filename,"rb")))
     return 0;
   if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
       && memcmp(magic, MAGIC_TXT, MAGIC_SIZE) == 0)
     score = 100;
   fclose(fh);
   
   return score;
}

static void
  free_a_hash_table_entry(gpointer key, gpointer value, gpointer user_data)
{
   g_free(key); g_free(value);
}


static GwyContainer*
createc_load(const gchar *filename)
{
   GObject *object = NULL;
   gchar *buffer = NULL;
   gsize size = 0;
   GError *err = NULL;
   GHashTable *hash = NULL;
   GwyDataField *dfield;
   
   if (!g_file_get_contents(filename, &buffer, &size, &err))
     {
	g_warning("Cannot read file %s", filename);
	g_clear_error(&err);
	return NULL;
     }
   if (size < MAGIC_SIZE || memcmp(buffer, MAGIC_TXT, MAGIC_SIZE)!=0) 
     {
	g_warning("File %s is not a Createc image file", filename);
	g_free(buffer);
	return NULL;
     }
   
   hash = read_hash(buffer);
   
   dfield = hash_to_data_field(hash, buffer);
   
   object = gwy_container_new();
   gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
				    G_OBJECT(dfield));

   store_metadata(GWY_CONTAINER(object), hash);
   
   g_hash_table_foreach(hash, free_a_hash_table_entry, NULL);
   g_hash_table_destroy(hash);
   gwy_file_abandon_contents(buffer, size, &err);
   
   return (GwyContainer*)object;
}

/* Read the ASCII header and fill the hash with key/value pairs */
static GHashTable*
  read_hash(gchar *buffer)
{
   GHashTable *hash;
   gchar *line, *eq, *p;
   
   p = buffer;
   
   line = next_line(&p);
   if (!line) return NULL;
   if (!strstr(line, "[Parameter]")!=0) return NULL;
   
   hash = g_hash_table_new(g_str_hash, g_str_equal);
   while (p[0]) 
     {
	line = next_line(&p);
	if (!line) goto fail;
	eq = strchr(line, '=');
	if (!eq) goto fail;
	*eq = '\0';
	g_hash_table_insert(hash, g_strdup(line), g_strdup(eq + 1) );
	gwy_debug("<%s>: <%s>\n", line, eq + 1);
     }
   
   return hash;
   
fail:
   g_hash_table_destroy(hash);
   return NULL;
}

/* Macros to extract integer/double variables */
#define HASH_GET(key, var, typeconv) \
   if (!(s = g_hash_table_lookup(hash, key))) \
     { \
	g_warning("%s not found",key); \
	return NULL; \
     } \
   var = typeconv(s)

#define HASH_INT(key, var)    HASH_GET(key, var, atoi)
#define HASH_DOUBLE(key, var) HASH_GET(key, var, atof)
#define HASH_STRING(key, var) HASH_GET(key, var, /* */)
   
static GwyDataField*
  hash_to_data_field(GHashTable *hash, 
		     gchar *buffer)
{
   GwyDataField *dfield;
   GObject *unit;
   const gchar *s; /* for HASH_GET macros */
   gint xres, yres, bpp, offset;
   gdouble xreal, yreal, q;
   gboolean is_current;
   gdouble *data;
   gint ti1, ti2; gdouble td; /* temporary storage */
   
   bpp = 2; /* int16, always */
   offset = 16384 + 2; /* header + 2 offset bytes */
   is_current = 0;
   
   HASH_INT("Num.X", xres);
   HASH_INT("Num.Y", yres);

   HASH_INT("Delta X", ti1);
   HASH_INT("GainX", ti2);
   HASH_DOUBLE("Xpiezoconst", td); /* lowcase p, why? */
   xreal = xres * ti1; /* dacs */
   xreal *= 20.0/65536.0 * ti2; /* voltage per dac */
   xreal *= td * 1.0e-10; /* piezoconstant [A/V] */

   HASH_INT("Delta Y", ti1);
   HASH_INT("GainY", ti2);
   HASH_DOUBLE("YPiezoconst", td); /* upcase P */
   yreal = yres * ti1;
   yreal *= 20.0/65536.0 * ti2;
   yreal *= td * 1.0e-10;

   HASH_INT("GainZ", ti2);
   HASH_DOUBLE("ZPiezoconst", td); /* upcase P */
   q = 1.0; /* unity dac */
   q *= 20.0/65536.0 * ti2; /* voltage per dac */
   q *= td * 1.0e-10; /* piezoconstant [A/V] */
   
   dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal, FALSE));
   data = gwy_data_field_get_data(dfield);
   if ( !read_binary_data(xres*yres, data, buffer + offset, bpp) ) 
     {
	g_object_unref(dfield);
	return NULL;
     }
   gwy_data_field_multiply(dfield, q);
   
   unit = gwy_si_unit_new("m");
   gwy_data_field_set_si_unit_xy(dfield, GWY_SI_UNIT(unit));
   g_object_unref(unit);
                                                                                   
   unit = gwy_si_unit_new(is_current ? "A" : "m");
   gwy_data_field_set_si_unit_z(dfield, GWY_SI_UNIT(unit));
   g_object_unref(unit);
        
   return dfield;
}

/* Macro for storing meta data */

#define HASH_STORE(key) \
   if (!(val = g_hash_table_lookup(hash, key))) \
     { \
	g_warning("%s not found",key); \
     } \
   metakey = g_strdup_printf("/meta/%s", key); \
   gwy_debug("key = %s, val = %s\n", key, val); \
   gwy_container_set_string_by_name(data, metakey, g_strdup(val)); \
   g_free(metakey)

static void
  store_metadata(GwyContainer *data, GHashTable *hash)
{
   gchar *val, *metakey; /* for HASH_STORE macro */
   gchar *tobestored[]=
     {"Titel", "Delta X", "Delta Y", "Delay X+", "Delay X-",
	  "Delay Y", "Rotation", "GainX", "GainY", "GainZ",
	  "BiasVoltage", "Gainpreamp", "Chan(1,2,4)", "Scancoarse",
	  "FBIset", "FBRC", "ZPiezoconst", "Xpiezoconst", "YPiezoconst",
	  "Dactonmx", "Dactonmz", "Scantype", "memo:0",
	  "memo:1", "memo:2", NULL};
   gint ctr = 0;
   
   while (tobestored[ctr]) 
     {
	HASH_STORE(tobestored[ctr]);
	ctr++;
     }
}


/**
 *  * next_line:
 *  * @buffer: A character buffer containing some text.
 *  *
 *  * Extracts a next line from @buffer.
 *  *
 *  * @buffer is updated to point after the end of the line and the "\n"
 *  * (or "\r\n") is replaced with "\0", if present.
 *  *
 *  * Returns: The start of the line.  %NULL if the buffer is empty or %NULL.
 *  *          The line is not duplicated, the returned pointer points somewhere
 *  *          to @buffer.
 *  **/
static gchar*
  next_line(gchar **buffer)
{
   gchar *p, *q;

   if (!buffer || !*buffer)
     return NULL;
   
   q = *buffer;
   p = strchr(*buffer, '\n');
   if (p) 
     {
	if (p > *buffer && *(p-1) == '\r')
	  *(p-1) = '\0';
	*buffer = p+1;
	*p = '\0';
     }
   
   else
     *buffer = NULL;
   
   return q;
}

static gboolean
  read_binary_data(gint n, gdouble *data,
		   gchar *buffer,
		   gint bpp)
{
   gint i;
   gdouble q;
                                                                                   
   q = 1.0/(1 << (8*bpp));
   switch (bpp) 
     {
 case 1:
	for (i = 0; i < n; i++)
	  data[i] = q*buffer[i];
	break;
	
 case 2:
	  {
	     gint16 *p = (gint16*)buffer;
	     
	     for (i = 0; i < n; i++)
	       data[i] = q*p[i];
	  }
	break;
	
 case 4:
	  {
	     gint32 *p = (gint32*)buffer;
	                                                                                     
	     for (i = 0; i < n; i++)
	       data[i] = q*p[i];
	  }
	
	break;
	                                                                                
      default:
	g_warning("bpp = %d unimplemented", bpp);
	return FALSE;
	break;
     }
                                                                                   
   return TRUE;
}
