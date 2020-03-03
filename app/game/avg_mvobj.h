/*-------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Midas Zhou
-------------------------------------------------------------------*/
#ifndef __AVG_MVOBJ_H__
#define __AVG_MVOBJ_H__

#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "page_avenger.h"


typedef struct avg_mvobj_data AVG_MVOBJ; /* movable object */
struct avg_mvobj_data {
	unsigned int		id;		/* Identity number */
	unsigned long int   refcnt;		/* Refresh count */

	/* Icons and images */
  const	EGI_IMGBUF	*icons;	 		/* Icons collection */
	int		icon_index;	 	/* Index of the icons for the mvobj image */
	EGI_IMGBUF	*refimg;		/* Loaded image as for refrence */
	EGI_IMGBUF	*actimg;		/* Image for displaying */

	/* Reference station */
	AVG_MVOBJ	*station;		/* A station, as a ref. obj to the mvobj */

	/* Position, center of the mvobj */
	EGI_POINT	pxy;	 		/* Current position in integer, image center hopefully.*/
	float		fpx;			/* Coord. in float, to improve calculation precision  */
	float		fpy;
	EGI_FVAL	fvpx;			/* Fixed point value for pxy.x */
	EGI_FVAL	fvpy;

	/* Heading & Speed */
	int		heading;		/* Current heading, in Degree
						 * screenY as heading 0 Degree line.
					         * Counter_Clockwise as positive direction.
						 * Constrain to [0, 360]
						 */
	int		speed;   		/* Current speed, in pixles per refresh */
	float		fspeed;
	int		vang;			/* angular velocity, degree per refresh */
	float		fvang;

	/* Trail mode */
	int (*trail_mode)(AVG_MVOBJ *);  	/* Method to refresh trail
						 * Jobs:
						 *	1. Update params(positon/heading/vang...) of the mvobj.
						 *	2. Check if its out of visible region. then renew the
						 *	   mvobj or destroy it.
						 *	3. Update actimg and/or refimg.
						 */

	/* Renew mode */
	int (*renew_method)(AVG_MVOBJ *);	/* Method to renew a mvobj
						 * 	1. Reset necessary parameters.
						 *	2. Reset refimg and/or actimg.
						 */

	/* For hit effect */
	int (*hit_effect)(AVG_MVOBJ *);  	/* Method to display effect for an hit/damaged mvobj */
	bool		is_hit;			/* Whether it's hit */
	int		effect_index;		/* Effect image index of icons, starting index of a serial subimages */
	int 		effect_stages;		/* Total number of special effect subimages in icons,
						 * after it's hit/damaged.
						 */
	int		stage;			/* current exploding image index, from 0 to effect_stages-1 */
};

/*  FUNCTIONS  */
AVG_MVOBJ* avg_create_mvobj(    EGI_IMGBUF *icons,  int icon_index,
				EGI_POINT pxy, int heading, int speed,
				int (*trail_mode)(AVG_MVOBJ *)
			    );

void 	avg_destroy_mvobj(AVG_MVOBJ **mvobj);

int 	avg_effect_exploding(AVG_MVOBJ *mvobj);

int 	upward_trail(AVG_MVOBJ *mvobj);
int 	plane_trail(AVG_MVOBJ *plane);
int 	bullet_trail(AVG_MVOBJ *mvobj);
int 	turn_trail(AVG_MVOBJ *mvobj);

int 	avg_renew_plane(AVG_MVOBJ *plane);
int 	avg_renew_bullet(AVG_MVOBJ *bullet);

int 	refresh_mvobj(AVG_MVOBJ *mvobj);

int avg_random_speed(void);

#endif
