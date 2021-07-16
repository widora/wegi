#!/bin/bash

all_apps="test_surfman test_surfuser surfman_guider surf_wetradio surf_madplay surf_editor surf_wallpaper surf_book surf_wifiscan surf_tetris"

if [ ${*} == "clean" ]
then
 echo "Clean $all_apps"
 rm $all_apps
 exit
fi

for app in $all_apps
do
	make test TEST_NAME=$app EGILIB=static
done
