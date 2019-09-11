#!/bin/bash
echo -e "AT^NDISDUP=1,1,\"data.vip.hr\"\r" > /dev/ttyUSB0
sleep 2
sudo dhclient wwan0
sudo noip2
