/**
 * @(#) $Id$
 * Gwyddion plug-in proxy dump file handling.
 * Written by Yeti <yeti@gwyddion.net>.  Public domain.
 **/
#ifndef __GWYDDION_DUMP_H__
#define __GWYDDION_DUMP_H__

#include <string>
#include <map>

/* Data field, more or less equivalent of GwyDataField GObject;
 * members are public as encapsualtion makes little sense here */
class DataField {
    public:
    int xres;        /* x resolution (in pixels) */
    int yres;        /* y resolution (in pixels) */
    double xreal;    /* x real size (in xyunits) */
    double yreal;    /* y real size (in xyunits) */
    double *data;    /* data itself (in zunits) */
    std::string xyunits;  /* base lateral SI units */
    std::string zunits;   /* base value SI units */

    /* No void constructor, intentionally */
    DataField(unsigned long int xres_, unsigned long int yres_);
    DataField(const DataField &dfield);
    ~DataField();
};

/* The hash tables and a dump file representation, this is a a _very poor_
 * GwyContainer imitation, as C++ lacks generic types so we resort to two
 * separate hash tables (the other possibility is to (a) include/implement
 * (b) become dependent on a RTTI libray):
 * data is a hash table type for data fields (DataField's),
 * meta is a hash table type for string metadata. */
class Dump {
    public:
    std::map<std::string,DataField>    data;    /* data fields */
    std::map<std::string,std::string>  meta;    /* string metadata */

    bool read(const char *filename);   /* read a dump file, returns success */
    bool write(const char *filename);  /* write a dump file, returns success */
};

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
