HEAD=$1
TAG=$2
DST=`realpath $3`
MAIN=`realpath ${DST}/statefs-${TAG}.tar`
COR=`realpath ${DST}/statefs-${TAG}-cor.tar`
git archive --prefix=statefs-${TAG}/ --format=tar -o ${MAIN} ${HEAD}
cd cor
git archive --prefix=statefs-${TAG}/cor/ --format=tar -o ${COR} ${HEAD}
cd ..
tar -Af ${MAIN} ${COR}
gzip ${MAIN}
