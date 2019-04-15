#!/bin/sh

# $Id: version.sh 989 2015-01-13 07:49:35Z xiangw $ 
####################################

svn_revision=`cd "$1" && LC_ALL=C svn info 2> /dev/null | grep Revision | cut -d' ' -f2`
test $svn_revision || svn_revision=`cd "$1" && grep revision .svn/entries 2>/dev/null | \
                                    cut -d '"' -f2 2> /dev/null`
test $svn_revision || svn_revision=000

if test "$svn_revision" = UNKNOWN && test -n "$2"; then
    NEW_REVISION="#define BUILD_VERSION \"$2\""
else
    NEW_REVISION="#define BUILD_VERSION $svn_revision"
fi
OLD_REVISION=`cat version.h 2> /dev/null`

# Update version.h only on revision changes to avoid spurious rebuilds
if test "$NEW_REVISION" != "$OLD_REVISION"; then
    echo "$NEW_REVISION" > ./app/ver.h
fi

