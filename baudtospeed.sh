#!/bin/sh
#
# Optionally take the input from cc -Wp,-dM; and create a conversion
# functions from baud to POSIX speed_t. At least try to.
#

# Common speeds, including all the POSIX required ones
stdspeeds='50 75 110 134 150 200 300 600 1200 1800 2400 4800 9600
19200 38400 57600 115200 230400 460800 500000 576000 921600 1000000
1152000 1500000 2000000 2500000 3000000 3500000 4000000'

speeds=`(
    for s in $stdspeeds; do echo $s; done
    sed -ne 's/^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*B\([1-9][0-9]*\)[[:space:]].*$/\1/p'
) | sort -n -u`

echo '#include "baudtospeed.h"'
echo '#define SPEED_T_IS_SANE /* Begin optimistically... */'

for s in $speeds; do
    echo ''
    echo "#ifdef B$s"
    echo "#if B$s != $s"
    echo "#undef SPEED_T_IS_SANE"
    echo '#endif'
    echo '#endif'
done

echo ''
echo ''

echo 'speed_t baudtospeed(unsigned long baud)'
echo '{'
echo '#ifdef SPEED_T_IS_SANE'
echo '    return baud;'
echo '#else'
echo '    switch (baud) {'

for s in $speeds; do
    echo "#ifdef B$s"
    echo "        case $s:"
    echo "            return B$s;"
    echo '#endif'
done


echo '        default:'
echo '            return B0;'
echo '    }'
echo '#endif /* SPEED_T_IS_SANE */'
echo '}'
