#!/bin/bash
kill -INT `cat ./our-smtp-proxy.pid`
echo "SIGINT sent, check proxy.log to see result"
