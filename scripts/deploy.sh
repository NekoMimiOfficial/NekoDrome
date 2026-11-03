#!/usr/bin/bash
set -e

TITLE_ID="NEMIKO001"

rm -rf eboot.bin
cp ./nekodrome.self ./eboot.bin

curl --ftp-method nocwd -T eboot.bin ftp://$VITA_IP:1337/ux0:/app/$TITLE_ID/
echo "launch $TITLE_ID" | nc $VITA_IP 1338
