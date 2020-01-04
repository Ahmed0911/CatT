sudo ifconfig wlan1 down
sudo iw dev wlan1 set monitor otherbss fcsfail
sudo ifconfig wlan1 up
sudo iwconfig wlan1 channel 13
