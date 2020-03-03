/*----------------- !!! OBSOLETE !!! --------------------------------
    ===---  This is for clock digital number only !!!!  ---===

Dictionary for symbols

Midas Zhou
--------------------------------------------------------------------*/
#include <stdint.h>
#include <linux/fb.h>
#include "egi_fbgeom.h"

#define DICT_IMG_WIDTH 240
#define DICT_IMG_HEIGHT 320

/* number of symbols in a dict */
#define DICT_NUM_LIMIT_H20W15 32  /* MAX. 16*16=256 */

/* symbols dict data  */
extern uint16_t *dict_h20w15;


/* -----------------  function  ------------------ */
uint16_t *dict_init_h20w15(void);
void dict_display_img(FBDEV *fb_dev,char *path);
uint16_t *dict_load_h20w15(char *path);
void dict_print_symb20x15(uint16_t *dict);
void dict_writeFB_symb20x15(FBDEV *fb_dev, int blackoff, uint16_t color, \
                                 int index, int x0, int y0);
void wirteFB_str20x15(FBDEV *fb_dev, int blackoff, uint16_t color, \
                                 char *str, int x0, int y0);
void dict_release_h20w15(void);
