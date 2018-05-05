#!/bin/sh

PROGRAM=thor.py
HOST=$1
PORT=$2
RUNNINGTOTAL=0

for n in {1..5}; do
./$PROGRAM "$HOST":"$PORT"/
	TIME=$(./$PROGRAM "$HOST":"$PORT"/ | cut -d ' ' -f 7)
	echo $TIME
done
for n in {1..5}; do
./$PROGRAM "$HOST":"$PORT"/text/index.html
done
for n in {1..5}; do
./$PROGRAM "$HOST":"$PORT"/scripts/env.sh
done
