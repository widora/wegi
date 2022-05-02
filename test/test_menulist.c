/*------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Journal:
2022-04-29:
	1. Create the file.

Midas Zhou
midaszhou@yahoo.com(Not in use since 2022_03_01)
------------------------------------------------------------------*/
#include <stdio.h>
#include "egi_common.h"
#include "egi_FTsymbol.h"
#include "egi_surface.h"
#include "egi_surfcontrols.h"

int main(int argc, char** argv)
{


        ESURF_MENULIST *mlist_sysRoot;

#if 0 ///////////////////////////////////////////////////
	ESURF_MENULIST *mlist_sensors, *mlist_temp;
        /* P1. Create a MenuList Tree */
        /* P1.1 Create root mlist: ( mode, root, x0, y0, mw, mh, face, fw, fh, capacity) */
        mlist_sysRoot=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, true, 10, 240-10,
                                                                                        100, 30, egi_sysfonts.regular, 16, 16, 8 );
        mlist_sensors=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0,0, 120, 30, egi_sysfonts.regular, 16, 16, 8 );
        mlist_temp=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0,0, 100, 30, egi_sysfonts.regular, 16, 16, 8 );

	/* Add items to mlist_temp */
        egi_surfMenuList_addItem(mlist_temp, "Temp0", NULL, NULL);
        egi_surfMenuList_addItem(mlist_temp, "Temp1", NULL, NULL);
        egi_surfMenuList_addItem(mlist_temp, "Temp2", NULL, NULL);
        egi_surfMenuList_addItem(mlist_temp, "Temp3", NULL, NULL);

        /* P1.2 Add items to mlist_sensors */
        egi_surfMenuList_addItem(mlist_sensors, "Sensor0", NULL, NULL);
        egi_surfMenuList_addItem(mlist_sensors, "Sensor1", NULL, NULL);
        egi_surfMenuList_addItem(mlist_sensors, "Temp", mlist_temp, NULL);
        egi_surfMenuList_addItem(mlist_sensors, "Sensor3", NULL, NULL);

        /* P1.3 Add items to mlist_sysRoot */
        egi_surfMenuList_addItem(mlist_sysRoot, "Monitor", NULL, NULL);
        egi_surfMenuList_addItem(mlist_sysRoot, "subRoot", mlist_sensors, NULL);
        egi_surfMenuList_addItem(mlist_sysRoot, "Preference", NULL, NULL);

#else
        ESURF_MENULIST *mlistMonitor;
        ESURF_MENULIST *mlistSetting;
        ESURF_MENULIST *mlistSensors;
        ESURF_MENULIST *mlistNetstat;
        ESURF_MENULIST *mlistNetSetting;

        /* P1. Create a MenuList Tree */
        /* P1.1 Create root mlist: ( mode, root, x0, y0, mw, mh, face, fw, fh, capacity) */
        mlist_sysRoot=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, true, 0, 0, /* At this point, layout is not clear. x0/y0 set at P3 */
                                                                                        110, 30, egi_sysfonts.regular, 16, 16, 8 );
        mlistMonitor=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0,0, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
        mlistSetting=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0,0, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
        mlistNetstat=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0,0, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
        mlistNetSetting=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0,0, 110, 30, egi_sysfonts.bold, 16, 16, 8 );
        mlistSensors=egi_surfMenuList_create(MENULIST_ROOTMODE_LEFT_BOTTOM, false, 0,0, 110, 30, egi_sysfonts.bold, 16, 16, 8 );

        /* P2.1 Add items to mlistSensors */
        egi_surfMenuList_addItem(mlistSensors, "Sensor0", NULL, NULL);
        egi_surfMenuList_addItem(mlistSensors, "Sensor1", NULL, NULL);
        egi_surfMenuList_addItem(mlistSensors, "Sensor2", NULL, NULL);
        egi_surfMenuList_addItem(mlistSensors, "Sensor3", NULL, NULL);

        /* P2.2 Add items to mlistNetstat */
        egi_surfMenuList_addItem(mlistSetting, "System", NULL, NULL);
        egi_surfMenuList_addItem(mlistSetting, "Sound", NULL, NULL);
        egi_surfMenuList_addItem(mlistSetting, "BackLight", NULL, NULL);
        egi_surfMenuList_addItem(mlistSetting, "Power", NULL, NULL);

        /* P2.3 Add items to mlistMonitor */
        egi_surfMenuList_addItem(mlistMonitor, "CPU Temp", NULL, NULL);
        egi_surfMenuList_addItem(mlistMonitor, "CPU Load", NULL, NULL);
        egi_surfMenuList_addItem(mlistMonitor, "Netstat", mlistNetstat, NULL);
        egi_surfMenuList_addItem(mlistMonitor, "Sensors", mlistSensors, NULL);

        /* P3.1 Add items to mlistNetSetting */
        egi_surfMenuList_addItem(mlistNetSetting, "IP", NULL, NULL);
        egi_surfMenuList_addItem(mlistNetSetting, "WiFi", NULL, NULL);

        /* P3.2 Add items to mlistNetstat */
        egi_surfMenuList_addItem(mlistNetstat, "Setting", mlistNetSetting, NULL);
        egi_surfMenuList_addItem(mlistNetstat, "Interface", NULL, NULL);
        egi_surfMenuList_addItem(mlistNetstat, "Traffic", NULL, NULL);

        /* P4. Add items to menulist */
        egi_surfMenuList_addItem(mlist_sysRoot, "设置", mlistSetting, NULL);
        egi_surfMenuList_addItem(mlist_sysRoot, "Monitor", mlistMonitor, NULL);
        egi_surfMenuList_addItem(mlist_sysRoot, "Preference", NULL, NULL);

#endif

	int ox=0,oy=0;
	int offYU=0, offYD=0, offXR=0, offXL=0;
	if(egi_surfMenuList_getMaxOffset(mlist_sysRoot, ox, oy, &offYU, &offYD, &offXR, &offXL)<0)
		printf("Fail to get MaxLayoutSize!\n");
	printf("offYU=%d, offYD=%d, offXR=%d, offXL=%d\n", offYU, offYD, offXR, offXL);

	int w,h;
	egi_surfMenuList_getMaxLayoutSize(mlist_sysRoot, &w, &h);
	printf("Max layout size: W%dxH%d\n", w,h);

	return 0;
}
