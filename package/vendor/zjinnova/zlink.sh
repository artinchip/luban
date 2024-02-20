#!/bin/sh

###########################
EXE_PATH=/usr/bin/
###########################

$EXE_PATH/mdnsd &
#$EXE_PATH/z-mdnsd &
while true;do
    $EXE_PATH/z-link 
    sleep 1 
done
