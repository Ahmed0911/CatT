#!/bin/bash
gpio export 18 out
while :
do
raspivid -vf -n -w 1280 -h 720 -cd MJPEG -t 0 -fps 30 -b 5000000 -l -o tcp://0.0.0.0:12345
sleep 1
done