
#ifndef TIP_MORPH
#define TIP_MORPH

long **allocmatrix(long ysiz, long xsiz);

void freematrix(long **mptr, long ysiz);

long **ireflect(long **surface, long surf_xsiz, long surf_ysiz);

long **idilation(long **surface, long surf_xsiz, long surf_ysiz,
	      long **tip, long tip_xsiz, long tip_ysiz, long xc, long yc);

long **ierosion(long **image, long im_xsiz, long im_ysiz,
	      long **tip, long tip_xsiz, long tip_ysiz,
	      long xc, long yc);

long **icmap(long **image, long im_xsiz, long im_ysiz,
	      long **tip, long tip_xsiz,long tip_ysiz,
              long **rsurf,
	      long xc, long yc);

long **iopen(long **image, long im_xsiz, long im_ysiz, long **tip, long tip_xsiz, long tip_ysiz);


void itip_estimate(long **image, long im_xsiz, long im_ysiz,
		   long tip_xsiz, long tip_ysiz, long xc, long yc, long **tip0,
		   long thresh);

long itip_estimate_iter(long **image, long im_xsiz, long im_ysiz, long tip_xsiz, long tip_ysiz,
   long xc, long yc, long **tip0, long thresh);

void itip_estimate0(long **image, long im_xsiz, long im_ysiz, long tip_xsiz, long tip_ysiz,
   long xc, long yc, long **tip0, long thresh);

long useit(long x, long y, long **image, long sx, long sy, long delta);

long itip_estimate_point(long ixp, long jxp, long **image, 
			 long im_xsiz, long im_ysiz, long tip_xsiz, long tip_ysiz,
			 long xc, long yc, long **tip0, long thresh);


#endif
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

