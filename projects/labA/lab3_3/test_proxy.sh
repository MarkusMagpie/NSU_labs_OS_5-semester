#!/bin/bash

gcc -o proxy proxy_cache.c mycache.c && sudo ./proxy
http_proxy=http://localhost:80 wget -O /dev/null http://speedtest.selectel.ru/10MB
http_proxy=http://localhost:80 wget -O /dev/null http://speedtest.selectel.ru/100MB