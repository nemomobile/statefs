#!/bin/bash

if [ $# -ne 3 ]; then
    echo "Usage: ${0} <head> <tag> <dst-path>"
    exit 1
fi

HEAD=$1
TAG=$2
DST=`realpath $3`
MAIN=`realpath ${DST}/statefs-${TAG}.tar`
COR=`realpath ${DST}/statefs-${TAG}-cor.tar`
TUT=`realpath ${DST}/statefs-${TAG}-tut.tar`
echo "Main"
git archive --prefix=statefs-${TAG}/ --format=tar -o ${MAIN} ${HEAD}
cd cor
echo "Cor"
git archive --prefix=statefs-${TAG}/cor/ --format=tar -o ${COR} HEAD
cd tut
pwd
echo "Tut"
git archive --prefix=statefs-${TAG}/cor/tut/ --format=tar -o ${TUT} HEAD
echo "Merge"
tar -Af ${MAIN} ${COR}
tar -Af ${MAIN} ${TUT}
rm -f ${MAIN}.bz2
bzip2 ${MAIN}
rm ${COR} ${TUT}
