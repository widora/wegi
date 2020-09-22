#!/bin/sh
#############################################################################
# To play rtsp live mp3 stream with mplayer.
#
# NOTE:
# 1. Most case ~1s latency! sometimes <0.5s.
# 2. It takes too long time to load lavf?? 
#    libavformat file format detected. ( start to decode.)
#      ...  waiting ...
#    [lavf] stream 0: audio (mp2), -aid 0 (start to play)
# 3.For mplayer, to press '->' to search to 0 position will shorten the latency!
#
# Midas Zhou
###############################################################################

while [ 1 ]
do

  ## OPTIONS
  # -nortc  -vfm ffmpeg  
  # -cache 1024  -nocache
  # -rtsp-stream-over-tcp
  # NOSYNC: -autosync 0 -mc 0, SYNC: -autosync 30 -mc 2.0
  mplayer -nortc -vfm ffmpeg -ac ffmp2 ${*}

  sleep 1
done
