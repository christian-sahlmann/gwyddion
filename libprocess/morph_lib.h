
#ifndef TIP_MORPH
#define TIP_MORPH

gdouble **allocmatrix(gint ysiz, gint xsiz);

void freematrix(gdouble **mptr, gint ysiz);

gdouble **ireflect(gdouble **surface, gint surf_xsiz, gint surf_ysiz);

gdouble **idilation(gdouble **surface, gint surf_xsiz, gint surf_ysiz,
	      gdouble **tip, gint tip_xsiz, gint tip_ysiz, gint xc, gint yc);

gdouble **ierosion(gdouble **image, gint im_xsiz, gint im_ysiz,
	      gdouble **tip, gint tip_xsiz, gint tip_ysiz,
	      gint xc, gint yc);

gdouble **icmap(gdouble **image, gint im_xsiz, gint im_ysiz,
	      gdouble **tip, gint tip_xsiz,gint tip_ysiz,
              gdouble **rsurf,
	      gint xc, gint yc);

gdouble **iopen(gdouble **image, gint im_xsiz, gint im_ysiz, gdouble **tip, gint tip_xsiz, gint tip_ysiz);


void itip_estimate(gdouble **image, gint im_xsiz, gint im_ysiz,
		   gint tip_xsiz, gint tip_ysiz, gint xc, gint yc, gdouble **tip0,
		   gdouble thresh);

gint itip_estimate_iter(gdouble **image, gint im_xsiz, gint im_ysiz, gint tip_xsiz, gint tip_ysiz,
   gint xc, gint yc, gdouble **tip0, gdouble thresh);

void itip_estimate0(gdouble **image, gint im_xsiz, gint im_ysiz, gint tip_xsiz, gint tip_ysiz,
   gint xc, gint yc, gdouble **tip0, gdouble thresh);

gint useit(gint x, gint y, gdouble **image, gint sx, gint sy, gint delta);

gint itip_estimate_point(gint ixp, gint jxp, gdouble **image, 
			 gint im_xsiz, gint im_ysiz, gint tip_xsiz, gint tip_ysiz,
			 gint xc, gint yc, gdouble **tip0, gdouble thresh);


#endif
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

