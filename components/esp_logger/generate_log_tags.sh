#!/bin/bash
LOG_TAGS=`grep -RPho "TAG ?= ?\"\\K([0-9A-Za-z_]+)"  . | sort | uniq`


echo "const char *log_tags[] = {" > $1
for tag in $LOG_TAGS;
do
  echo "    \"$tag\"," >> $1
done
echo "    0" >> $1
echo "};" >> $1
clang-format -i $1
