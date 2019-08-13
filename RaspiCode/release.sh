#!/bin/bash
sudo ./setpwm 17

echo "Content-type: text/html"
echo ""

echo '<html>'
echo '<head>'
#echo '<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">'
echo '<meta http-equiv="refresh" content="3;url=http://192.168.1.10/">'
echo '<title>Door State</title>'
echo '</head>'
echo '<body>'
echo '<font size="7">RELEASED</font>'
echo '</body>'
echo '</html>'

exit 0