/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "dwt.h"

typedef struct {
        gint ncof;
        gint ioff;
        gint joff;
        gdouble *cc;
        gdouble *cr;
} GwyDWTFilter;


/*private functions prototypes*/
static GwyDataLine* pwt(GwyDWTFilter *wt, GwyDataLine *dline, gint n, gint isign);
    
static GwyDWTFilter *wtset(GwyDataLine *wt_coefs);
    
static gint remove_by_universal_threshold(GwyDataField *dfield, gint ulcol, gint ulrow, gint brcol, gint brrow,
		                        gboolean hard, gdouble threshold);
static gint remove_by_adaptive_threshold(GwyDataField *dfield, gint ulcol, gint ulrow, gint brcol, gint brrow,
		                        gboolean hard, gdouble multiple_threshold, gdouble noise_variance);
static gint remove_by_threshold(GwyDataField *dfield, gint ulcol, gint ulrow, gint brcol, gint brrow,
		                        gboolean hard, gdouble multiple_threshold, gdouble noise_variance);


static gdouble smedian(GwyDataField *dfield, gint ulcol, gint ulrow, gint brcol, gint brrow);
/*public functions*/


/**
* gwy_dwt_set_coefficients:
* @dline: dataline where coefficients should be stored (allocated or NULL)
* @type: wavelet type
*
* Fills resampled or nely allocated GwyDataLine with wavelet coefficients. 
* 
* Returns: resampled or newly allocated GwyDataLine with wavelet coefficients.
 **/
GwyDataLine*
gwy_dwt_set_coefficients(GwyDataLine *dline, GwyDWTType type)
{
    GwyDataLine *ret;
    
    if (!dline) ret = GWY_DATA_LINE(gwy_data_line_new(4, 4, FALSE));
    else ret = dline;

    switch (type){
	case (GWY_DWT_HAAR):
	gwy_data_line_resample(ret, 2, GWY_INTERPOLATION_NONE);
	ret->data[0] = 1;
	ret->data[1] = 1;
	break;

	case (GWY_DWT_DAUB4):
	gwy_data_line_resample(ret, 4, GWY_INTERPOLATION_NONE);
	ret->data[0] = 0.68301270189222;
	ret->data[1] = 1.18301270189222;
	ret->data[2] = 0.31698729810778;
	ret->data[3] = -0.18301270189222;
	break;

	case (GWY_DWT_DAUB6):
	gwy_data_line_resample(ret, 6, GWY_INTERPOLATION_NONE);
	ret->data[0] = 0.47046720778405; 
	ret->data[1] = 1.14111691583131;
	ret->data[2] = 0.65036500052554;
	ret->data[3] = -0.19093441556797;
	ret->data[4] = -0.12083220831036;
	ret->data[5] = 0.04981749973164;
	break;

	case (GWY_DWT_DAUB8):
	gwy_data_line_resample(ret, 8, GWY_INTERPOLATION_NONE);
	ret->data[0] = 0.32580342805130; 
	ret->data[1] = 1.01094571509183;
	ret->data[2] = 0.89220013824676;
	ret->data[3] = -0.03957502623564;
	ret->data[4] = -0.26450716736904;
	ret->data[5] = 0.04361630047418;
	ret->data[6] = 0.04650360107098;
	ret->data[7] = -0.01498698933036;
	break;

	case (GWY_DWT_DAUB12):
	gwy_data_line_resample(ret, 12, GWY_INTERPOLATION_NONE);
	ret->data[0] = 0.111540743350*sqrt(2);
	ret->data[1] = 0.494623890398*sqrt(2);
	ret->data[2] = 0.751133908021*sqrt(2);
	ret->data[3] = 0.315250351709*sqrt(2);
	ret->data[4] = -0.226264693965*sqrt(2);
	ret->data[5] = -0.129766867567*sqrt(2);
	ret->data[6] = 0.097501605587*sqrt(2);
	ret->data[7] = 0.027522865530*sqrt(2);
	ret->data[8] = -0.031582039318*sqrt(2);
	ret->data[9] = 0.000553842201*sqrt(2);
	ret->data[10] = 0.004777257511*sqrt(2);
	ret->data[11] = -0.001077301085*sqrt(2);
	break;

	case (GWY_DWT_DAUB20):
	gwy_data_line_resample(ret, 20, GWY_INTERPOLATION_NONE);
	ret->data[0] = 0.026670057901*sqrt(2); 
	ret->data[1] = 0.188176800078*sqrt(2);
	ret->data[2] = 0.527201188932*sqrt(2);
	ret->data[3] = 0.688459039454*sqrt(2);
	ret->data[4] = 0.281172343661*sqrt(2);
	ret->data[5] = -0.249846424327*sqrt(2);
	ret->data[6] = -0.195946274377*sqrt(2);
	ret->data[7] = 0.127369340336*sqrt(2);
	ret->data[8] = 0.093057364604*sqrt(2); 
	ret->data[9] = -0.071394147166*sqrt(2);
	ret->data[10] = -0.029457536822*sqrt(2);
	ret->data[11] = 0.033212674059*sqrt(2); 
	ret->data[12] = 0.003606553567*sqrt(2);
	ret->data[13] = -0.010733175483*sqrt(2);
	ret->data[14] = 0.001395351747*sqrt(2);
	ret->data[15] = 0.001992405295*sqrt(2);
	ret->data[16] = -0.000685856695*sqrt(2);
	ret->data[17] = -0.000116466855*sqrt(2);
	ret->data[18] = 0.000093588670*sqrt(2);
	ret->data[19] = -0.000013264203*sqrt(2);
	break;

	default:
	g_assert_not_reached();
	break;
    }
				
    return ret;	
}

/**
* gwy_dwt_set_coefficients:
* @dline: dataline where coefficients should be stored (allocated or NULL)
* @type: wavelet type
*
* Fills resampled or nely allocated GwyDataLine with wavelet coefficients. 
* 
* Returns: resampled or newly allocated GwyDataLine with wavelet coefficients.
 **/

GwyDataLine*
gwy_dwt_plot_wavelet(GwyDataLine *dline, GwyDataLine *wt_coefs)
{
    GwyDataLine *hlp = GWY_DATA_LINE(gwy_data_line_new(dline->res, dline->real, FALSE));

    /*TODO fill this*/
        
    g_object_unref(hlp);
    return dline;
}

/**
* gwy_dwt_set_coefficients:
* @dline: dataline where coefficients should be stored (allocated or NULL)
* @type: wavelet type
*
* Fills resampled or nely allocated GwyDataLine with wavelet coefficients. 
* 
* Returns: resampled or newly allocated GwyDataLine with wavelet coefficients.
 **/

GwyDataLine*
gwy_dwt_plot_scaling_function(GwyDataLine *dline, GwyDataLine *wt_coefs)
{
    GwyDataLine *hlp = GWY_DATA_LINE(gwy_data_line_new(dline->res, dline->real, FALSE));

    /*TODO fill this*/
    
    g_object_unref(hlp);
    return dline;
}

/**
* gwy_data_line_dwt:
* @dline: dataline to be transformed
* @wt_coefs: dataline where wavelet transfor coefficients are stored.
* @isign: direction of the transform (1)...direct, (-1)...inverse
* @minsize: size of minimal transform result block
*
* Performs steps of the wavelet decomposition while the smallest
* low pass coefficients block is equal to @minsize. Run with
* @minsize = @dline->res/2 to perform one step of decomposition
* or @minsize = 4 to perform full decomposition (or anything between).
* 
* Returns: transformed GwyDataLine.
 **/
GwyDataLine* 
gwy_data_line_dwt(GwyDataLine *dline, GwyDataLine *wt_coefs, gint isign, gint minsize)
{
    gint nn;
    gint n = dline->res; 
    gint k;
    GwyDataLine *shiftedline;
    GwyDWTFilter *wt;
    if (n < 4) return NULL;

    shiftedline = GWY_DATA_LINE(gwy_data_line_new(dline->res + 1, dline->real, FALSE));
    
    wt = wtset(wt_coefs);

    for (k=0; k<dline->res; k++) shiftedline->data[k+1] = dline->data[k];
    
    if (isign >= 0)
	for (nn=n; nn>=(2*minsize); nn>>=1) pwt(wt, shiftedline, nn, isign);
    else
	for (nn=2*minsize; nn<=n; nn<<=1) pwt(wt, shiftedline, nn, isign);

    for (k=0; k<dline->res; k++) dline->data[k] = shiftedline->data[k+1];
  
    g_object_unref(shiftedline);
    
    return dline;
}


/**
* gwy_data_field_xdwt:
* @dfield: datafield to be transformed
* @wt_coefs: dataline where wavelet transfor coefficients are stored.
* @isign: direction of the transform (1)...direct, (-1)...inverse
* @minsize: size of minimal transform result block
*
* Performs steps of the X-direction image wavelet decomposition while the smallest
* low pass coefficients block is equal to @minsize. Run with
* @minsize = @dfield->xres/2 to perform one step of decomposition
* or @minsize = 4 to perform full decomposition (or anything between).
* 
* Returns: transformed GwyDataField.
 **/
GwyDataField* 
gwy_data_field_xdwt(GwyDataField *dfield, GwyDataLine *wt_coefs, gint isign, gint minsize)
{
    gint k;
    GwyDataLine *rin;

    rin = GWY_DATA_LINE(gwy_data_line_new(dfield->xres, dfield->xreal, FALSE));

    for (k = 0; k < dfield->yres; k++) {
	gwy_data_field_get_row(dfield, rin, k);
	rin = gwy_data_line_dwt(rin, wt_coefs, isign, minsize);
	gwy_data_field_set_row(dfield, rin, k);
    }
    g_object_unref(rin);	 
    return dfield;
}

/**
* gwy_data_field_ydwt:
* @dfield: datafield to be transformed
* @wt_coefs: dataline where wavelet transfor coefficients are stored.
* @isign: direction of the transform (1)...direct, (-1)...inverse
* @minsize: size of minimal transform result block
*
* Performs steps of the Y-direction image wavelet decomposition while the smallest
* low pass coefficients block is equal to @minsize. Run with
* @minsize = @dfield->yres/2 to perform one step of decomposition
* or @minsize = 4 to perform full decomposition (or anything between).
* 
* Returns: transformed GwyDataField.
 **/
GwyDataField* 
gwy_data_field_ydwt(GwyDataField *dfield, GwyDataLine *wt_coefs, gint isign, gint minsize)
{
    gint k;
    GwyDataLine *rin;

    rin = GWY_DATA_LINE(gwy_data_line_new(dfield->yres, dfield->yreal, FALSE));

    for (k = 0; k < dfield->xres; k++) {
	gwy_data_field_get_column(dfield, rin, k);
	rin = gwy_data_line_dwt(rin, wt_coefs, isign, minsize);
	gwy_data_field_set_column(dfield, rin, k);
    }
    g_object_unref(rin);	     
    return dfield;
}

/**
* gwy_data_field_dwt:
* @dfield: datafield to be transformed (square)
* @wt_coefs: dataline where wavelet transfor coefficients are stored.
* @isign: direction of the transform (1)...direct, (-1)...inverse
* @minsize: size of minimal transform result block
*
* Performs steps of the 2D image wavelet decomposition while the smallest
* low pass coefficients block is equal to @minsize. Run with
* @minsize = @dfield->xres/2 to perform one step of decomposition
* or @minsize = 4 to perform full decomposition (or anything between).
* 
* Returns: transformed GwyDataField.
 **/
GwyDataField* 
gwy_data_field_dwt(GwyDataField *dfield, GwyDataLine *wt_coefs, gint isign, gint minsize)
{
    if ((!dfield) || (!wt_coefs)) return NULL;
    GwyDataLine *rin;
    gint nn, k;

    rin = GWY_DATA_LINE(gwy_data_line_new(dfield->xres, dfield->xreal, FALSE));

    if (isign >= 0)
    {
	for (nn = dfield->xres; nn>=(2*minsize); nn>>=1)
        {
	    for (k = 0; k < nn; k++) {
		gwy_data_field_get_row_part(dfield, rin, k, 0, nn);
		rin = gwy_data_line_dwt(rin, wt_coefs, isign, nn/2);
		gwy_data_field_set_row_part(dfield, rin, k, 0, nn);
	    }
	    for (k = 0; k < nn; k++) {
		gwy_data_field_get_column_part(dfield, rin, k, 0, nn);
		rin = gwy_data_line_dwt(rin, wt_coefs, isign, nn/2);
		gwy_data_field_set_column_part(dfield, rin, k, 0, nn);
	    }
	}
    }
    else {
	for (nn = 2*minsize; nn<=dfield->xres; nn<<=1)
	{
	    for (k = 0; k < nn; k++) {
		gwy_data_field_get_row_part(dfield, rin, k, 0, nn);
		rin = gwy_data_line_dwt(rin, wt_coefs, isign, nn/2);
		gwy_data_field_set_row_part(dfield, rin, k, 0, nn);
	    }
	    for (k = 0; k < nn; k++) {
		gwy_data_field_get_column_part(dfield, rin, k, 0, nn);
		rin = gwy_data_line_dwt(rin, wt_coefs, isign, nn/2);
		gwy_data_field_set_column_part(dfield, rin, k, 0, nn);
	    }
	}
    }

    g_object_unref(rin);	 
    return dfield;
}

GwyDataField *gwy_data_field_dwt_denoise(GwyDataField *dfield, GwyDataLine *wt_coefs, gboolean hard,
					                           gdouble multiple_threshold,
								   GwyDWTDenoiseType type)
{
    gint br, ul, ulcol, ulrow, brcol, brrow, count;
    gdouble median, noise_variance, threshold;
    
    gwy_data_field_dwt(dfield, wt_coefs, 1, 4);

    ulcol = dfield->xres/2; ulrow = dfield->xres/2;
    brcol = dfield->xres; brrow = dfield->xres;
	 
    median = smedian(dfield, ulcol, ulrow, brcol, brrow);
    noise_variance = median/0.6745;
   

    if (type == GWY_DWT_DENOISE_UNIVERSAL)
    {
	threshold = noise_variance*sqrt(2*log(dfield->xres*dfield->yres/4))*multiple_threshold;
    }
      
    for (br = dfield->xres; br>4; br>>=1)
    {
	ul = br/2;

	ulcol = ul; ulrow = ul;
	brcol = br; brrow = br;
	switch (type){
	    case(GWY_DWT_DENOISE_SCALE_ADAPTIVE):
	    count = remove_by_threshold(dfield, ulcol, ulrow, brcol, brrow, hard, multiple_threshold, noise_variance);
	    break;

	    case(GWY_DWT_DENOISE_UNIVERSAL):
	    count = remove_by_universal_threshold(dfield, ulcol, ulrow, brcol, brrow, hard, threshold);
	    break;

	    case(GWY_DWT_DENOISE_SPACE_ADAPTIVE):
	    count = remove_by_adaptive_threshold(dfield, ulcol, ulrow, brcol, brrow, hard, multiple_threshold, noise_variance);
	    break;

	    default:
	    g_assert_not_reached();
	    break;
	}
	printf("Level %d, diagonal: %d removed\n", br, count);

	ulcol = 0; ulrow = ul;
	brcol = ul; brrow = br;
	switch (type){
	    case(GWY_DWT_DENOISE_SCALE_ADAPTIVE):
	    count = remove_by_threshold(dfield, ulcol, ulrow, brcol, brrow, hard, multiple_threshold, noise_variance);
	    break;

	    case(GWY_DWT_DENOISE_UNIVERSAL):
	    count = remove_by_universal_threshold(dfield, ulcol, ulrow, brcol, brrow, hard, threshold);
	    break;

	    case(GWY_DWT_DENOISE_SPACE_ADAPTIVE):
	    count = remove_by_adaptive_threshold(dfield, ulcol, ulrow, brcol, brrow, hard, multiple_threshold, noise_variance);
	    break;

	    default:
	    g_assert_not_reached();
	    break;
	}
	printf("Level %d, horizontal: %d removed\n", br, count);

	ulcol = ul; ulrow = 0;
	brcol = br; brrow = ul;
	switch (type){
	    case(GWY_DWT_DENOISE_SCALE_ADAPTIVE):
	    count = remove_by_threshold(dfield, ulcol, ulrow, brcol, brrow, hard, multiple_threshold, noise_variance);
	    break;

	    case(GWY_DWT_DENOISE_UNIVERSAL):
	    count = remove_by_universal_threshold(dfield, ulcol, ulrow, brcol, brrow, hard, threshold);
	    break;

	    case(GWY_DWT_DENOISE_SPACE_ADAPTIVE):
	    count = remove_by_adaptive_threshold(dfield, ulcol, ulrow, brcol, brrow, hard, multiple_threshold, noise_variance);
	    break;

	    default:
	    g_assert_not_reached();
	    break;
	}
	printf("Level %d, vertical: %d removed\n", br, count);
    }
    
    gwy_data_field_dwt(dfield, wt_coefs, -1, 4);    
    return dfield;
}



/*private functions*/

static GwyDataLine*
pwt(GwyDWTFilter *wt, GwyDataLine *dline, gint n, gint isign)
{
	double ai, ai1;
	long i, ii, j, jf, jr, k, n1, ni, nj, nh, nmod;

	if (n < 4) return NULL;
	GwyDataLine *wksp;
	wksp = GWY_DATA_LINE(gwy_data_line_new(n+1, n+1, TRUE));
	
	nmod = wt->ncof*n; 
	n1 = n-1;			 
	nh = n >> 1;
	if (isign >= 0) 
	{
		for (ii = 1, i = 1; i <= n ; i += 2, ii++) 
		{
			ni = i+nmod+wt->ioff;
			nj = i+nmod+wt->joff;
			for (k=1; k<=wt->ncof; k++) 
			{
				jf = n1 & (ni+k); 
				jr = n1 & (nj+k);
				wksp->data[ii] += wt->cc[k] * dline->data[jf+1];
				wksp->data[ii+nh] += wt->cr[k] * dline->data[jr+1];
			}
		}

	} 
	else 
	{ 
		for (ii = 1, i = 1; i <= n; i+= 2, ii++) 
		{
			ai = dline->data[ii];
			ai1 = dline->data[ii+nh];
			ni = i+nmod+wt->ioff;
			nj = i+nmod+wt->joff;
			for (k=1; k<=wt->ncof; k++) 
			{
				jf=(n1 & (ni+k))+1;
				jr=(n1 & (nj+k))+1;
				wksp->data[jf] += wt->cc[k]*ai;
				wksp->data[jr] += wt->cr[k]*ai1;
			}
		}
	}
	
	for (j=1; j<=n; j++) dline->data[j]=wksp->data[j];
	g_object_unref(wksp);

	return dline;
}


static GwyDWTFilter *
wtset(GwyDataLine *wt_coefs)
{
    int i, k;
    float sig = -1.0;
    GwyDWTFilter *wt;

    wt = g_new(GwyDWTFilter, 1);
    wt->cc = g_malloc(sizeof(gdouble)*(wt_coefs->res + 1));
    wt->cr = g_malloc(sizeof(gdouble)*(wt_coefs->res + 1));
    wt->ncof = wt_coefs->res;
    
    for (i=0; i<wt_coefs->res; i++) wt->cc[i+1]=wt_coefs->data[i]/sqrt(2);

    for (k=1; k<=wt_coefs->res; k++)
    {
	wt->cr[wt_coefs->res + 1 - k] = sig*wt->cc[k];
	sig= -sig;
    }

    /*FIXME none of the NRC shifts centers wavelet well*/
    wt->ioff = wt->joff = -(wt_coefs->res >> 1);
/*    wt->ioff = -2;
    wt->joff = -wt->ncof+2;*/
    
    return wt;
}

/*universal thresholding with supplied threshold value*/
static gint
remove_by_universal_threshold(GwyDataField *dfield, gint ulcol, gint ulrow, gint brcol, gint brrow, 
		    gboolean hard, gdouble threshold)
{
    gdouble *datapos;
    gint i, j, count;

    count = 0;
    datapos = dfield->data + ulrow*dfield->xres + ulcol;
    for (i = 0; i < (brrow - ulrow); i++) {
       gdouble *drow = datapos + i*dfield->xres;

       for (j = 0; j < (brcol - ulcol); j++) {
	   if (fabs(*drow) < threshold)
	   {
	       if (hard) *drow = 0;
	       else {
		   if (*drow<0) (*drow)+=threshold;
		   else (*drow)-=threshold;
	       }
	       count++;
	   }
	   drow++;
       }
    }
    return count;
}

/*area adaptive thresholding*/
static gint
remove_by_adaptive_threshold(GwyDataField *dfield, gint ulcol, gint ulrow, gint brcol, gint brrow, 
		    gboolean hard, gdouble multiple_threshold, gdouble noise_variance)
{
    gdouble threshold, rms;
    gdouble *datapos;
    gint i, j, count;
    gint pbrcol, pbrrow, pulcol, pulrow;
    gint size = 12;

    count = 0;
    datapos = dfield->data + ulrow*dfield->xres + ulcol;
    for (i = 0; i < (brrow - ulrow); i++) {
       gdouble *drow = datapos + i*dfield->xres;

       for (j = 0; j < (brcol - ulcol); j++) {

	   pulcol = MAX(ulcol + j - size/2, ulcol);
	   pulrow = MAX(ulrow + i - size/2, ulrow);
	   pbrcol = MIN(ulcol + j + size/2, brcol);
	   pbrrow = MIN(ulrow + i + size/2, brrow);

	   rms = gwy_data_field_area_get_rms(dfield, pulcol, pulrow, pbrcol-pulcol, pbrrow-pulrow);
	   if ((rms*rms - noise_variance*noise_variance) > 0)
	   {
	 	rms = sqrt(rms*rms - noise_variance*noise_variance);
		threshold = noise_variance*noise_variance/rms*multiple_threshold;
	   }
	   else
	   {	
	        threshold = 
		    MAX(gwy_data_field_area_get_max(dfield, pulcol, pulrow, pbrcol-pulcol, pbrrow-pulrow), 
			-gwy_data_field_area_get_min(dfield, pulcol, pulrow, pbrcol-pulcol, pbrrow-pulrow));
	   }	   
	   
	   if (fabs(*drow) < threshold)
	   {	       	       
	       if (hard) *drow = 0;
	       else {
		   if (*drow<0) (*drow)+=threshold;
		   else (*drow)-=threshold;
	       }
	       count++;
	   }
	   drow++;
       }
    }
    return count;
}

/*scale adaptive thresholding*/
static gint
remove_by_threshold(GwyDataField *dfield, gint ulcol, gint ulrow, gint brcol, gint brrow, 
		    gboolean hard, gdouble multiple_threshold, gdouble noise_variance)
{
    gdouble rms, threshold;
    gdouble *datapos;
    gint i, j, n, count;

    n = (brrow-ulrow)*(brcol-ulcol);
    
    rms = gwy_data_field_area_get_rms(dfield, ulcol, ulrow, brcol-ulcol, brrow-ulrow);
    if ((rms*rms - noise_variance*noise_variance) > 0)
    {
	rms = sqrt(rms*rms - noise_variance*noise_variance);
	threshold = noise_variance*noise_variance/rms*multiple_threshold;
    }
    else
    {	
        threshold = 
	    MAX(gwy_data_field_area_get_max(dfield, ulcol, ulrow, brcol-ulcol, brrow-ulrow), 
		-gwy_data_field_area_get_min(dfield, ulcol, ulrow, brcol-ulcol, brrow-ulrow));
    }

    count = 0;
    datapos = dfield->data + ulrow*dfield->xres + ulcol;
    for (i = 0; i < (brrow - ulrow); i++) {
       gdouble *drow = datapos + i*dfield->xres;

       for (j = 0; j < (brcol - ulcol); j++) {
	   if (fabs(*drow) < threshold)
	   {
	       if (hard) *drow = 0;
	       else {
		   if (*drow<0) (*drow)+=threshold;
		   else (*drow)-=threshold;
	       }
	       count++;
	   }
	   drow++;
       }
    }
    return count;
}


gint dsort(const void *p_a, const void *p_b)
{
   gdouble *a=(gdouble *)p_a;
   gdouble *b=(gdouble *)p_b;
   if (*a < *b) return -1;
   else if (*a == *b) return 0;
   else return 1;
}

static gdouble smedian(GwyDataField *dfield, gint ulcol, gint ulrow, gint brcol, gint brrow)
{
   gint i, j, n, k;
   gdouble *datapos, val;
   n = (brrow-ulrow)*(brcol-ulcol);
   gdouble *buf= g_malloc( sizeof(double)*n);
   

   k = 0;
   datapos = dfield->data + ulrow*dfield->xres + ulcol;
   for (i = 0; i < (brrow - ulrow); i++) {
       gdouble *drow = datapos + i*dfield->xres;

       for (j = 0; j < (brcol - ulcol); j++) {
	   buf[k] = fabs(*drow);
	   k++;
	   drow++;
       }
   }
  
   qsort((void *)buf, n, sizeof(gdouble), dsort);

   val = (buf[(gint)(n/2)-1]+buf[(gint)(n/2)])/2;
   g_free(buf);
   return val;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
