#!/bin/sh

## To play rstp CAM stream.
## Example: ./mpcam.sh rtsp://192.168.8.1:5456/test.avi
## Go forward by pressing '->' to minish latency

while [ 1 ]
do
  ## -vf scale=240:176 -fs  -vf pp=hb/vb/dr/al  -rtsp-stream-over-tcp
  ## -vfm ffmpeg: Can greatly improve AV sync! but NOT here
  ## For'Too many buffered pts':  -nocorrect-pts
  ## NOSYNC: -autosync 0 -mc 0, SYNC: -autosync 30 -mc 2.0
  ## -brightness 12
  mplayer -nocorrect-pts ${*} -framedrop -fs -vf scale=240:176
  #mplayer -nocorrect-pts ${*} -framedrop -fs -vf rotate=2
  sleep 1
done
