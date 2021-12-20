#!/bin/bash

# Lab4: LOSS=0, CORRUPT=0
SPEED=10
DELAY=10
LOSS=0
CORRUPT=100
#BDP - to be modified when implementing sliding window
BDP=$((SPEED * DELAY))

killall -9 link 2> /dev/null
killall -9 recv 2> /dev/null
killall -9 send 2> /dev/null

./link_emulator/link speed=$SPEED delay=$DELAY loss=$LOSS corrupt=$CORRUPT &> /dev/null &
sleep 1
./recv &
sleep 1

./send $BDP
