#!/bin/sh
v4l2-ctl -c repeat_sequence_header=1
v4l2-ctl -c h264_i_frame_period=30
v4l2-ctl -c video_bitrate=10000000
/home/pi/Projects/RaspiCamApp/camcap > log.txt

