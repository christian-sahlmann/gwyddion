
#ifndef TIP_MORPH
#define TIP_MORPH


/*simple routines - integer arithmetics*/
gint **iallocmatrix(gint ysiz, gint xsiz);

void ifreematrix(gint **mptr, gint ysiz);

gint **ireflect(gint **surface, gint surf_xsiz, gint surf_ysiz);

gint **idilation(gint **surface, gint surf_xsiz, gint surf_ysiz,
	      gint **tip, gint tip_xsiz, gint tip_ysiz, gint xc, gint yc);

gint **ierosion(gint **image, gint im_xsiz, gint im_ysiz,
	      gint **tip, gint tip_xsiz, gint tip_ysiz,
	      gint xc, gint yc);

gint **icmap(gint **image, gint im_xsiz, gint im_ysiz,
	      gint **tip, gint tip_xsiz, gint tip_ysiz,
              gint **rsurf,
	      gint xc, gint yc);

/*simple routines - double arithmetics (can be very slow)*/
gdouble **dallocmatrix(gint ysiz, gint xsiz);

void dfreematrix(gdouble **mptr, gint ysiz);

gdouble **dreflect(gdouble **surface, gint surf_xsiz, gint surf_ysiz);

gdouble **ddilation(gdouble **surface, gint surf_xsiz, gint surf_ysiz,
	      gdouble **tip, gint tip_xsiz, gint tip_ysiz, gint xc, gint yc);

gdouble **derosion(gdouble **image, gint im_xsiz, gint im_ysiz,
	      gdouble **tip, gint tip_xsiz, gint tip_ysiz,
	      gint xc, gint yc);

/*tip estimation routines - all in integer artithmetics*/
gint **iopen(gint **image, gint im_xsiz, gint im_ysiz, gint **tip, gint tip_xsiz, gint tip_ysiz);

void itip_estimate(gint **image, gint im_xsiz, gint im_ysiz,
		   gint tip_xsiz, gint tip_ysiz, gint xc, gint yc, gint **tip0,
		   gint thresh, gboolean use_edges);

gint itip_estimate_iter(gint **image, gint im_xsiz, gint im_ysiz, gint tip_xsiz, gint tip_ysiz,
   gint xc, gint yc, gint **tip0, gint thresh, gboolean use_edges);

void itip_estimate0(gint **image, gint im_xsiz, gint im_ysiz, gint tip_xsiz, gint tip_ysiz,
   gint xc, gint yc, gint **tip0, gint thresh, gboolean use_edges);

gint useit(gint x, gint y, gint **image, gint sx, gint sy, gint delta);

gint itip_estimate_point(gint ixp, gint jxp, gint **image, 
			 gint im_xsiz, gint im_ysiz, gint tip_xsiz, gint tip_ysiz,
			 gint xc, gint yc, gint **tip0, gint thresh,
             gboolean use_edges);


#endif
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

