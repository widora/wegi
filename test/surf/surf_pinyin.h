/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This file should be include in test_surfman.c


TODO:
1. Make thread_pinyin and surfuser_pinyin members of EGI_SURFMAN.

Journal:
2022-05-09: Create the file.
2022-05-10:
	1. Add load_pinyin_data(), free_pinyin_data().
2022-05-11:
	1. Add my_refresh_surface().
2022-05-12:
	1. Add init_pinyin().
	2. Add parse_pinyin_input().
2022-05-13:
	1. parse_pinyin_input(): Parse escape sequences for PINYIN.


Midas Zhou
知之者不如好之者好之者不如乐之者
------------------------------------------------------------------*/
#include "egi_unihan.h"
#include <ctype.h>

/* SURFACE Functions */
void* surf_pinyin(void *argv);
void my_refresh_surface(EGI_SURFUSER *surfuser);

/* PINYIN processing functions */
void init_pinyin(void);		/* Call in surf_pinyin() */
int load_pinyin_data(void);
void free_pinyin_data(void);
int parse_pinyin_input(char *chars, int *nch);
void FTsymbol_writeFB(FBDEV *fbdev, const char *txt, int px, int py, EGI_16BIT_COLOR color);

/* Variables for Input Method: PINYIN */
int fw=18;
int fh=20;
wchar_t wcode;
EGI_UNIHAN_SET *uniset;		             /* Sigle unihans */
EGI_UNIHANGROUP_SET *group_set;              /* Unihan group sets */
bool enable_pinyin=false; 	             /* Enable input method PINYIN, set/reset in surf_pinyin(). also as mutex_lock for surfuser_pinyin. */
char strPinyin[4*8]={0}; 	             /* To store unbroken input pinyin string */
char group_pinyin[UNIHAN_TYPING_MAXLEN*4];   /* For divided group pinyins */
EGI_UNICODE group_wcodes[4]; 		     /* group wcodes */
char display_pinyin[UNIHAN_TYPING_MAXLEN*4]; /* For displaying PINYIN typings */
unsigned int pos_uchar;
char unihans[512][4*4]; //[256][4]; /* To store UNIHANs/UNIHANGROUPs complied with input pinyin, in UFT8-encoding. 4unihans*4bytes */
EGI_UNICODE unicodes[512];      /* To store UNICODE of unihans[512] MAX for typing 'ji' totally 298 UNIHANs */
unsigned int unindex;
char phan[4]; 	          /* temp. */
int unicount;             /* Count of unihan(group)s found */
char strHanlist[128];     /* List of unihans to be displayed for selecting, 5-10 unihans as a group. */
char strItem[32];         /* buffer for each UNHAN with preceding index, example "1.啊" */
int GroupStartIndex;      /* Start index for displayed unihan(group)s */
int GroupItems;           /* Current unihan(group)s items in selecting panel */
int GroupMaxItems=7;      /* Max. number of unihan(group)s displayed for selecting */
int HanGroups=0;          /* Groups with selecting items, each group with Max. GroupMaxItems unihans. */
int HanGroupIndex=0;	  /* group index number, < HanGroups */

/*------------------------------------------
This is a thread function for SURFACE PINYIN.
------------------------------------------*/
void* surf_pinyin(void *argv)
{
	const char *mpid_lock_file="/var/run/surfman_guider_pinyin.pid";

	int msw=320;
	int msh=30*2;

	/* -1. Detach thread */
	pthread_detach(pthread_self());

	/* 0. Define variables and Refs */
	EGI_SURFUSER  *msurfuser=NULL;
	EGI_SURFSHMEM *msurfshmem=NULL; /* Only a ref. to surfuser->surfshmem */
	FBDEV	      *mvfbdev=NULL;    /* Only a ref. to &surfuser->vfbdev */
        EGI_IMGBUF    *msurfimg=NULL;   /* Only a ref. to surfuser->imgbuf */
        SURF_COLOR_TYPE mcolorType=SURF_RGB565;   /* surfuser->vfbdev color type */
	//EGI_16BIT_COLOR mbkgcolor;

	/* 1. Register/Create a surfuser */
	egi_dpstd("Register to create a surfuser...\n");
	/* Initial position at (320-msw)/2,(240-msh)/2, OR ... */
	msurfuser=egi_register_surfuser(ERING_PATH_SURFMAN, mpid_lock_file, 0,240-msh, msw,msh,msw,msh, mcolorType);
	if(msurfuser==NULL) {
		egi_dpstd(DBG_RED"Fail to register surface!\n"DBG_RESET);
		return (void *)-1;
	}

	/* 2. Get Ref. pointers to vfbdev/surfimg/surfshmem */
	mvfbdev=&msurfuser->vfbdev;
	//mvfbdev->pixcolor_on=true; //OK, default
	msurfimg=msurfuser->imgbuf;
	msurfshmem=msurfuser->surfshmem;

	/* 3. Assign OP functions, to connect with CLOSE/MIN./MAX. buttons etc. */
	//msurfshmem->minimize_surface  = surfuser_minimize_surface; /* Surface module default function */
	//msurfshmem->redraw_surface	= surfuser_redraw_surface;
	//msurfshmem->maximize_surface	= surfuser_maximize_surface; /* Need resize */
	//msurfshmem->normalize_surface	= surfuser_normalize_surfac; /* Need resize */
	//msurfshmem->close_surface	= surfuser_close_surface;
	//msurfshmem->draw_canvas	= my_draw_canvas;
	//msurfshmem->user_mouse_event  = my_mouse_event;
	msurfshmem->refresh_surface     = my_refresh_surface;

	/* 4. Name for the surface, here it will NOT appear on topbar as for option TOPBAR_NONE. */
	strncpy(msurfshmem->surfname, "EGI_PINYIN", SURFNAME_MAX-1);

	/* 5. First draw surface */
	/* 5.1 Set colors */
	msurfshmem->bkgcolor=COLOR_FloralWhite; //COLOR_Cornsilk; //WEGI_COLOR_GRAYB; /* OR defalt BLACK */
	//msurfshmem->topmenu_bkgcolor=xxx;
	//msurfshmem->topmenu_hltbkgcolor=xxx;
	/* 5.2 First draw default */
	surfuser_firstdraw_surface(msurfuser, TOPBAR_NONE|SURFFRAME_THICK, 0, NULL);  /* Default firstdraw operation */
        /* 5.2.1 Name */
        //FTsymbol_writeFB("EGI全拼输入", 320-120, txtbox.endxy.y-60+3, WEGI_COLOR_ORANGE, NULL, NULL);
        FTsymbol_uft8strings_writeFB(mvfbdev, egi_sysfonts.regular,        /* FBdev, fontface */
                                        18, 20, (const UFT8_PCHAR)"EGI全拼输入",   /* fw,fh, pstr */
                                        120, 1, 0,                   /* pixpl, lines, fgap */
                                        msw-120, 5,                  /* x0,y0, */
                                        COLOR_Coral, -1, 255,  /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);     /* int *cnt, int *lnleft, int* penx, int* peny */
	/* 5.2.2 Division line */
        fbset_color2(mvfbdev, COLOR_Gray);
        draw_line(mvfbdev, 15, msh/2, msw-15, msh/2);

	/* 6. Start Ering Routine  */
	egi_dpstd("Start ering routine...\n");
	if( pthread_create(&msurfshmem->thread_eringRoutine, NULL, surfuser_ering_routine, msurfuser)!=0 ) {
		egi_dperr("Fail to launch thread EringRoutine!\n");
		if(egi_unregister_surfuser(&msurfuser)!=0)
			egi_dpstd("Fail to unregister surfuser!\n");
		return (void *)-1;
	}

	/* 7. Other jobs... */

	/* 8. Activate image */
	msurfshmem->sync=true;

	/* 9. Pass out reference params. */
	surfuser_pinyin=msurfuser;
	enable_pinyin=true;

	/* 10. Init pinyin */
	init_pinyin();

	/* ===== Main Loop ===== */
	while(msurfshmem->usersig !=SURFUSER_SIG_QUIT ) {
		usleep(500000);
	}

	/* P1. Join ering routine */
	//surfuser->surfshmem->usersig=SURFUSER_SIG_QUIT; //Useless if thread is busy calling a BLOCKING function.
	egi_dpstd("Cancel thread...\n");
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	/* Make sure mutex unlocked in pthread if any! */
	/* Useless if thread is busy calling a BLOCKING function. */
	egi_dpstd("Join thread eringRoutine...\n");

#if 1	/* It will be blocked if thread_eringRoutine is busy calling a BLOCKING function. (when the surface is NOT TopSurface!) */
        egi_dpstd("Shutdown surfuser->uclit->sockfd!\n");
        if(shutdown(msurfuser->uclit->sockfd, SHUT_RDWR)!=0)
            egi_dperr("close sockfd");
#endif
	if(pthread_join(msurfshmem->thread_eringRoutine, NULL)!=0)
		egi_dperr("Fail to join eringRoutine\n");

	/* P2. Unregister and destroy surfuser */
	egi_dpstd("Unregister surfuser...\n");
	if(egi_unregister_surfuser(&msurfuser)!=0)
		egi_dpstd("Fail to unregister surfuser!\n");

	egi_dpstd("Surface exit OK!\n");

	/* P3. Reset surfuser_pinyin */
	enable_pinyin=false;
	surfuser_pinyin=NULL;
	surface_pinyin=NULL;

	return (void *)0;
}

/*----------------------------------------------
Reacton on ering_msg_type ERING_SURFACE_REFRESH.

----------------------------------------------*/
void my_refresh_surface(EGI_SURFUSER *surfuser)
{
	int i;

	egi_dpstd(DBG_YELLOW"Surface need refresh!\n"DBG_RESET);


	EGI_SURFSHMEM *surfshmem=surfuser->surfshmem;

        pthread_mutex_lock(&surfshmem->shmem_mutex);
/* ------ >>> Surface shmem Critical Zone */

	/* 1. Erase old images of Typing and Unihan(groups) */
	draw_filled_rect2(&surfuser->vfbdev, surfshmem->bkgcolor, 2,2, surfshmem->vw-120, 30-2);
	draw_filled_rect2(&surfuser->vfbdev, surfshmem->bkgcolor, 2,30-2, surfshmem->vw-1-2, surfshmem->vh-1-2);

	/* 2. Display PINYIN typings */
	if(display_pinyin[0])
            FTsymbol_writeFB(&surfuser->vfbdev, display_pinyin, 15, 3, WEGI_COLOR_FIREBRICK);

        /* 3. Display unihan(group)s on panel for selecting */
        if( unicount>0 ) {
        	bzero(strHanlist, sizeof(strHanlist));
                for( i=GroupStartIndex; i<unicount; i++ ) {
                	snprintf(strItem,sizeof(strItem),"%d.%s ",i-GroupStartIndex+1,unihans[i]);
                        strcat(strHanlist, strItem);

                        /* Check if out of range */
                        //if(FTsymbol_uft8strings_pixlen(egi_sysfonts.regular, fw, fh, (UFT8_PCHAR)strHanlist) > 320-15 )
                        if(FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fw, fh, (UFT8_PCHAR)strHanlist) > 320-15 )
                        {
                        	strHanlist[strlen(strHanlist)-strlen(strItem)]='\0';
                                break;
                        }
                }
                GroupItems=i-GroupStartIndex;
                FTsymbol_writeFB(&surfuser->vfbdev, strHanlist, 15, 3+30, WEGI_COLOR_BLACK);
         }

/* ------ <<<  Surface shmem Critical Zone  */
         pthread_mutex_unlock(&surfshmem->shmem_mutex);

}

/*-----------------------------
Init variables for PINYIN input.
------------------------------*/
void init_pinyin(void)
{
	//bzero(strPinyin,sizeof(strPinyin));
	strPinyin[0]=0;
	unicount=0;
	//bzero unihans[]
	//bzero(display_pinyin, sizeof(display_pinyin));
	display_pinyin[0]=0;
	GroupStartIndex=0; GroupItems=0;
}

/*-----------------------------------------------------------------
1. Free old data of uniset and group_set.
2. Load PINYIN data of uniset and group_set.
3. Load new_words to expend group_set.
4. Load update_words to fix some pingyings. Prepare pinyin sorting.

Return:
        0       OK
        <0      Fails
------------------------------------------------------------------*/
int load_pinyin_data(void)
{
        int ret=0;

        /* 0. Free old data ... */
        UniHan_free_set(&uniset);
        UniHanGroup_free_set(&group_set);

        /* 1. Load UniHan Set for PINYIN Assembly and Input */
        uniset=UniHan_load_set(UNIHANS_DATA_PATH);
        if( uniset==NULL ) {
                egi_dpstd(DBG_RED"Fail to load uniset from %s!\n"DBG_RESET, UNIHANS_DATA_PATH);
                return -1;
        }
        if( UniHan_quickSort_set(uniset, UNIORDER_TYPING_FREQ, 10) !=0 ) {
                egi_dpstd(DBG_RED"Fail to quickSort uniset!\n"DBG_RESET);
                ret=-2; goto FUNC_END;
        }

        /* 2. Load group_set from text first, if it fails, then try data file. */
        group_set=UniHanGroup_load_CizuTxt(UNIHANGROUPS_EXPORT_TXT_PATH);
        if( group_set==NULL) {
                egi_dpstd(DBG_YELLOW"Fail to load group_set from %s, try to load from %s...\n"DBG_RESET,
							UNIHANGROUPS_EXPORT_TXT_PATH,UNIHANGROUPS_DATA_PATH);
                /* Load UniHanGroup Set Set for PINYIN Input, it's assumed the data already merged with nch==1 UniHans! */
                group_set=UniHanGroup_load_set(UNIHANGROUPS_DATA_PATH);
                if(group_set==NULL) {
                        printf("Fail to load group_set from %s!\n",UNIHANGROUPS_DATA_PATH);
                        ret=-3; goto FUNC_END;
                }
        }
        else {
                /* Need to add uniset to grou_set, as a text group_set has NO UniHans inside. */
                if( UniHanGroup_add_uniset(group_set, uniset) !=0 ) {
                        egi_dpstd(DBG_RED"Fail to add uniset to  group_set!\n"DBG_RED);
                        ret=-4; goto FUNC_END;
                }
        }

        /* 3. quickSort typing for PINYIN input */
        if( UniHanGroup_quickSort_typings(group_set, 0, group_set->ugroups_size-1, 10) !=0 ) {
                egi_dpstd(DBG_RED"Fail to quickSort typings for group_set!\n"DBG_RED);
                ret=-5;
                goto FUNC_END;
        }

FUNC_END:
        /* If fails, free uniset AND group_set */
        if(ret!=0) {
                UniHan_free_set(&uniset);
                UniHanGroup_free_set(&group_set);
        }

        return ret;
}

/*------------------------
Free pinyin data.
-------------------------*/
void free_pinyin_data(void)
{
        UniHan_free_set(&uniset);
        UniHanGroup_free_set(&group_set);
}


/*--------------------------------------------------
Convert PINYIN typing into UNIHANs with UTF-8 encoding.

Note:
1. Keys and Operations:
   LowCase alphabets:  As PINYIN typings.
   UppCase alphabets:  As input chars.
   SPACE: 	  Select first unihan(group) in the list panel.
   Digits:        Select unihan(group) with index number as input digit.
   DownArrow/RightArrow:   Display next page of unihan(group)s onto list panel.
   UpArrow/LeftArrow:      Display prev page of unihan(group)s onto list panel.
   ESC:		  To empty/cancel all unihan(group)s in the list panel.
   BACKSPACE:     Erase the last char in pinyin typing and update unihan(group)s.

TODO:
1. If chars[] still has chars left ....save them.


@chars:	 Input as ASCII chars as PINYIN typings.
	 Output as unihan/unhihan_group in UTF-8.
@nch:	 Total number of input ASCII chars.
	 Length of

Return:
	0	OK
	<0	Fails
---------------------------------------------------*/
int parse_pinyin_input(char *chars, int *nch)
{
	int i,j, k,n;
	char ch;

	egi_dpstd(DBG_GREEN"display_pinyin: %s\n"DBG_RESET, display_pinyin);
	egi_dpstd("nch=%d: ", *nch);
	for(i=0; i<*nch; i++) printf("0x%02x ",chars[i]);
	printf("\n");

#if 0 /* TEST: ---------- */
	chars[*nch]=0;
	egi_dpstd(DBG_GREEN"input: %s\n"DBG_RESET,chars);
	strcat(display_pinyin, chars);
	//strncpy(display_pinyin, chars, nch);
	return 0;
#endif

	/* Parse each input char */
	for(n=0; n<*nch; n++) {
		ch=chars[n];

		/* F0. Parse TTY input escape sequences for Control Key */
		if(ch==27) {
//		   egi_dpstd("---- Escape Sequence ----\n");

		   /* F0.1 ESCAPE: 1 OR 2 chars */
		   if( n+1==*nch || (n+1<*nch && chars[n+1]==0) ) {
			/* Clear */
			bzero(strPinyin,sizeof(strPinyin));
			bzero(display_pinyin, sizeof(display_pinyin));
			unicount=0; HanGroupIndex=0;

			/* Increment and Continue */
			n+=1; continue;
		   }

		   /* F0.2 Other Control Key: 3 chars, Sequence starts with "\033[" */
		   if( n+2<*nch && chars[n+1]=='[' ) {  /* 91 '[' */
			switch(chars[n+2]) {
				case 'A':  /* UP Arrow: "\033[A" */
				case 'D':  /* LEFT Arrow: "\033[D" */
				   //egi_dpstd("---- UP Arrow ----\n");
				   if(strPinyin[0]!=0) {
					for(i=7; i>0; i--) {
					    if(GroupStartIndex-i <0)
						continue;
					    /* Generate new unihan(group) list in panel */
					    bzero(strHanlist, sizeof(strHanlist));
					    for(j=0; j<i; j++) {
					        snprintf(strItem, sizeof(strItem), "%d.%s ",j+1,unihans[GroupStartIndex-i+j]);
						strcat(strHanlist, strItem);
					    }
					    /* Check if space if OK */
					    if(FTsymbol_uft8string_getAdvances(egi_sysfonts.regular, fw, fh, (UFT8_PCHAR)strHanlist)<320-15 )
						break;
					}
				   }
				   /* Decrease new GroupStartIndex and GroupItems */
				   GroupStartIndex -=i;

				   n+=2; continue;
				   break;
				case 'B':  /* DOWN Arrow: "\033[B" */
				case 'C':  /* RIGHT Arrow: "\033[C" */
				   if(strPinyin[0]!=0) {
					if( GroupStartIndex+GroupItems < unicount )
						GroupStartIndex += GroupItems;
				   }

				   n+=2; continue;
				   break;
				default:
				   n+=2; continue;
				   break;
			}

		   } /* END F0.2 */

		} /* END F0. */


		/* F1. BACKSPACE */
		if( ch==0x7F ) {
		   /* F1.1 strPinyin has content */
		   if(strlen(strPinyin)>0 ) {
			/* F1.1.1 Erase last char */
			strPinyin[strlen(strPinyin)-1]='\0';
			/* F1.1.2 Since strPinyin is updated: To find out all unihans complying with input pinyin typing. */
			HanGroupIndex=0;
			UniHan_divide_pinyin(strPinyin, group_pinyin, UHGROUP_WCODES_MAXSIZE);
			/* F1.1.3 Copy/Formulate group_pinyin to display_pinyin */
			bzero(display_pinyin, sizeof(display_pinyin));
			for(i=0; i<UHGROUP_WCODES_MAXSIZE; i++) {
				strcat(display_pinyin, group_pinyin+i*UNIHAN_TYPING_MAXLEN);
				strcat(display_pinyin, " ");
			}
			/* F1.1.4 Locate typings and get group_set */
			unicount=0;
			if( UniHanGroup_locate_typings(group_set, group_pinyin)==0 ) {
				k=0;
				/* F1.1.4.1 Copy unigroups to unihans[] for displaying */
				while( k< group_set->results_size ) {
					pos_uchar=group_set->ugroups[group_set->results[k]].pos_uchar;
					strcpy(unihans[unicount], (char *)group_set->uchars+pos_uchar);
					unicount++;
					if( unicount > sizeof(unihans)/sizeof(unihans[0]) )
						break;
					k++;
				}
			}
			/* F1.1.5 Reset/Init. Index/Itmes for displaying */
			GroupStartIndex=0; GroupItems=0;

			*nch=0; return 0; /* TODO: Assume nch==1! */
		   }
		   /* F1.2 No content in strPinyin, revert it. */
		   else  { //(strlen(strPinyin)==0 )
			*nch=1; return 0; /* TODO: Assume nch==1! */
		   }
		}

		/* F2. Insert chars:  !!! printable chars + '\n' +TAB !!! */
		else if( ch>31 || ch==9 || ch==10 ) {
			/* F2.1  SPACE: Select unihans[0] OR insert SPACE. */
			if( ch==32 ) {
				/* F2.1.1 If strPinyin is empty, revert/keep it. */
				if( strPinyin[0]=='\0' ) {
					*nch=1; return 0; /* TODO: Assume nch==1 ! */
				}
				/* F2.1.2 Get/Return the first unihan(group) in the displaying panel */
				*nch=strlen(unihans[0]);
				strncpy(chars,unihans[0], *nch);
				/* F2.1.3 Clear strPinyin and unihans[] */
				bzero(strPinyin, sizeof(strPinyin));
				unicount=0;
				bzero(display_pinyin, sizeof(display_pinyin));
				/* F2.1.4 Reset/Init. Index/Itmes for displaying */
				GroupStartIndex=0; GroupItems=0;

				return 0;
			}
			/* F2.2  DIGITS: Select Unihan(group) OR keep digit as a char. */
			else if(isdigit(ch)) {
				/* F2.2.1 Select Unihan(group) as per digit index */
				if(strPinyin[0]!='\0' && (ch-48>0 && ch-48<=GroupItems) ) {
					/* F2.2.1.1 Get/Return the selected unihan(group) in the displaying panel */
					unindex=GroupStartIndex+ch-48-1;
					*nch=strlen(&unihans[unindex][0]); /* TODO : strlen(unihans[unindex] */
					strncpy(chars, &unihans[unindex][0], *nch);

					/* F2.2.1.2 Clear strPinyin and unihans[] */
					bzero(strPinyin,sizeof(strPinyin));
					unicount=0;
					//bzero unihans[]
					bzero(display_pinyin, sizeof(display_pinyin));

					/* F2.2.1.3 Reset/Init. Index/Itmes for displaying */
					GroupStartIndex=0; GroupItems=0;

					return 0;
				}
				/* F2.2.2 Keep as a digit, as No content in strPinyin[] or unihans[] */
				else {
					*nch=1; return 0; /* TODO: assume nch==1 */
					//continue;
				}
			}
			/* F2.3  ALPHABET:  Input as part of PINYIN typing. */
			else if( (isalpha(ch) && islower(ch)) || ch==0x27 ) { /* ' as delimiter/separator for typing */
				/* F2.3.1 Continue input to strPinyin ONLY IF space is available in strPinyin */
				if( strlen(strPinyin) < sizeof(strPinyin)-1 ) {
					/* F2.3.1.1 Add to strPinyin[] */
					strPinyin[strlen(strPinyin)]=ch;
					egi_dpstd(DBG_GREEN"strPinyin: %s\n"DBG_RESET, strPinyin);
					/* F2.3.1.2 Since strPinyin updated: To find out all unihans complying with the typing */
					HanGroupIndex=0;
					bzero(&unihans[0][0],sizeof(unihans));
					UniHan_divide_pinyin(strPinyin, group_pinyin, UHGROUP_WCODES_MAXSIZE);
					/* F2.3.1.3 Copy/Formulate group_pinyin to display_pinyin */
					bzero(display_pinyin, sizeof(display_pinyin));
					for(i=0; i<UHGROUP_WCODES_MAXSIZE; i++) {
						strcat(display_pinyin, group_pinyin+i*UNIHAN_TYPING_MAXLEN);
						strcat(display_pinyin, " ");
					}
					/* F2.3.1.4 Locate typings and get group_set */
					unicount=0;
					if( UniHanGroup_locate_typings(group_set, group_pinyin)==0 ) {
						k=0;
						while( k < group_set->results_size ) {
							pos_uchar=group_set->ugroups[group_set->results[k]].pos_uchar;
							strcpy(unihans[unicount], (char *)group_set->uchars +pos_uchar);
							unicount++;
							if(unicount > sizeof(unihans)/sizeof(unihans[0]))
								break;
							k++;
						}
					}
					/* F2.3.1.4 Reset/Init. Index/Itmes for displaying */
					GroupStartIndex=0; GroupItems=0;

					*nch=0; return 0;
				}
			}
			/* F2.4  ELSE: Convert to SBC case(full width)  */
			else {
				/* TODO: */
			}

		} /* End F2. Insert char */
		/* F3. TODO: Other undefined chars, NOW ignore.... */
		else {
		}

	}  /* End for() */

	/* NOW clear it, before ering to SURFACEs. */
	// *nch=0;  /* Nope! need to pass '!#$...' to surfaces. */

	return 0;
}


/*-------------------------------------
FTsymbol Write, single line.

@txt:           Input text
@px,py:         Start point.
@color:         txt color.
@penx, peny:    pointer to pen X.Y.
--------------------------------------*/
void FTsymbol_writeFB(FBDEV *fbdev, const char *txt, int px, int py, EGI_16BIT_COLOR color)
{

        FTsymbol_uft8strings_writeFB(   fbdev, egi_sysfonts.regular,    /* FBdev, fontface */
                                        fw, fh,(const UFT8_PCHAR)txt,   /* fw,fh, pstr */
                                        320, 1, 0,            /* pixpl, lines, fgap */
                                        px, py,               /* x0,y0, */
                                        color, -1, 255,       /* fontcolor, transcolor,opaque */
                                        NULL, NULL, NULL, NULL);      /* int *cnt, int *lnleft, int* penx, int* peny */
}

