#!/bin/sh
########################################
# A test script to push live mp3 stream.
# Midas Zhou
########################################
while [ 1 ]
do
  killall -9 ffmpeg
  killall -9 ffserver

  ffserver -f ffmp3.conf -d &
  sleep 1

  ## OPTIONS
  ## -rtsp_transport tcp
  ## -bufsize 1k   -f alsa -i hw:0,0
  ffmpeg -v verbose -f alsa -thread_queue_size 1024 -i default -b:a 48k -c:a mp2  http://localhost:8090/feed_mp3.ffm

  sleep 1
done
