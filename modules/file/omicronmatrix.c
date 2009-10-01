/*
 *  Copyright (C) 2008, Philipp Rahe, David Necas
 *  E-mail: hquerquadrat@gmail.com
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of 
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public 
 *  License along with this program; if not, write to the Free 
 *  Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111 USA
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-omicron-matrix-spm">
 *   <comment>Omicron MATRIX SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="ONTMATRX0101TLKB"/>
 *   </magic>
 *   <glob pattern="*.mtrx"/>
 *   <glob pattern="*.MTRX"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Omicron MATRIX
 * .mtrx
 * Read
 **/

/* Version 0.81, 17.10.2008 */
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <libprocess/datafield.h>
#include "err.h"


#define FILEIDENT "ONTMATRX0101"
#define FILEIDENT_SIZE (sizeof(FILEIDENT)-1)

#define IMGFILEIDENT "ONTMATRX0101TLKB"
#define IMGFILEIDENT_SIZE (sizeof(IMGFILEIDENT)-1)

#define PARFILEIDENT "ONTMATRX0101ATEM"
#define PARFILEIDENT_SIZE (sizeof(PARFILEIDENT)-1)

#define EXTENSION_HEADER ".mtrx"

#define STRING_MAXLENGTH 10000


/* defining OSNAVERSION, as used in the AFM group in Osnabrueck 
 * inverts all df data and multiplies with 5.464 
 * you shouldn't use this unless you know what you are doing
 */
//#define OSNAVERSION 1

/** Stores data for quick access. 
 *  All supplement data is stored in a GwyContainer called meta
 */
typedef struct {
    guint32 xpoints;
    guint32 ypoints;
    gdouble width;
    gdouble height;
    gdouble zoom;
    gdouble rastertime;
    gint32  gridmode;

    // data for processing
    guint32 proc_cur_img_no;
    guint32 proc_intended_no;
    guint32 proc_available_no;

    // data during filereading
    guint32 state;

    // concerning the filename
    guint32 session;
    guint32 trace;
    gchar* channelname;
} MatrixData;

/** Transferfunctions for correct scaling of Z/Df/I/Ext2... data */
#define TFF_LINEAR1D 1
#define TFF_LINEAR1D_NAME "TFF_Linear1D"
#define TFF_MULTILINEAR1D 2
#define TFF_MULTILINEAR1D_NAME "TFF_MultiLinear1D"

/** stores information about scaling */
typedef struct {
    guint32 tfftype;
    gdouble factor_1;
    gdouble offset_1;
    gdouble neutralfactor_2;
    gdouble offset_2;
    gdouble prefactor_2;
    gdouble preoffset_2;
    gdouble raw1_2;
    gdouble whole_2;
    guint32 cnumber;
    gchar* channelname;
} ZScaling;

/** States during parsing of parameterfile */
#define IMAGE_FOUND 1
#define UNKNOWN 0
#define FILE_END 2

/** Datatypes for MATRIX files */
#define OMICRON_UINT32 1
#define OMICRON_DOUBLE 2
#define OMICRON_CHAR 3
#define OMICRON_BOOL 4


static gboolean    module_register    (void);
static gint        matrix_detect      (const GwyFileDetectInfo *fi,
                                            gboolean only_name);
static gchar*      matrix_readstring  (const guchar** buffer,
                                            guint32* size);
static guint32     matrix_readdata_to_container 
                                      (const guchar** fp,
                                            const gchar* name,
                                            const gchar* metaname,
                                            GwyContainer* container,
                                            GwyContainer* meta,
                                            guint32 check);
static guint32     matrix_readdata    (void* data,
                                            const guchar** fp,
                                            guint32 check);
static guint32     matrix_scanparamfile  (const guchar** buffer,
                                            GwyContainer* container,
                                            GwyContainer* meta,
                                            MatrixData* matrixdata);
static guint32     matrix_scanimagefile  (const guchar** buffer,
                                            GwyContainer* container,
                                            GwyContainer* meta,
                                            MatrixData* matrixdata,
                                            gboolean useparamfile);
static GwyContainer* matrix_load         (const gchar* filename,
                                            GwyRunType mode,
                                            GError** error);
static gdouble     matrix_tff            (gint32 value,
                                            ZScaling* scale);

/** calculates the correct physical value using the 
 *  corresponding transfer function
 */
static gdouble matrix_tff(gint32 v, ZScaling* s) {
    if(s->tfftype == TFF_LINEAR1D) {
      // use linear1d: p = (r - n)/f
      return ((gdouble)v - s->offset_1)/s->factor_1;
    } else if(s->tfftype == TFF_MULTILINEAR1D) {
      // use multilinear1d:
      // p = (r - n)*(r0 - n0)/(fn * f0)
      //   = (r - n)*s.whole_2
      return ((gdouble)v - s->preoffset_2)*s->whole_2;
    } else {
      // unknown tff
      g_warning("unknown transfer function, scaling will be wrong");
      return (gdouble)v;
    }
}



static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Omicron MATRIX (param.mtrx & data.mtrx)"),
    "Philipp Rahe <hquerquadrat@gmail.com>",
#ifdef OSNAVERSION
    "0.82-Osnabruck",
#else
    "0.82",
#endif
    "Philipp Rahe",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean module_register(void)
{
    gwy_file_func_register("omicronmatrix",
                           N_("Omicron MATRIX (.mtrx & .mtrx)"),
                           (GwyFileDetectFunc)&matrix_detect,
                           (GwyFileLoadFunc)&matrix_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint matrix_detect(const GwyFileDetectInfo *fileinfo,
                          gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, 
                                EXTENSION_HEADER)
               ? 15 : 0;

    if(fileinfo->buffer_len > IMGFILEIDENT_SIZE &&
       0 == memcmp(fileinfo->head, IMGFILEIDENT, IMGFILEIDENT_SIZE))
         return 100;
    return 0;
}



/** read a string from the paramter or data file
 *  remember to free the result! */
static gchar* matrix_readstring(const guchar** fp, 
                                guint32* size) {
    gchar* str = NULL;
    // len is the number of characters (each 16Bit) encoded
    guint32 len;
    GError* tmperr = NULL;

    len = gwy_get_guint32_le(fp);
    if(len == 0) {
        return g_strdup("");
    }
    if(len > STRING_MAXLENGTH) {
      g_warning("omicronmatrix::matrix_readstring:"
                " len>STRING_MAXLENGTH, string not readable");
      return NULL;
    }
    str = g_utf16_to_utf8((gunichar2*)*fp, len, NULL, NULL, &tmperr);
    if(tmperr != NULL) {
      g_warning("omicronmatrix::matrix_readstring:"
                " error reading or converting string");
      g_error_free(tmperr);
      *fp += 2*len;
      return str;
    } else {
        // advance by length in gchar
        *fp += 2*len;
        if(size != NULL) *size = len;
        return str;
    }
}



/** Reads the next datafield and store it in the container 
 *  at key <name>.
 *  If <metacontainer>!=NULL, it is also stored here.
 *  These fields have a identifier in front.
 */
static guint32 matrix_readdata_to_container(const guchar** fp, 
                                          const gchar* name,
                                          const gchar* metaname,
                                          GwyContainer* container,
                                          GwyContainer* metacontainer,
                                          guint32 check)
{
    gchar* id = NULL;
    if(check == 1) {
        guint32 a = gwy_get_guint32_le(fp);
        if(a != 0 ) {
            *fp -= 4;
            gwy_container_set_int32_by_name(container, name, a);
            if(metacontainer != NULL) {
              gchar val[30];
              g_snprintf(val, sizeof(val), "%u", a);
              gwy_container_set_string_by_name(metacontainer, metaname,
                                               (guchar*)g_strdup(val));
            }
            return 0;
        }
    }

    id = g_strndup((gchar*)*fp, 4);
    *fp += 4;
    if(0 == strncmp(id, "GNOL", 4)) {
        // UInt32
        guint32 v = gwy_get_guint32_le(fp);
        gwy_container_set_int32_by_name(container, name, v);
        if(metacontainer != NULL) {
          gchar val[30];
          g_snprintf(val, sizeof(val), "%u", v);
          gwy_container_set_string_by_name(metacontainer, 
                                           metaname, 
                                           (guchar*)g_strdup(val));
        }
        g_free(id);
        return 0;
    } else if(0 == strncmp(id, "LOOB", 4)) {
        // bool, 32bit
        guint32 a = gwy_get_guint32_le(fp);
        gwy_container_set_boolean_by_name(container, name, a != 0);
        if(metacontainer != NULL) {
          gchar val[30];
          g_snprintf(val, sizeof(val), "%i", a);
          gwy_container_set_string_by_name(metacontainer, 
                                           metaname, 
                                           (guchar*)g_strdup(val));
        }
        g_free(id);
        return 0;
    } else if(0 == strncmp(id, "BUOD", 4)) {
        // double, 32bit
        gdouble v = gwy_get_gdouble_le(fp);
        gwy_container_set_double_by_name(container, name, v);
        if(metacontainer != NULL) {
          gchar val[30];
          g_snprintf(val, sizeof(val), "%e", v);
          gwy_container_set_string_by_name(metacontainer, 
                                           metaname, 
                                           (guchar*)g_strdup(val));
        }
        g_free(id);
        return 0;
    } else if(0 == strncmp(id, "GRTS", 4)) {
        // string
        gchar* str;
        str = matrix_readstring(fp, NULL);
        gwy_container_set_string_by_name(container, 
                                         name, (guchar*)str);
        if(metacontainer != NULL) {
          gwy_container_set_string_by_name(metacontainer, 
                                           metaname, 
                                           (guchar*)g_strdup(str));
        }
        g_free(id);
        return 0;
    }
    return 1;
}


/** same as matrix_readdata_to_container, but simply returns
 *  the value in <data>. 
 *  be careful: Provide enough memory
 */
static guint32 matrix_readdata(void* data, 
                             const guchar** fp, 
                             guint32 check)
{
    gchar* id = NULL;
    guint32 uintval = 0;
    gdouble dval = 1.0;
    gboolean boolval = FALSE;

    if(check == 1) {
        guint32 a = gwy_get_guint32_le(fp);
        if(a != 0) {
            *fp -= 4;
        if(data != NULL && sizeof(data) == sizeof(guint32)) {
          *((guint32*)(data)) = a;
        } else {
          g_warning("omicronmatrix::matrix_readdata:"
                    " datafield not readable");
        }
            return OMICRON_UINT32;
        }
    }

    id = g_strndup((gchar*)*fp, 4);
    *fp += 4;
    if(0 == strncmp(id, "GNOL", 4)) {
        // UInt32
                  uintval = gwy_get_guint32_le(fp);
        if(data != NULL) {
          *((guint32*)(data)) = uintval;
        } else {
          g_warning("omicronmatrix::matrix_readdata:"
                    " datafield not readable");
        }
        g_free(id);
        return OMICRON_UINT32;
    } else if(0 == strncmp(id, "LOOB", 4)) {
        // bool, 32bit
        guint32 a = gwy_get_guint32_le(fp);
        boolval = (a != 0);
        if(data != NULL) {
          *((gboolean*)(data)) = boolval;
        } else {
          g_warning("omicronmatrix::matrix_readdata:"
                    " datafield not readable");
        }
        g_free(id);
        return OMICRON_BOOL;
    } else if(0 == strncmp(id, "BUOD", 4)) {
        // double, 32bit
        dval = gwy_get_gdouble_le(fp);
        if(data != NULL) {
          *((gdouble*)(data)) = dval;
        } else {
          g_warning("omicronmatrix::matrix_readdata:"
                    " datafield not readable");
        }
        g_free(id);
        return OMICRON_DOUBLE;
    } else if(0 == strncmp(id, "GRTS", 4)) {
        // string
        gchar* locdata = matrix_readstring(fp, NULL);
        if(data != NULL) {
          data = locdata;
        } else {
          g_free(locdata);
          g_warning("omicronmatrix::matrix_readdata:"
                    " datafield not readable");
        }
        g_free(id);
        return OMICRON_CHAR;
    }
    return 1;
}


    
/** Scans OMICRON MATRIX parameterfiles
 */
static guint32 matrix_scanparamfile(const guchar** infile, 
                              GwyContainer* container,
                              GwyContainer* meta,
                              MatrixData* matrixdata)
{
  const guchar* fp = NULL;
  gchar* ident = NULL;
  gint32 len;

  if(matrixdata != NULL && (matrixdata->state == 1 || 
                            matrixdata->state == 2)) {
      /* File end reached or image has been found. 
         Do not proceed with parsing the parameter file
       */
      return 0;
  }
  // use local fp, 
  // advance infile in the end by len
  fp = *infile;
  // read block identifier and advance buffer by 4
  ident = g_strndup((gchar*)fp, 4);
  fp += 4;

  /* next 4B are the length of following block in Bytes. 
   * As infile points before the identifier,
   * advance by 8B more
   */
  len = gwy_get_guint32_le(&fp) + 8;
  gwy_debug("omicronmatrix::matrix_scanparamfile: %s, len: %u", 
            ident, len);

  if(strncmp(ident, "REFX", 4) && strncmp(ident, "NACS", 4)
    && strncmp(ident, "TCID", 4) && strncmp(ident, "SCHC", 4)
    && strncmp(ident, "TSNI", 4) && strncmp(ident, "SXNC", 4)
    && strncmp(ident, "LNEG", 4)) {
    /* In the following blocks the timestamp is available */
    /* these are the blocks, which are NOT listed above */
    /* timestamp is time_t with 8B */
      //guint64 longtime = gwy_get_guint64_le(&fp);
      fp += 8;
      len += 8;
  } else {
      /* No timestamp available,
         but perhaps one is stored in timestamp
         from scanning before */
  }
  

  if( 0 == strncmp(ident, "ATEM", 4)) {
    // Data at beginning of parameter file
    gchar* programmname = NULL;
    gchar* version = NULL;
    gchar* profil = NULL;
    gchar* user = NULL;

    // program 
    programmname = matrix_readstring(&fp, NULL);
    gwy_container_set_string_by_name(meta, 
        "META: Program", (guchar*)programmname);
    // version
    version = matrix_readstring(&fp, NULL);
    gwy_container_set_string_by_name(meta,
                     "META: Version", (guchar*)version);                
    fp += 4;
    // profile name
    profil = matrix_readstring(&fp, NULL);
    gwy_container_set_string_by_name(meta,
                     "META: Profil", (guchar*)profil);                
    // username
    user = matrix_readstring(&fp, NULL);
    gwy_container_set_string_by_name(meta,
                     "META: User", (guchar*)user);                

  } else if( 0 == strncmp(ident, "DPXE", 4)) {
    // Description and project files
    guint32 i=0;  
    fp += 4;  
    for(i=0; i<7; i++) {
      // read 7 strings
      gchar* s1 = NULL;
      gchar key[30];
      g_snprintf(key, sizeof(key), "EXPD: s%d", i);
      s1 = matrix_readstring(&fp, NULL);
      gwy_container_set_string_by_name(meta, g_strdup(key), 
                                       (guchar*)s1);
    }

  }else if(0 == strncmp(ident, "QESF", 4)) {

  }else if(0 == strncmp(ident, "SPXE", 4)) {
    // Initial Configuration of the OMICRON system
    fp += 4; 
    
    while(fp - *infile < len) {
        matrix_scanparamfile(&fp, 
                           container,
                           meta,
                           matrixdata);
    }

  }else if( 0 == strncmp(ident, "LNEG", 4)) {
    // description
    guint32 i = 0;
    for (i = 0; i<3; i++) {
      // read strings
      gchar* s1 = NULL;
      gchar key[30];
      g_snprintf(key, sizeof(key), "GENL: s%d", i);
      s1 = matrix_readstring(&fp, NULL);
      gwy_container_set_string_by_name(meta, g_strdup(key), 
                                       (guchar*)s1);   
    }

  }else if(0 == strncmp(ident, "TSNI", 4)) {
    // configuration of instances
    guint32 anz = gwy_get_guint32_le(&fp);
    guint32 i = 0;
    for(i=0; i<anz; i++) {
      /* Instance and Elements are following */
      gchar* s1 = NULL;
      gchar* s2 = NULL;
      gchar* s3 = NULL;
      gchar key[100];
      guint32 count;

      s1 = matrix_readstring(&fp, NULL);
      s2 = matrix_readstring(&fp, NULL);
      s3 = matrix_readstring(&fp, NULL);

      g_snprintf(key, sizeof(key), "TSNI:%s::%s(%s)", s1, s2, s3);

      /* Number of following properties to instance */
      count = gwy_get_guint32_le(&fp);  
      while ( count > 0) {
        gchar* t1 = NULL;
        gchar* t2 = NULL;
        gchar key2[100];

        t1 = matrix_readstring(&fp, NULL);
        t2 = matrix_readstring(&fp, NULL);
        g_snprintf(key2, sizeof(key2), "%s.%s", key, t1);
        gwy_container_set_string_by_name(meta, g_strdup(key2), 
                                         (guchar*)t2);
        if(t1 != NULL) g_free(t1);

        count--;
      }
      if(s1 != NULL) g_free(s1);
      if(s2 != NULL) g_free(s2);
      if(s3 != NULL) g_free(s3);
    }
            
  }else if(FALSE && 0 == strncmp(ident, "SXNC", 4)) {
    // configuration of boards
    // not relevant for correct opening
    guint32 count = 0;
    guint32 i = 0;

    count = gwy_get_guint32_le(&fp);
    for(i=0; i<count; i++) {
      /* Name and state */
      // read two strings
      // read an int: number of following groups of
      //   two strings
    }


  }else if(0 == strncmp(ident, "APEE", 4)) {
    // configuration of experiment
    // altered values are recorded in PMOD
    // the most important parts are in XYScanner
    gchar* inst = NULL;
    gchar* prop = NULL;
    gchar* unit = NULL;
    guint32 a, charlen, restype;
    gdouble doubleval;
    guint32 uint32val;
    guint32 gnum;
    gboolean checksub = FALSE;
    gchar val[30];
    fp += 4;
    gnum = gwy_get_guint32_le(&fp);


    while(gnum > 0) {
      inst = matrix_readstring(&fp, &charlen);
        if(0 == strcmp(inst, "XYScanner")) {
          checksub = TRUE;
        } else {
          checksub = FALSE;
        }
      /* next 4B are number of Group items */
      a = gwy_get_guint32_le(&fp);
      while(a > 0) {
        prop = matrix_readstring(&fp, NULL);  
        unit = matrix_readstring(&fp, NULL);
          if(checksub) {
            if(0 == strcmp(prop, "Height")) { 
              // image height in m, MATRIX 1.0 and 2.x
              restype = matrix_readdata(&doubleval, &fp, 1);
              if(restype != OMICRON_DOUBLE) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " height unreadable");
                  matrixdata->height = 1;
              } else {
                  matrixdata->height = doubleval;
                  if(meta != NULL) {
                      g_snprintf(val, sizeof(val), "%e", doubleval);
                      gwy_container_set_string_by_name(meta, 
                         g_strconcat("EEPA:",inst, ".", prop, 
                                     " [", unit, "]", NULL), 
                         (guchar*)g_strdup(val));
                  }
              }

            } else if(0 == strcmp("Width", prop)) {
              // image width in m, MATRIX 1.0 and 2.x
              restype = matrix_readdata(&doubleval, &fp, 1);
              if(restype != OMICRON_DOUBLE) {
                 g_warning("omicronmatrix::matrix_scanparamfile:"
                           " width unreadable");
                 matrixdata->width = 1;
                
              } else {
                matrixdata->width = doubleval;
                if(meta != NULL) {
                   g_snprintf(val, sizeof(val), "%e", doubleval);
                   gwy_container_set_string_by_name(meta, 
                     g_strconcat("EEPA:",inst, ".", prop, 
                                 " [", unit, "]", NULL), 
                     (guchar*)g_strdup(val));
                }
              }

            } else if(0 == strcmp("X_Points", prop) || 
                      0 == strcmp("Points", prop)) {
              // Image points in x direction, MATRIX 1.0
              restype = matrix_readdata(&uint32val, &fp, 1);
              if(restype != OMICRON_UINT32) {
                 g_warning("omicronmatrix::matrix_scanparamfile:"
                           " xpoints unreadable");
                 matrixdata->xpoints = 0;
              } else {
                matrixdata->xpoints = uint32val;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%u", uint32val);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }

            } else if(0 == strcmp("Y_Points", prop) ||
                      0 == strcmp("Lines", prop)) {
              // Image points in y direction
              restype = matrix_readdata(&uint32val, &fp, 1);
               if(restype != OMICRON_UINT32) {
                   g_warning("omicronmatrix::matrix_scanparamfile:"
                             " ypoints unreadable");
                   matrixdata->ypoints = 0;
               } else {
                matrixdata->ypoints = uint32val;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%u", uint32val);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
               }

            } else if(0 == strcmp("Raster_Period_Time", prop) ||
                      0 == strcmp("Raster_Time", prop)) {
              // Rastertime in sec  
              restype = matrix_readdata(&doubleval, &fp, 1);
              if(restype != OMICRON_DOUBLE) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " rastertime unreadable");
                  matrixdata->rastertime = 1;
              } else {
                matrixdata->rastertime = doubleval;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%e", doubleval);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }
            } else if(0 == strcmp("Grid_Mode", prop) ||
                      0 == strcmp("Scan_Constraint", prop)) {
              // 0: Constraint none
              // 1: Constraint Line
              // 2: Constraint Point  
              restype = matrix_readdata(&uint32val, &fp, 1);
              if(restype != OMICRON_UINT32) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " gridmode unreadable");
                  matrixdata->gridmode = 0;
              } else {
                matrixdata->gridmode = uint32val;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%u", uint32val);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,  
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }

            } else if(0 == strcmp("Zoom", prop)) {
              // Zoomfactor
              restype = matrix_readdata(&uint32val, &fp, 1);
              if(restype != OMICRON_UINT32) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " zoom unreadable");
                  matrixdata->zoom = 1;
              } else {
                matrixdata->zoom = uint32val;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%u", uint32val);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }
            } else {
              // any other block inside XYScanner
              matrix_readdata_to_container(&fp,  
                 g_strconcat("/0/meta/",inst, ".", prop, NULL),
                 g_strconcat("EEPA:",inst, ".", prop, " [", unit, "]", NULL),
                 container, meta, 1);
            }
          } else {
            // any other block  
            matrix_readdata_to_container(&fp,  
                 g_strconcat("/0/meta/",inst, ".", prop, NULL),
                 g_strconcat("EEPA:",inst, ".", prop, " [", unit, "]", NULL),
                 container, meta, 1);
          }
        a -= 1;
        g_free(prop);
        g_free(unit);
      } // while a>0
      g_free(inst);
      gnum--;
    } // while gnum > 0

    }else if(0 == strncmp(ident, "DOMP", 4)) {
      // modified parameter during scanning
      // Changed configuration of APEE
      // parametername, unit, value
      gchar* inst = NULL;
      gchar* prop = NULL;
      gchar* unit = NULL;
      guint32 restype, uint32val;
      gdouble doubleval;
      gchar val[30];

      fp += 4; 
      // read two strings: instance, propertiy
      inst = matrix_readstring(&fp, NULL);
      prop = matrix_readstring(&fp, NULL);
      unit = matrix_readstring(&fp, NULL);
      if(0 == strcmp(inst, "XYScanner")) {

        // Possible important change
        if(0 == strcmp(prop, "Height")) {
              // image height in m
              restype = matrix_readdata(&doubleval, &fp, 1);
              if(restype != OMICRON_DOUBLE) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " height unreadable");
                  matrixdata->height = 1.0;
              } else {
                matrixdata->height = doubleval;
                if(meta != NULL) {
                      g_snprintf(val, sizeof(val), "%e", doubleval);
                      gwy_container_set_string_by_name(meta, 
                         g_strconcat("EEPA:",inst, ".", prop,
                                     " [", unit, "]", NULL), 
                         (guchar*)g_strdup(val));
                  }
              }

            } else if(0 == strcmp("Width", prop)) {
              // image width in m
              restype = matrix_readdata(&doubleval, &fp, 1);
              if(restype != OMICRON_DOUBLE) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " width unreadable");
                  matrixdata->width = 1.0;
              } else {
                matrixdata->width = doubleval;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%e", doubleval);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }

            } else if(0 == strcmp("X_Points", prop) ||
                      0 == strcmp("Points", prop)) {
              // Image points in x direction
              restype = matrix_readdata(&uint32val, &fp, 1);
              if(restype != OMICRON_UINT32) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " xpoints unreadable");
                  matrixdata->xpoints = 0;
              } else {
                matrixdata->xpoints = uint32val;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%u", uint32val);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }

            } else if(0 == strcmp("Y_Points", prop) ||
                      0 == strcmp("Lines", prop)) {
              // Image points in y direction
              restype = matrix_readdata(&uint32val, &fp, 1);
              if(restype != OMICRON_UINT32) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " ypoints unreadable");
                  matrixdata->ypoints = 0;
              } else {
                matrixdata->ypoints = uint32val;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%u", uint32val);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }

            } else if(0 == strcmp("Raster_Period_Time", prop) ||
                      0 == strcmp("Raster_Time", prop)) {
              // Rastertime in sec   
              restype = matrix_readdata(&doubleval, &fp, 1);
              if(restype != OMICRON_DOUBLE) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " rastertime unreadable");
                  matrixdata->rastertime = 0;
              } else {
                matrixdata->rastertime = doubleval;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%e", doubleval);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }

            } else if(0 == strcmp("Grid_Mode", prop) ||
                      0 == strcmp("Scan_Constraint", prop)) {
              // 0: Constraint none
              // 1: Constraint Line
              // 2: Constraint Point  
              restype = matrix_readdata(&uint32val, &fp, 1);
              if(restype != OMICRON_UINT32) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " gridmode unreadable");
                  matrixdata->gridmode = 0;
              } else {
                matrixdata->gridmode = uint32val;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%u", uint32val);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }
  

            } else if(0 == strcmp("Zoom", prop)) {
              // Zoomfactor
              restype = matrix_readdata(&uint32val, &fp, 1);
              if(restype != OMICRON_UINT32) {
                  g_warning("omicronmatrix::matrix_scanparamfile:"
                            " zoom unreadable");
                  matrixdata->zoom = 1;
              } else {
                matrixdata->zoom = uint32val;
                if(meta != NULL) {
                  g_snprintf(val, sizeof(val), "%u", uint32val);
                  gwy_container_set_string_by_name(meta, 
                    g_strconcat("EEPA:",inst, ".", prop,
                                " [", unit, "]", NULL), 
                    (guchar*)g_strdup(val));
                }
              }

            }
      }
      // write to container as well  
      matrix_readdata_to_container(&fp, 
                   g_strconcat("/meta/eepa/",inst, ".", prop, NULL),
                   g_strconcat("EEPA: ", inst, ".",
                               prop, " [", unit, "]", NULL),
                   container, meta, 1);
      g_free(inst);
      g_free(prop);
      g_free(unit);

  }else if(FALSE && 0 == strncmp(ident, "ICNI", 4)) {
    // State of Experiment 
    // 4B 0x00 and following number
    
  }else if( 0 == strncmp(ident, "KRAM", 4)) {
    // Calibration of system
    gchar* cal = NULL;
    cal = matrix_readstring(&fp, NULL);
    gwy_container_set_string_by_name(meta,
                           "MARK: Calibration", (guchar*)cal); 

  }else if(FALSE && 0 == strncmp(ident, "WEIV", 4)) {
    // deals with the scanning windows
  
  }else if(FALSE && 0 == strncmp(ident, "CORP", 4)) {
    // Processors of the scanning windows
    
  }else if(0 == strncmp(ident, "FERB", 4)) {
    gchar* filename = NULL;
    const gchar* savedname = NULL;
    // Filename of images
    fp += 4;  
    filename = matrix_readstring(&fp, NULL);
    savedname = 
          (gchar*)gwy_container_get_string_by_name(container,
                                   "/meta/imagefilename");
    if(g_str_has_suffix(savedname, filename) || 
       g_str_has_suffix(filename, savedname)) {
         // Image is found
         // the valid values are now in matrixdata
         matrixdata->state = IMAGE_FOUND;
    }
    g_free(filename);
        
  }else if(0 == strncmp(ident, "YSCC", 4)) {
    // Unknown block
    fp += 4;  
    while(fp - *infile < len) {
      // has inner blocks TCID, SCHC, NACS, REFX
      matrix_scanparamfile(&fp, container, meta, matrixdata);
    }

  }else if(0 == strncmp(ident, "TCID", 4)) {
    // description and internal number of captured channels
    // has to be linkend to the physical devices 
    // given in XFER to get the scaling
    gchar* s1 = NULL;
    gchar* s2 = NULL;
    guint32 a, number, i;
    // No timestamp, advance 8B
    fp += 8;
    number = gwy_get_guint32_le(&fp);
    for(i=0; i<number; i++) {
      // whatever the following is
      fp += 16;
      s1 = matrix_readstring(&fp, NULL);
      s2 = matrix_readstring(&fp, NULL);
      g_free(s1);
      g_free(s2);
    }
    // Number of channels
    number = gwy_get_guint32_le(&fp);
    for(i=0; i<number; i++) {
      gchar* name = NULL;
      gchar* unit = NULL;
      gchar key[30];
      fp += 4;
      a = gwy_get_guint32_le(&fp);
      fp += 8;
      name = matrix_readstring(&fp, NULL);
      unit = matrix_readstring(&fp, NULL);
      // store information in GwyContainer
      g_snprintf(key, sizeof(key), "/channels/%u/", a);
      gwy_container_set_string_by_name(container,
                   g_strconcat(key, "name", NULL),
                   (guchar*)name);
      gwy_container_set_string_by_name(container,
                   g_strconcat(key, "unit", NULL),
                   (guchar*)unit);
    }

  }else if( 0 == strncmp(ident, "SCHC", 4)) {
    // header of triangle curves

  }else if( 0 == strncmp(ident, "NACS", 4)) {
    // data of triangle curves

  }else if(0 == strncmp(ident, "REFX", 4)) {
    // data after triangle curves,
    // these are factors for scaling, given for the physical devices
    guint32 number;
    //gdouble value, factor, offset;
    guint32 i, a;
    while(fp - *infile < len) {
      gchar* name = NULL;
      gchar* unit = NULL;
      gchar key[30];
      fp += 4;  
      number = gwy_get_guint32_le(&fp);
      name = matrix_readstring(&fp, NULL);
      g_snprintf(key, sizeof(key), "/channels/%u/tff", number);
      // set string by name requires gchar* key
      gwy_container_set_string_by_name(container, 
                      g_strdup(key), (guchar*)g_strdup(name));
      unit = matrix_readstring(&fp, NULL);
      a = gwy_get_guint32_le(&fp);
      for(i=0; i<a; i++) {
        gchar* prop = NULL;
        prop = matrix_readstring(&fp, NULL);
        g_snprintf(key, sizeof(key), "/channels/%u/%s", number, prop);
        matrix_readdata_to_container(&fp, g_strdup(key), NULL,
                                     container, NULL, 0);
        g_free(prop);
      }
      g_free(name);
      g_free(unit);
    }

  }else if(0 == strncmp(ident, "DEOE", 4)) {
    // End of file
    matrixdata->state = FILE_END;
    g_free(ident);
    return 0;
  }
  g_free(ident);
  *infile += len;
  return 1;
}

/** Find the correct scaling for one channel
 */
static void matrix_foreach(gpointer key, 
                           gpointer value, gpointer data) 
{
  const gchar*  sval = NULL;
  ZScaling* zscale = NULL;
  gchar** split = NULL;

  zscale = (ZScaling*)data;
  if(G_VALUE_HOLDS(value, G_TYPE_STRING)) {
    sval = g_value_get_string(value);
    split = g_strsplit(g_quark_to_string(GPOINTER_TO_UINT(key)), 
                       "/", 4);
    /* split+1 = channels, split+2 = number, 
       split+3 = name/unit/factor/offset      */
    if(0 == strcmp("name", *(split+3)) && 
       0 == strcmp(zscale->channelname, sval)) {
         // corresponding factor, offset and unit found!
         zscale->cnumber = atoi(*(split+2));
    }
  } 
}


/** scanimagefile
  * reads an OMICRON data/image file
  */
static guint32 matrix_scanimagefile(const guchar** fp, 
                                GwyContainer* container, 
                                GwyContainer* meta,
                                MatrixData* matrixdata,
                                gboolean useparamfile
                             )
{
    gchar* ident;
    guint32 len;

    ident = g_strndup((gchar*)*fp, 4);
    *fp += 4;
    len = gwy_get_guint32_le(fp);
    gwy_debug("omicronmatrix::matrix_scanimagefile: %s, length: %d",
              ident, len);

    if(matrixdata->xpoints == 0 || matrixdata->ypoints == 0) {
        // parameters are not correct. Use those from the image file
        useparamfile = FALSE;
    }

    if (0 == strncmp(ident, "TLKB", 4)) {
        // ImageFile
        // next 8B: timestamp
        gchar times[40];
        guint64 date = gwy_get_guint64_le(fp);
        time_t timestamp = date;
        struct tm* sdate = localtime(&timestamp);
        strftime(times, sizeof(times), "%H:%M:%S %d.%m.%Y", sdate);
        //g_snprintf(times, sizeof(times), "%i", date);
        gwy_container_set_string_by_name(meta, 
                        "! Image ended at", (guchar*)g_strdup(times));
        len += 8;
        *fp += 4;
        while (0 != matrix_scanimagefile(fp, 
                                       container, 
                                       meta,
                                       matrixdata,
                                       useparamfile)
              ) 
        { // scans imagefile 
        }
    } else if (0 == strncmp(ident, "CSED", 4)) {
        // headerdata
        // the next 20 B are unknown
        *fp+= 20;
        // intended number of points
        matrixdata->proc_intended_no = gwy_get_guint32_le(fp); 
        // captured number of points
        matrixdata->proc_available_no = gwy_get_guint32_le(fp); 
        *fp += len -20-4-4;

    } else if (0 == strncmp(ident, "ATAD", 4)) {
        // Image data
        // GwyDataField for TraceUp, ReTraceUp, TraceDown, ReTraceDown
        GwyDataField* dfield_tup = NULL;
        GwyDataField* dfield_retup = NULL;
        GwyDataField* dfield_tdown = NULL;
        GwyDataField* dfield_retdown = NULL;
        // as well as pointer
        gdouble* data_tup = NULL;
        gdouble* data_retup = NULL;
        gdouble* data_tdown = NULL;
        gdouble* data_retdown = NULL;
        // and indices
        guint32 ind_tup, ind_retup, ind_tdown, ind_retdown;
        guint32 xres, yres, maxint, cntl, cntp, n, avail;
        gdouble width, height;
        GwySIUnit* unit;
        const gchar* sunit;
        gdouble fac = 1.0;
        ZScaling zscale;
        gchar key[40];
        gchar msg[100];
        gchar inverted[15];

        g_snprintf(inverted, sizeof(inverted), "");

        if(useparamfile) {
            xres = matrixdata->xpoints;
            yres = matrixdata->ypoints;
            width = matrixdata->width/(gdouble)matrixdata->zoom;
            height = matrixdata->height/(gdouble)matrixdata->zoom;
        } else {
            // guess sizes
            xres = (guint32)floor(sqrt(matrixdata->proc_intended_no/4.));
            yres = (guint32)floor(sqrt(matrixdata->proc_intended_no/4.));
            width = 1;
            height = 1;
            matrixdata->gridmode = 0;
            g_warning("omicronmatrix::matrix_scanimagefile:"
                      " image sizes probably incorrect");
        }
        
        if(matrixdata->gridmode == 2) {
            // Constraint Point
            dfield_tup = gwy_data_field_new(xres, yres, 
                                            width, height, FALSE);
            data_tup = gwy_data_field_get_data(dfield_tup);
            ind_tup     = xres*(yres-1);
            ind_retup   = ind_tdown = ind_retdown = 0;
            maxint = xres*yres;
            n = 0;
            avail = matrixdata->proc_available_no;

        } else if(matrixdata->gridmode == 1) {
            // Constraint Line
            dfield_tup = gwy_data_field_new(xres, yres, 
                                            width, height, FALSE);
            data_tup = gwy_data_field_get_data(dfield_tup);
            dfield_retup = gwy_data_field_new(xres, yres, 
                                              width, height, FALSE);
            data_retup = gwy_data_field_get_data(dfield_retup);
            ind_tup     = xres*(yres-1);
            ind_retup   = xres*yres - 1;
            ind_tdown   = ind_retdown = 0;
            maxint = 2*xres*yres;
            n = 0;
            avail = matrixdata->proc_available_no;

        } else {
            // Constraint None
            dfield_tup = gwy_data_field_new(xres, yres,
                                            width, height, FALSE);
            data_tup = gwy_data_field_get_data(dfield_tup);
            dfield_retup = gwy_data_field_new(xres, yres,
                                              width, height, FALSE);
            data_retup = gwy_data_field_get_data(dfield_retup);
            dfield_tdown = gwy_data_field_new(xres, yres,
                                              width, height, FALSE);
            data_tdown = gwy_data_field_get_data(dfield_tdown);
            dfield_retdown = gwy_data_field_new(xres, yres,
                                                width, height, FALSE);
            data_retdown = gwy_data_field_get_data(dfield_retdown);
            ind_tup     = xres*(yres-1);
            ind_retup   = xres*yres - 1;
            ind_tdown   = 0;
            ind_retdown = xres - 1;
            maxint = 4*xres*yres;
            n = 0;
            avail = matrixdata->proc_available_no;

        }
        if(useparamfile) {
            // Get correct scaling factor
            zscale.channelname = matrixdata->channelname;
            // look for correct Z/I/Df/.... scaling
            gwy_container_foreach(container, "/channels/", 
                                    matrix_foreach, &zscale);
            gwy_debug("omiconmatrix::matrix_loadimagefile:"
                      " Channel found, getting the data");
            g_snprintf(key, sizeof(key), "/channels/%u/", zscale.cnumber);
            if(0 == strcmp((gchar*)gwy_container_get_string_by_name(
                           container, g_strconcat(key, "tff", NULL)), 
               TFF_LINEAR1D_NAME)) {
              // TFF_LINEAR1D is used          
              zscale.tfftype = TFF_LINEAR1D;
              zscale.factor_1 = gwy_container_get_double_by_name(container,
                               g_strconcat(key, "Factor", NULL));
              zscale.offset_1 = gwy_container_get_double_by_name(container,
                               g_strconcat(key, "Offset", NULL));
            } else if(0 == strcmp((gchar*)
                        gwy_container_get_string_by_name(container,
                       g_strconcat(key, "tff", NULL)),
                     TFF_MULTILINEAR1D_NAME)) {
              // TFF_MULTILINEAR1D is used
              zscale.tfftype = TFF_MULTILINEAR1D;
              zscale.neutralfactor_2= gwy_container_get_double_by_name(
                       container, g_strconcat(key, "NeutralFactor", NULL));
              zscale.offset_2       = gwy_container_get_double_by_name(
                       container, g_strconcat(key, "Offset", NULL));
              zscale.prefactor_2    = gwy_container_get_double_by_name(
                       container, g_strconcat(key, "PreFactor", NULL));
              zscale.preoffset_2    = gwy_container_get_double_by_name(
                       container, g_strconcat(key, "PreOffset", NULL));
              zscale.raw1_2         = gwy_container_get_double_by_name(
                       container, g_strconcat(key, "Raw_1", NULL));
              zscale.whole_2        = (zscale.raw1_2 - zscale.preoffset_2)/
                       (zscale.neutralfactor_2*zscale.prefactor_2);
            } else {
              // UNKNOWN Transfer Function is used
              // setting factor to 1.0 to obtain unscaled data
              g_warning("omicronmatrix::matrix_loadimagefile:"
                       " unknown transferfunction, scaling will be wrong");
              zscale.tfftype = TFF_LINEAR1D;
              zscale.factor_1 = 1.0;
              zscale.offset_1 = 0.0;
            }
            sunit = (gchar*)gwy_container_get_string_by_name(container,
                         g_strconcat(key, "unit", NULL));
#ifdef OSNAVERSION
            if(0 == strcmp(zscale.channelname, "Df")) {
              fac = -1.0/5.464;
              g_snprintf(inverted, sizeof(inverted), " (x 1/-5.464)"); 
            }
#endif
        } else {
            // parameter file is not available, use the plain values
            zscale.tfftype = TFF_LINEAR1D;
            zscale.factor_1 = 1.0;
            zscale.offset_1 = 0.0;
            sunit = NULL;
            g_snprintf(inverted, sizeof(inverted), " (unscaled)"); 
        }


        gwy_debug(g_strconcat("omicronmatrix::matrix_loadimagefile ",
                              msg, NULL)); 
        if(matrixdata->gridmode == 2 ) {
            // Constraint Point
            // parse data, data is encoded as Integer, 32Bit
            for(cntl = 0; cntl<yres; cntl++) {
                for(cntp = 0; cntp < xres && n < avail; cntp++) {
                  // Trace Up
                  data_tup[ind_tup] = 
                      fac*matrix_tff(gwy_get_gint32_le(fp), &zscale);
                  ind_tup++;
                  n++;
                }
                ind_tup -= 2*xres;
            }
        } else if(matrixdata->gridmode == 1) {
            // Constraint Line
            for(cntl = 0; cntl<yres; cntl++) {
                for(cntp = 0; cntp < xres && n < avail; cntp++) {
                  // Trace Up
                  data_tup[ind_tup] = 
                      fac*matrix_tff(gwy_get_gint32_le(fp), &zscale);
                  ind_tup++;
                  n++;
                }
                for(cntp = 0; cntp < xres && n < avail; cntp++) {
                  // Retrace Up
                  data_retup[ind_retup] = 
                      fac*matrix_tff(gwy_get_gint32_le(fp), &zscale);
                  ind_retup--;
                  n++;
                }
                ind_tup -= 2*xres;
            }
        } else {
            // Constraint None or unknown
            // parse data, data is encoded as Integer, 32Bit
            for(cntl = 0; cntl<yres; cntl++) {
                for(cntp = 0; cntp < xres && n < avail; cntp++) {
                  // Trace Up
                  data_tup[ind_tup] = 
                      fac*matrix_tff(gwy_get_gint32_le(fp), &zscale);
                  ind_tup++;
                  n++;
                }
                for(cntp = 0; cntp < xres && n < avail; cntp++) {
                  // Retrace Up
                  data_retup[ind_retup] =
                      fac*matrix_tff(gwy_get_gint32_le(fp), &zscale);
                  ind_retup--;
                  n++;
                }
                ind_tup -= 2*xres;
            }
            for(cntl = 0; cntl<yres; cntl++) {
                for(cntp = 0; cntp < xres && n < avail; cntp++) {
                  // Trace Down
                  data_tdown[ind_tdown] = 
                      fac*matrix_tff(gwy_get_gint32_le(fp), &zscale);
                  ind_tdown++;
                  n++;
                }
                for(cntp = 0; cntp < xres && n < avail; cntp++) {
                  // Retrace Down
                  data_retdown[ind_retdown] = 
                      fac*matrix_tff(gwy_get_gint32_le(fp), &zscale);
                  ind_retdown--;
                  n++;
                }
                ind_retdown += 2*xres;
            }
        } 
        gwy_debug("scanimage: avail: %u n: %u", avail, n);
        gwy_debug("omicronmatrix::matrix_scanimagefile:"
                  " Data successfully read");

        if(matrixdata->gridmode == 2 ) {
          //Constraint Point, only TraceUp
          unit = gwy_si_unit_new("m");
          gwy_data_field_set_si_unit_xy(dfield_tup, unit);
          g_object_unref(unit);
          unit = gwy_si_unit_new(sunit);
          gwy_data_field_set_si_unit_z(dfield_tup, unit);
          g_object_unref(unit);

          g_snprintf(msg, sizeof(msg), "%u-%u %s TraceUp%s", 
                     matrixdata->session, matrixdata->trace, 
                     matrixdata->channelname, inverted);
          gwy_container_set_object_by_name(container, "/0/data",
                                           dfield_tup);
          g_object_unref(dfield_tup);
          gwy_container_set_string_by_name(container,"/0/data/title",
                                    (guchar*)g_strdup(msg));
          gwy_container_set_object_by_name(container, "/0/meta", meta);
          //g_object_unref(meta);
          gwy_debug("omicronmastrix::matrix_scanimagefile:"
                    " gridmode=2, Data saved to container");

        } else if(matrixdata->gridmode == 1) {
          unit = gwy_si_unit_new("m");
          gwy_data_field_set_si_unit_xy(dfield_tup, unit);
          g_object_unref(unit);
          unit = gwy_si_unit_new(sunit);
          gwy_data_field_set_si_unit_z(dfield_tup, unit);
          g_object_unref(unit);

          unit = gwy_si_unit_new("m");
          gwy_data_field_set_si_unit_xy(dfield_retup, unit);
          g_object_unref(unit);
          unit = gwy_si_unit_new(sunit);
          gwy_data_field_set_si_unit_z(dfield_retup, unit);
          g_object_unref(unit);

          g_snprintf(msg, sizeof(msg), "%u-%u %s TraceUp %s", 
                     matrixdata->session, matrixdata->trace, 
                     matrixdata->channelname, inverted);
          gwy_container_set_object_by_name(container, "/0/data", 
                                           dfield_tup);
          g_object_unref(dfield_tup);
          gwy_container_set_string_by_name(container, "/0/data/title", 
                                    (guchar*)g_strdup(msg));
          gwy_container_set_object_by_name(container, "/0/meta", meta);

          g_snprintf(msg, sizeof(msg), "%u-%u %s RetraceUp %s", 
                     matrixdata->session, matrixdata->trace, 
                     matrixdata->channelname, inverted);
          gwy_container_set_object_by_name(container, "/1/data",
                                           dfield_retup);
          g_object_unref(dfield_retup);
          gwy_container_set_string_by_name(container, "/1/data/title", 
                                    (guchar*)g_strdup(msg));
          gwy_container_set_object_by_name(container, "/1/meta", meta);
          //g_object_unref(meta);

          gwy_debug("omicronmastrix::matrix_scanimagefile:"
                    " gridmode=1, Data saved to container");

        } else {
          // Constraint None or unknown 
          unit = gwy_si_unit_new("m");
          gwy_data_field_set_si_unit_xy(dfield_tup, unit);
          g_object_unref(unit);
          unit = gwy_si_unit_new(sunit);
          gwy_data_field_set_si_unit_z(dfield_tup, unit);
          g_object_unref(unit);

          unit = gwy_si_unit_new("m");
          gwy_data_field_set_si_unit_xy(dfield_retup, unit);
          g_object_unref(unit);
          unit = gwy_si_unit_new(sunit);
          gwy_data_field_set_si_unit_z(dfield_retup, unit);
          g_object_unref(unit);

          unit = gwy_si_unit_new("m");
          gwy_data_field_set_si_unit_xy(dfield_tdown, unit);
          g_object_unref(unit);
          unit = gwy_si_unit_new(sunit);
          gwy_data_field_set_si_unit_z(dfield_tdown, unit);
          g_object_unref(unit);

          unit = gwy_si_unit_new("m");
          gwy_data_field_set_si_unit_xy(dfield_retdown, unit);
          g_object_unref(unit);
          unit = gwy_si_unit_new(sunit);
          gwy_data_field_set_si_unit_z(dfield_retdown, unit);
          g_object_unref(unit);
            
          g_snprintf(msg, sizeof(msg), "%u-%u %s TraceUp %s", 
                     matrixdata->session, matrixdata->trace, 
                     matrixdata->channelname, inverted);
          gwy_container_set_object_by_name(container, "/0/data", 
                                           dfield_tup);
          g_object_unref(dfield_tup);
          gwy_container_set_string_by_name(container, "/0/data/title", 
                                    (guchar*)g_strdup(msg));
          gwy_container_set_object_by_name(container, "/0/meta", meta);

          g_snprintf(msg, sizeof(msg), "%u-%u %s RetraceUp %s", 
                     matrixdata->session, matrixdata->trace, 
                     matrixdata->channelname, inverted);
          gwy_container_set_object_by_name(container, "/1/data", 
                                           dfield_retup);
          g_object_unref(dfield_retup);
          gwy_container_set_string_by_name(container, "/1/data/title", 
                                    (guchar*)g_strdup(msg));
          gwy_container_set_object_by_name(container, "/1/meta", meta);

          g_snprintf(msg, sizeof(msg), "%u-%u %s TraceDown %s", 
                     matrixdata->session, matrixdata->trace, 
                     matrixdata->channelname, inverted);
          gwy_container_set_object_by_name(container, "/2/data",
                                           dfield_tdown);
          g_object_unref(dfield_tdown);
          gwy_container_set_string_by_name(container, "/2/data/title", 
                                    (guchar*)g_strdup(msg));
          gwy_container_set_object_by_name(container, "/2/meta", meta);

          g_snprintf(msg, sizeof(msg), "%u-%u %s RetraceDown %s", 
                     matrixdata->session, matrixdata->trace, 
                     matrixdata->channelname, inverted);
          gwy_container_set_object_by_name(container, "/3/data",
                                           dfield_retdown);
          g_object_unref(dfield_retdown);
          gwy_container_set_string_by_name(container, "/3/data/title", 
                                    (guchar*)g_strdup(msg));
          gwy_container_set_object_by_name(container, "/3/meta", meta);
          //g_object_unref(meta);
          gwy_debug("omicronmastrix::matrix_scanimagefile:"
                    " Data saved to container");
      }
    } else {
        // Block identifier is unknown, perhaps the fileend is reached
        g_warning("omicronmatrix::matrix_scanimagefile:"
                  " Block identifier unknown");
        g_free(ident);
        return 0;
    }
    g_free(ident);
    return 1;
}


/** Load the data file. For correct sizes and scaling the 
 *  corresponding parameter file is needed. 
 */
static GwyContainer* matrix_load(const gchar* filename, 
                                    G_GNUC_UNUSED GwyRunType mode,
                                    GError** error) 
{
    GwyContainer* container = NULL;
    GwyContainer* meta = NULL;
    guchar* imgbuffer = NULL;
    guchar* parbuffer = NULL;
    const guchar* fp = NULL;
    GError* err = NULL;
    gsize imgsize, parsize;
    gboolean useparamfile = TRUE;
    gboolean wrongscaling = FALSE;
    MatrixData matrixdata;
    gchar** fsplit = NULL;
    gchar** ifsplit1 = NULL;
    gchar** ifsplit2 = NULL;
    gchar* lastpart = NULL;
    gchar* paramfilename = NULL;
    const gchar* delimiter = ".";
    gchar newdelimiter = '_';

    // Some default values
    gwy_clear(&matrixdata, 1);
    matrixdata.zoom = 1.0;
    // TODO: correct error-management
    
    /* start with the image file */
    if(!gwy_file_get_contents(filename, &imgbuffer, &imgsize, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if(imgsize >= IMGFILEIDENT_SIZE &&
             memcmp(imgbuffer, IMGFILEIDENT, IMGFILEIDENT_SIZE) != 0) {
        err_FILE_TYPE(error, "Omicron Matrix");
        gwy_file_abandon_contents(imgbuffer, imgsize, NULL);
        return NULL;
    }
    /******* Image file is existing and seems to be valid, ********/
    gwy_debug("Now check parameter file: %s", filename);

    /* now check parameter file to get correct sizes */
    fsplit = g_strsplit(filename, "--", 2);
    if(g_strv_length(fsplit) != 2) {
      // filename has unknown structure
      useparamfile = FALSE;
    } else {
      paramfilename = g_strconcat(*fsplit, "_0001.mtrx", NULL);
      useparamfile = TRUE;
    }
    if(useparamfile && 
           !gwy_file_get_contents(paramfilename, &parbuffer,
                                    &parsize, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        useparamfile = FALSE;
        wrongscaling = TRUE;
        g_warning("omicronmatrix: Cannot open parameter file: %s",
                  paramfilename);
    }
    if(useparamfile && parsize >= PARFILEIDENT_SIZE && 
           memcmp(parbuffer, PARFILEIDENT, PARFILEIDENT_SIZE) != 0) {
        gwy_file_abandon_contents(parbuffer, parsize, NULL);
        useparamfile = FALSE;
        wrongscaling = TRUE;
        g_warning("omicronmatrix: Cannot read parameter file: %s",
                  paramfilename);
    }
    /******** Parameter file is existing and seems to be valid *****/

    gwy_debug("omicronmatrix: parameter file: %s", paramfilename);
    // For all data
    container = gwy_container_new();
    // For metadata
    meta = gwy_container_new();

    if(g_strv_length(fsplit) == 2) {
      /* Parse image filename to obtain numbers and channel 
         default_.....--1_1.Df_mtrx  
            (*fsplit)    (*fsplit+1)    */
      // Convert necessary due to differences in MATRIX V1.0 and V2.1  
      lastpart = g_strdelimit(*(fsplit+1), delimiter, newdelimiter);
      ifsplit1 = g_strsplit(lastpart, "_", 4);
      /* sess_trace_channel_mtrx 
           0    1     2      3    */
      matrixdata.session = (guint32)g_strtod(*ifsplit1, NULL);
      matrixdata.trace   = (guint32)g_strtod(*(ifsplit1+1), NULL);
      matrixdata.channelname = g_strdup(*(ifsplit1+2));
      gwy_debug(g_strconcat("omicronmatrix::matrix_load channel: ", 
              matrixdata.channelname,NULL));
    } else {
      g_warning("omicronmatrix::matrix_load:"
                " cannot parse image filename");
      matrixdata.session = 0;
      matrixdata.trace   = 0;
      matrixdata.channelname = g_strdup("unknown");
      wrongscaling = TRUE;
    }

    gwy_debug("omicronmatrix::matrix_load:"
              " Try loading parameter file, if available.");
    if(useparamfile) {
        // parameter file seems to be valid
        gboolean res;
        fp = parbuffer + FILEIDENT_SIZE;
        gwy_container_set_string_by_name(container, 
                              "/meta/imagefilename", 
                              (guchar*)g_strdup(filename));
        gwy_debug("omicronmatrix::matrix_load Scanning parameterfile");
        while (fp < parbuffer + parsize && 
               0 != matrix_scanparamfile(&fp, container, meta, 
                                         &matrixdata)) 
        { // scan parameterfile
        }
        res = gwy_file_abandon_contents(parbuffer, parsize, NULL);

    } else {
        // parameterfile is invalid, open the images with arb units
        g_warning("omicronmatrix::matrix_load: The lateral sizes "
                  "are incorrect, parameterfile is not available.");
        matrixdata.width = 1;
        matrixdata.height = 1;
        matrixdata.xpoints = 0;
        matrixdata.ypoints = 0;
        matrixdata.zoom = 1;  
        // get xpoints, ypoints via scan_image!
        wrongscaling = TRUE;
        gwy_file_abandon_contents(parbuffer, parsize, NULL);
    }
    if(wrongscaling) {
      // TODO: Popup dialog for user values for width, height, zscaling
    }

    matrixdata.proc_cur_img_no = 0;
    fp = imgbuffer + FILEIDENT_SIZE;

    // scan the imagefile. Store to the container
    gwy_debug("omicronmatrix::matrix_load:",
              " starting the image scan loop..");
    matrix_scanimagefile(&fp, 
                         container,
                         meta, 
                         &matrixdata,
                         useparamfile);

    gwy_debug("omicronmatrix::matrix_load Ending...");
    gwy_file_abandon_contents(imgbuffer, imgsize, NULL);
    g_free(paramfilename);
    g_strfreev(fsplit);
    g_strfreev(ifsplit1);
    g_strfreev(ifsplit2);
    g_free(matrixdata.channelname);
    g_object_unref(meta);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

