/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Midas Zhou
midaszhou@yahoo.com
------------------------------------------------------------------*/
#include "egi_common.h"
#include "egi_utils.h"
#include "egi_FTsymbol.h"
#include "avg_mvobj.h"
#include "avg_utils.h"
#include "avg_sound.h"
#include "page_avenger.h"

/* ON/OFF  */
bool disable_avgsound=false;

/* Game level and Score relevant */
static	int game_level=0;

static int credit_PlaneHitStation=-3;
static int credit_PlaneHitFender=-1;
static int credit_GunHitPlane=2;

static int win_score=100;   //20;
static int lose_score=-100; //-3;

static int score=0;


/* path for icon collections */
#ifdef LETS_NOTE
static const char *icon_path="/home/midas-zhou/avenger/icon_collections.png";
static const char *scene_path="/home/midas-zhou/avenger/scene";
#else
static const char *icon_path="/mmc/avenger/icon_collections.png";
static const char *scene_path="/mmc/avenger/scene";
#endif


/* image for PAGE bkground */
static char **scene_paths=NULL;
static	int    scene_total=0;
static	int    scene_num=0;
static	EGI_IMGBUF *scene_img=NULL;

/* Icons collection */
static	EGI_IMGBOX *imboxes=NULL;
static	EGI_IMGBUF *icon_collection=NULL;

/* Plane obj and icons */
/* 	    plane_icon_index = 0 - 23 */
static	AVG_MVOBJ  *planes[15];
static	int	    px,py;

/* Gun obj and icons */
static	int	    GunStation_icon_index=24;
static	AVG_MVOBJ  *gun_station=NULL;

/* Bullet obj and icon */
static	int	    bullet_icon_index=26;
static	AVG_MVOBJ  *bullet[8]={NULL,NULL,NULL,NULL};
static	int	    num_bullet=4; /* Active bullet */
static	int 	    bltx, blty;

/* Wall block and icons */
static	int	    wallblock_icon_index=25;
static	int 	    wblk_width=60;
static	int	    wblk_height=45;


void draw_scene_setting(void)
{
	int i;

	/* Draw wall blocks: just draw at the bottom of the PAGE */
	for(i=0; i<240/60; i++)
	        egi_subimg_writeFB( icon_collection, &gv_fb_dev, wallblock_icon_index, -1,
				    i*wblk_width, 320-wblk_height +10);

        /* For bullets that always in storage */
	for(i=4; i<8; i++)
	        egi_subimg_writeFB( icon_collection, &gv_fb_dev, bullet_icon_index, -1,
				    140+10*i, 320-40);
}


/*----------------------------------------
	     A thread function
-----------------------------------------*/
void *thread_game_avenger(EGI_PAGE *page)
{
	int i,j,k;
	int secTick=0;
	int nget=0;



	printf("Start GAME avenger...\n");
        game_readme();
	sleep(3);
	egi_page_needrefresh(page);
	egi_page_refresh(page);


	/* Load sound effect files to EGI_PCMBUF */
	if(!disable_avgsound)
		avg_load_sound();

        /* Get all image file for GAME scenes bkimg */
        scene_paths=egi_alloc_search_files(scene_path, "png, jpg", &scene_total);
	if(scene_total==0) {
		printf("%s: No game scene image file found!\n",__func__);
		return (void *)-1;
	} else {
		printf("%s: Total %d game scene images found!\n",__func__, scene_total);
	}

	/* ---  (1) Definie all icons  --- */

	/* 1.0 read in icon collection file */
	printf("read in icons...\n");
        icon_collection=egi_imgbuf_readfile(icon_path);
        if(icon_collection==NULL) {
                printf("%s: Fail to read image file '%s'.\n", __func__, icon_path);
		//egi_imgboxes_free(imboxes);
		return (void *)-2;
        }

	/* 1.1 Allocate subimage boxes for icon collection */
	imboxes=egi_imgboxes_alloc(28);

	/* 1.2 PLANE ICONS:  Subimage definition: 4 colums, 2 rows of subimages */
	if(imboxes==NULL) {
		printf("%s: Fail to alloc imgboxes!\n",__func__);
		return (void *)-3;
	}
	for(i=0; i<6; i++) {
		for(j=0; j<4; j++) {
			imboxes[i*4+j]=(EGI_IMGBOX){ j*50, i*40, 50, 40 };
		}
	}

	/* 1.3 GUN ICONS:  Subimage definition  */
	imboxes[GunStation_icon_index]=(EGI_IMGBOX){ 0, 6*40, 80, 80 };

	/* 1.4 WALL block ICONS: Subimage definition */
	imboxes[wallblock_icon_index]=(EGI_IMGBOX){ 110, 320-45, 60, 45 };

	/* 1.5 Bullet ICON: Subimage definition */
	imboxes[bullet_icon_index]=(EGI_IMGBOX){ 80, 6*40, 30, 80 };

	/* 1.6 Set subimage for icon collection */
	icon_collection->subimgs=imboxes; imboxes=NULL; /* Ownership transferred */
	icon_collection->submax=28-1;


	/* ---  (2) Create all objects --- */

	/*  2.1 Create planes*/
	for(i=0; i<15; i++)  {
		planes[i]=avg_create_mvobj( icon_collection, egi_random_max(8)-1,   /* EGI_IMGBUF *icons, icon_index */
       	                              (EGI_POINT){egi_random_max(240+50)-25, -25},	/* EGI_POINT pxy */
		    			    egi_random_max(37)-19,		/* heading, maybe out of sight */
					    avg_random_speed(),			/* int speed */
					    plane_trail	//upward_trail		/* int (*trail_mode)(AVG_MVOBJ *) */
                        	    	 );

		planes[i]->effect_index=20; /* Effect_image index of icons */
		planes[i]->effect_stages=4; /* Effect_images total */
		planes[i]->renew_method=avg_renew_plane;
		planes[i]->hit_effect=avg_effect_exploding;
	}

	/* 2.2 Create a gun station */
	gun_station=avg_create_mvobj( icon_collection, GunStation_icon_index, /* EGI_IMGBUF *icons, icon_index */
					(EGI_POINT){ 120, 320-40 },	   /* pxy, center */
		    			    egi_random_max(120)-60,	  /* heading, maybe out of sight */
					    0,				  /* int speed */
					    turn_trail			  /* int (*trail_mode)(AVG_MVOBJ *) */
                                         );
	gun_station->vang=3;//12;  /* station turning angular velocity */

	/* 2.3 Create a bullet */
	for(i=0; i<num_bullet; i++) {
		bullet[i]=avg_create_mvobj( icon_collection, bullet_icon_index,  	/* EGI_IMGBUF *icons, icon_index */
					    gun_station->pxy,		/* pxy, same as gun station */
	    				    gun_station->heading,	/* heading, same as gun station */
					    -15,				/* int speed */
					    bullet_trail		/* int (*trail_mode)(AVG_MVOBJ *) */
	                                );
		/* set ID */
		bullet[i]->id=i;
		/* renew method */
		bullet[i]->renew_method=avg_renew_bullet;
		/* It has a station */
		bullet[i]->station=gun_station;
		/* assign fixed type pos from gun to bullet */
		bullet[i]->fvpx.num=gun_station->fvpx.num;
		bullet[i]->fvpx.div=gun_station->fvpx.div;
		bullet[i]->fvpy.num=gun_station->fvpy.num;
		bullet[i]->fvpy.div=gun_station->fvpy.div;

	}

	/* Draw other scene setting */
	draw_scene_setting();

	/* ---  (3) Preparation before game  --- */

        /* Check whether lookup table fp16_cos[] and fp16_sin[] is generated */
        if( fp16_sin[30] == 0)
                mat_create_fpTrigonTab();


	/* reset tokens */
	k=0;
	secTick=0;


	/* ------ + ------ + ------ + ----- (4) GAME LOOP ----- + ----- + ----- + ----- */

	while(1) {

		k++;

		/*  --- 1. Update PAGE setting --- */

		/* 1.1 Update credit and time */
//		avenpage_update_creditTxt(score, game_level);

		/* 1.2 Increase/Decrese GAME Level/Stage/Episode ... */
		if( score >= win_score || score <= lose_score ) {
			/* A-0. Game Level Upgrading Info ... */
			if(score>=win_score) {
				 game_level++;
				 printf("Game win...\n");
				 //game_levelUp(true, game_level);
				 game_readme();
			}
			else {
				 game_level--;
				 printf("Game lose...\n");
				 //game_levelUp(false, game_level);
				 game_readme();
			}
			fb_refresh(&gv_fb_dev);
			sleep(3);

			/* dump old image data in FB filo */
//			fb_filo_dump(&gv_fb_dev);
       	  		fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */

			/* A-1. Change scene background image */
			printf("Update scene bk image...\n");
			scene_num++;
			if(scene_num > scene_total-1)
				scene_num=0;
			egi_imgbuf_free(scene_img);
			scene_img=egi_imgbuf_readfile(scene_paths[scene_num]);
			page->ebox->frame_img=scene_img;

			/* A-2. Reset socre */
			score=0;

			/* A-3. Change GAME difficulty level */

			/* A-4. Refresh PAGE setting */
			egi_page_needrefresh(page);
			egi_page_refresh(page);

			/* A-5. Other PAGE setting */
			draw_scene_setting();

		}


          /* <<<<< Flush FB and Turn on FILO  >>>>> */
       	  fb_filo_flush(&gv_fb_dev); /* flush and restore old FB pixel data */
          fb_filo_on(&gv_fb_dev);    /* start collecting old FB pixel data */

		/*  --- 2. Update Game Objectst --- */

		/* 2.1 Refresh gun station FIRST!, as time passes. */
		refresh_mvobj(gun_station);

		/* 2.2 Refresh bullet */
		for(i=0; i<num_bullet; i++)
			refresh_mvobj(bullet[i]);

		/* 2.4 TODO: Crash/Hit_target calculaiton and Score */
		/*  			--- Score Rules ---
		 *	1. If a suicide plane hits the fender wall, the gamer loses 1 point.
		 *	2. If a suicide plane hits the gun station, the gamer loses 3 points.
		 *	3. If the gun hits a plane, the gamer gains 1 point.
		 *	4. If the score is less than -3 points, the game is over.
		 *	5. The gamer shall survive all attacks and get scores above 50 to win the game.
		 */

		for(i=0; i<8; i++) {
			if(planes[i]->is_hit)
				continue;

			px=planes[i]->fvpx.num>>MATH_DIVEXP;
			py=planes[i]->fvpy.num>>MATH_DIVEXP;

			/* 2.4.1 Plane hit gun station */
			if( py > 320-60
			    && px > 240/2-40
			    && px < 240/2+40 	) {
				planes[i]->is_hit=true;
				//printf("Plane py=%d crash on gun station!\n", py);
				score +=credit_PlaneHitStation;
			}
			/* 2.4.2 Plane crash on fender wall */
			else if( py > 320-wblk_height ) {
				planes[i]->is_hit=true;
				//printf("Plane py=%d crash on fender wall!\n",py);
				score += credit_PlaneHitFender;
			}

			/* 2.4.3 Bullet hit plane */
			for( j=0; j<num_bullet; j++ ) {
				bltx=bullet[j]->fvpx.num>>MATH_DIVEXP;
				blty=bullet[j]->fvpy.num>>MATH_DIVEXP;
				//if( abs(py-blty)<15 && abs(px-bltx)<15 ) {
				if( avg_intAbs(py-blty)<15 && avg_intAbs(px-bltx)<15 ) {
						planes[i]->is_hit=true;
						score +=credit_GunHitPlane;
				}
			}
		}

		/* 2.4 Refresh planes, as time passes. */
		for(i=0; i<8; i++)
			refresh_mvobj(planes[i]);

		/* 2.4 Update credit */
		avenpage_update_creditTxt(score, game_level);

#if 0		/*  --- 3. Demo. and Advertise --- */

		if( (secTick&(32-1)) == 0 && (nget !=secTick>>5) ) {
			nget=secTick>>5;
			game_readme(); sleep(3);
		}
#endif

	  fb_refresh(&gv_fb_dev);

          /* <<<<< Turn off FILO  >>>>> */
       	  fb_filo_off(&gv_fb_dev);  /* Stop filo */
	  #ifdef LETS_NOTE
	  //tm_delayms(20);
	  usleep(1000000/75);
	  #else
	  tm_delayms(100);
	  #endif

	}

	/* ---  END: Destroy facility and free resources --- */
	egi_imgbuf_free(icon_collection);
	for(i=0; i<15; i++)
		avg_destroy_mvobj(&planes[i]);

	for(i=0; i<4; i++)
		avg_destroy_mvobj(&bullet[i]);

	avg_destroy_mvobj(&gun_station);

	return (void *)0;
}
