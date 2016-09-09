#!/bin/bash
set -e
if [ -z "$NB_FRONTEND" ]
then
  echo "[WARN] No lab notebook set up"
fi
gl=$(git log -n 3)
gd=$(git diff)
gs=$(git status)
make clean
bldnum=$(cat .build_num)
bldnum=$(($bldnum+1))
echo $bldnum > .build_num
echo "#define BUILD_NUMBER $bldnum" > version.h
make -j48
sum=$(md5sum bin/hamilton/anemometer.bin)
if [ -n "$NB_FRONTEND" ]
then
  nb anemometer.build gitlog "$gl" gitdiff "$gd" gitstatus "$gs" md5 "$sum" build "$bldnum"
fi
echo "BUILT b$bldnum"
