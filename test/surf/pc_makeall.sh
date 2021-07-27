#!/bin/bash

all_apps="test_surfman test_surfuser surfman_guider surf_wetradio surf_madplay surf_editor surf_wallpaper surf_book surf_wifiscan surf_tetris"

if [ ${*} == "install" ]
then
 mv $all_apps ./pc_install
 exit
fi

if [ ${*} == "clean" ]
then
 echo "Clean $all_apps"
 rm $all_apps
 exit
fi


for app in $all_apps
do
	make -f PC_Makefile test EGILIB=static TEST_NAME=$app
done
