#ifndef __SPML_UTILS_H
#define __SPML_UTILS_H
/*
 * =====================================================================================
 *
 *        Filename:  spml-utils.c
 *
 *     Description:  ZLIB stream inflation, Base64 decoding, data types reading
 *                   functions
 *
 *         Version:  0.1
 *         Created:  20.02.2006 20:16:13 CET
 *        Revision:  none
 *        Compiler:  gcc
 *
 *          Author:  Jan Horak (xhorak@gmail.com),
 *         Company:
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <assert.h>


#define CHUNK 16000             /*/< size of input/output buffer for zlib inflate in bytes */
/**
 * Possible input data formats in SPML
 */
typedef enum {
    UNKNOWN_DATAFORMAT, FLOAT32, FLOAT64, INT8, INT16, INT32, UINT8, UINT16,
    UINT32, STRING
} dataFormat;

/**
 * Possible data coding in SPML
 */
typedef enum {
    UNKNOWN_CODING, ZLIB_COMPR_BASE64, BASE64, HEX, ASCII, BINARY
} codingTypes;

/**
 * Possible byte orders in SPML
 */
typedef enum {
    UNKNOWN_BYTEORDER, X_LITTLE_ENDIAN, X_BIG_ENDIAN
} byteOrder;

/**
 * Structure contain information about one datachannel group.
 * Each datachannel group has unique name and it contain
 * list of relevant datachannels.
 */
typedef struct {
    xmlChar *name;
    GList *datachannels;
} dataChannelGroup;

/**
 Structure to hold info about zlib stream inflating
 @param strm holds info for inflate function
 @param in input buffer
 @param out output buffer
 Each zlib_stream must be initialized before used byt inflate_init
 and destroyed by inflate_destroy
*/
typedef struct {
    z_stream strm;
    char in[CHUNK];
    char out[CHUNK];
} zlib_stream;


/* ZLIB INFLATING FUNCTIONS */
/* ------------------------ */

/**
 Initialization of zlib inflating
 @return Z_OK when initialization succeeded
*/
static int
inflate_init(zlib_stream * zstr)
{
    int ret;

    zstr->strm.zalloc = Z_NULL;
    zstr->strm.zfree = Z_NULL;
    zstr->strm.opaque = Z_NULL;
    zstr->strm.avail_in = 0;
    zstr->strm.next_in = Z_NULL;

    ret = inflateInit(&(zstr->strm));
    return ret;
}

/**
 Set input buffer to decompress
 @param in_buf pointer to input buffer
 @param count number of bytes in input buffer
 @return -1 when input buffer is too long to fit into in buffer
 (is greater than CHUNK define )
*/
static int
inflate_set_in_buffer(zlib_stream * zstr, char *in_buf, int count)
{
    if (count > CHUNK) {
        g_warning("Input buffer is too long (%d). Maximum size is %d.\n", count,
                  CHUNK);
        return -1;
    }
    if (count == 0)
        return 0;
    zstr->strm.avail_in = count;

    memcpy(&(zstr->in), in_buf, count);
    zstr->strm.next_in = zstr->in;
    /*zstr->strm.next_in = in_buf;  better to copy input buffer */

    return 0;
}

/**
 Run inflation of buffer
 Run inflation of input buffer which was previously set by inflate_set_in_buffer
 and store it to output buffer.
 */
static int
inflate_get_out_buffer(zlib_stream * zstr, GArray ** out_buf)
{
    int count;
    int ret;

    /* run inflate() on input until output buffer not full */
    do {
        zstr->strm.avail_out = CHUNK;
        zstr->strm.next_out = zstr->out;
        ret = inflate(&(zstr->strm), Z_NO_FLUSH);
        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
        switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                return ret;
        }
        count = CHUNK - zstr->strm.avail_out;
        *out_buf = g_array_append_vals(*out_buf, zstr->out, count);
        if (*out_buf == NULL) {
            g_warning
                ("Zlib inflate: output buffer wasn't written to dynamic array.\n");
            return Z_ERRNO;
        }
    } while (zstr->strm.avail_out == 0);

    return ret;
}

/**
 Dispose zlib inflate structure
*/
static void
inflate_destroy(zlib_stream * zstr)
{
    (void)inflateEnd(&(zstr->strm));
}

/**
 * Inflate content of in_buf to out_buf
 * @param in_buf pointer to GArray of chars which contain whole zlib stream to inflate
 * @param out_buf pointer to pointer to GArray of chars which will be set to inflated
 * stream, it is dynamically allocated so it must be freed by caller.
 * @return -1 when in_buf is not complete zlib compressed array or any other error
 * when unpacking
 */
static int
inflate_dynamic_array(GArray * in_buf, GArray ** out_buf)
{
    int i;
    int ret = 0;
    zlib_stream zstr;
    char *pCh = in_buf->data;

    inflate_init(&zstr);
    *out_buf = g_array_new(FALSE, FALSE, sizeof(char));
    for (i = 0; (i + CHUNK) < in_buf->len; i += CHUNK) {
        if (inflate_set_in_buffer(&zstr, pCh + i, CHUNK) != 0) {
            ret = -1;
            break;
        }
        if (inflate_get_out_buffer(&zstr, out_buf) != Z_OK) {
            g_warning
                ("Cannot inflate zlib compression. Be sure it is a compressed stream.\n");
            ret = -1;
            break;
        }
    }
    if (ret == 0) {
        /* inflate the rest of buffer */
        if (inflate_set_in_buffer(&zstr, pCh + i, in_buf->len - i) != 0) {
            ret = -1;
        }
        if (inflate_get_out_buffer(&zstr, out_buf) != Z_STREAM_END) {
            g_warning
                ("Cannot inflate zlib compression. Be sure it is a compressed stream.\n");
            ret = -1;
        }
    }
    inflate_destroy(&zstr);
    return ret;

}

/* DATATYPES READING FUNCTIONS */
/* --------------------------- */

/**
 * Read and return float value converted to double from input and move pointer
 * to input by its size in bytes (4).
 * Input data byteorder is respected.
 */
static double
get_float32(char **p, byteOrder order)
{
    union {
        char pp[4];
        float f;
    } z;

    if (order == X_LITTLE_ENDIAN) {
        memcpy(z.pp, *p, sizeof(float));
    }
    else if (order == X_BIG_ENDIAN) {
        z.pp[0] = (*p)[3];
        z.pp[1] = (*p)[2];
        z.pp[2] = (*p)[1];
        z.pp[3] = (*p)[0];
    }
    else {
        g_warning("SPML: get_float32(): unknown byte order.");
        return 0;
    }
    *p += sizeof(float);

    return z.f;
}

/**
 * Read and return double value from input and move pointer
 * to input by its size in bytes (8).
 * Input data byteorder is respected.
 */
static double
get_float64(char **p, byteOrder order)
{
    union {
        char pp[8];
        float f;
    } z;

    if (order == X_LITTLE_ENDIAN) {
        memcpy(z.pp, *p, sizeof(double));
    }
    else if (order == X_BIG_ENDIAN) {
        z.pp[0] = (*p)[7];
        z.pp[1] = (*p)[6];
        z.pp[2] = (*p)[5];
        z.pp[3] = (*p)[4];
        z.pp[4] = (*p)[3];
        z.pp[5] = (*p)[2];
        z.pp[6] = (*p)[1];
        z.pp[7] = (*p)[0];
    }
    else {
        g_warning("SPML: get_float64(): unknown byte order.");
        return 0;
    }
    *p += sizeof(double);

    return z.f;
}

/**
 * Read and return 32 bit int value converted to double from input and move
 * pointer to input by its size in bytes (4).
 * Input data byteorder is respected.
 */
static double
get_int32(char **p, byteOrder order)
{
    union {
        char pp[4];
        gint32 f;
    } z;

    if (order == X_LITTLE_ENDIAN) {
        memcpy(z.pp, *p, sizeof(gint32));
    }
    else if (order == X_BIG_ENDIAN) {
        z.pp[0] = (*p)[3];
        z.pp[1] = (*p)[2];
        z.pp[2] = (*p)[1];
        z.pp[3] = (*p)[0];
    }
    else {
        g_warning("SPML: get_int32(): unknown byte order.");
        return 0;
    }
    *p += sizeof(gint32);

    return (double)z.f;
}

/**
 * Read and return 32 ubit int value converted to double from input and move
 * pointer to input by its size in bytes (4).
 * Input data byteorder is respected.
 */
static double
get_uint32(char **p, byteOrder order)
{
    union {
        char pp[4];
        guint32 f;
    } z;

    if (order == X_LITTLE_ENDIAN) {
        memcpy(z.pp, *p, sizeof(guint32));
    }
    else if (order == X_BIG_ENDIAN) {
        z.pp[0] = (*p)[3];
        z.pp[1] = (*p)[2];
        z.pp[2] = (*p)[1];
        z.pp[3] = (*p)[0];
    }
    else {
        g_warning("SPML: get_uint32(): unknown byte order.");
        return 0;
    }
    *p += sizeof(guint32);

    return (double)z.f;
}

/**
 * Read and return 16 bit int value converted to double from input and move
 * pointer to input by its size in bytes (2).
 * Input data byteorder is respected.
 */
static double
get_int16(char **p, byteOrder order)
{
    union {
        char pp[2];
        gint16 f;
    } z;

    if (order == X_LITTLE_ENDIAN) {
        memcpy(z.pp, *p, sizeof(gint16));
    }
    else if (order == X_BIG_ENDIAN) {
        z.pp[0] = (*p)[1];
        z.pp[1] = (*p)[0];
    }
    else {
        g_warning("SPML: get_int16(): unknown byte order.");
        return 0;
    }
    *p += sizeof(gint16);

    return (double)z.f;
}

/**
 * Read and return 16 bit uint value converted to double from input and move
 * pointer to input by its size in bytes (2).
 * Input data byteorder is respected.
 */
static double
get_uint16(char **p, byteOrder order)
{
    union {
        char pp[2];
        guint16 f;
    } z;

    if (order == X_LITTLE_ENDIAN) {
        memcpy(z.pp, *p, sizeof(guint16));
    }
    else if (order == X_BIG_ENDIAN) {
        z.pp[0] = (*p)[1];
        z.pp[1] = (*p)[0];
    }
    else {
        g_warning("SPML: get_uint16(): unknown byte order.");
        return 0;
    }
    *p += sizeof(guint16);

    return (double)z.f;
}

/**
 * Read and return 8 bit int value converted to double from input and move
 * pointer to input by its size in bytes (1).
 */
static double
get_int8(char **p)
{
    gint8 val = **p;

    *p += sizeof(gint8);
    return (double)val;
}

/**
 * Read and return 8 bit int value converted to double from input and move
 * pointer to input by its size in bytes (1).
 */
static double
get_uint8(char **p)
{
    guint8 val = **p;

    *p += sizeof(guint8);
    return (double)val;
}

/* BASE64 DECODING FUNCTIONS */
/* ------------------------- */

/**
 * Convert symbol coded in BASE64 encoding to number which it represent
 * @param ch character in A-Za-Z0-9+/=
 * @return value of @param ch character
 * Symbol '=' which does not belong to BASE64 is interpreted as '=' to
 * get number of valid bytes for decodeblock function
 */
static char
convert_b64_symbol_to_number(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
    }
    else if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 26;
    }
    else if (ch >= '0' && ch <= '9') {
        return ch - '0' + 52;
    }
    else if (ch == '+') {
        return 62;
    }
    else if (ch == '/') {
        return 63;
    }
    else if (ch == '=') {
        g_warning("Let's hope symbol '%c' is ignored in BASE64 coding.", ch);
        return ch;
    }
    else {
        g_warning(" Cannot translate symbol '%c' to number in BASE64 encoding.",
                  ch);
        return 0;
    }

}

/**
 * Decode four 6-bit characters into three 8-bit bytes
 * @return number of valid output bytes
 * */
static int
decodeblock(unsigned char in[4], unsigned char out[3])
{
    unsigned char i[4];

    if (in[0] == '=' || in[1] == '=')
        return 0;               /* first or second byte can't be '=' */
    i[0] = convert_b64_symbol_to_number(in[0]);
    i[1] = convert_b64_symbol_to_number(in[1]);
    i[2] = convert_b64_symbol_to_number(in[2]);
    i[3] = convert_b64_symbol_to_number(in[3]);
    out[0] = (unsigned char)(i[0] << 2 | i[1] >> 4);
    out[1] = (unsigned char)(i[1] << 4 | i[2] >> 2);
    out[2] = (unsigned char)(((i[2] << 6) & 0xc0) | i[3]);
    return (in[2] == '=') ? 1 : (in[3] == '=') ? 2 : 3;
}


/**
 * Decode input buffer in BASE64 encoding to out_buf
 * @param in_buf pointer to input buffer
 * @param len length of input buffer
 * out_buf is created in dynamic memory must be deallocated by caller.
 * @return -1 when cannot append to dynamic array out_buf
 * @return 0 when input buffer was decoded correctly.
 *
 */
static int
decode_b64(char *in_buf, GArray ** out_buf, int len)
{
    int i;
    char b64_enc_chars[4];
    char b64_dec_chars[3];
    int pos = 0;
    int valid_bytes;

    *out_buf = g_array_new(FALSE, FALSE, sizeof(char));
    for (i = 0; i < len; i++) {
        if (in_buf[i] == '\n' || in_buf[i] == '\r'
            || in_buf[i] == ' ' || in_buf[i] == '\t') {
            /* skip all whitespaces (space, tab, line feed, carriage return) */
            continue;
        }

        b64_enc_chars[pos % 4] = in_buf[i];
        pos++;
        if ((pos % 4) == 0) {
            /* decode and save 4x6b to 3x8b */
            valid_bytes = decodeblock(b64_enc_chars, b64_dec_chars);
            if (valid_bytes > 0) {
                *out_buf =
                    g_array_append_vals(*out_buf, &b64_dec_chars, valid_bytes);
                if (*out_buf == NULL) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

/* SPML utils */
/*------------ */

static dataFormat
get_data_format(char *value)
{
    dataFormat ret;

    if (!value) {
        g_warning("SPML: Unknown dataformat for datachannel.");
        return UNKNOWN_DATAFORMAT;
    }
    else {
        if (strcmp(value, "FLOAT32") == 0) {
            ret = FLOAT32;
        }
        else if (strcmp(value, "FLOAT64") == 0) {
            ret = FLOAT64;
        }
        else if (strcmp(value, "INT8") == 0) {
            ret = INT8;
        }
        else if (strcmp(value, "INT16") == 0) {
            ret = INT16;
        }
        else if (strcmp(value, "INT32") == 0) {
            ret = INT32;
        }
        else if (strcmp(value, "UINT8") == 0) {
            ret = UINT8;
        }
        else if (strcmp(value, "UINT16") == 0) {
            ret = UINT16;
        }
        else if (strcmp(value, "UINT32") == 0) {
            ret = UINT32;
        }
        else if (strcmp(value, "STRING") == 0) {
            ret = STRING;
        }
        else {
            g_warning("SPML: Dataformat for datachannel not recognized.");
            ret = UNKNOWN_DATAFORMAT;
        }
        gwy_debug("Dataformat read.");
        g_free(value);
        return ret;
    }
}

static codingTypes
get_data_coding(char *value)
{
    codingTypes ret;

    if (!value) {
        g_warning("SPML: Unknown coding type for datachannel.");
        return UNKNOWN_CODING;
    }
    else {
        if (strcmp(value, "ZLIB-COMPR-BASE64") == 0) {
            ret = ZLIB_COMPR_BASE64;
        }
        else if (strcmp(value, "BASE64") == 0) {
            ret = BASE64;
        }
        else if (strcmp(value, "HEX") == 0) {
            ret = HEX;
        }
        else if (strcmp(value, "ASCII") == 0) {
            ret = ASCII;
        }
        else if (strcmp(value, "BINARY") == 0) {
            ret = BINARY;
        }
        else {
            g_warning("SPML: Data coding for datachannel not recognized.");
            ret = UNKNOWN_CODING;
        }
        gwy_debug("coding read.");
        g_free(value);
        return ret;
    }
}

static byteOrder
get_byteorder(char *value)
{
    byteOrder ret;

    if (value) {
        if (strcmp(value, "BIG-ENDIAN") == 0) {
            ret = X_BIG_ENDIAN;
        }
        else if (strcmp(value, "LITTLE-ENDIAN") == 0) {
            ret = X_LITTLE_ENDIAN;
        }
        else {
            g_warning("SPML: Byte order for datachannel not recognized.");
            ret = UNKNOWN_BYTEORDER;
        }
        gwy_debug("byteorder read.");
        g_free(value);
        return ret;
    }
    else {
        g_warning("SPML: Unknown byteorder of datachannel.");
        return UNKNOWN_BYTEORDER;
    }
}

#endif
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
