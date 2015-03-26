#!/bin/bash
echo "Testing new transport.ini"
./our-smtp-proxy -t
[ "$?" == "1" ] && exit
echo "Reloading..."
kill -HUP `cat ./our-smtp-proxy.pid`
echo "SIGHUP sent, check proxy.log to see result"
