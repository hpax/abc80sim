#!/bin/sed
s/\([\\"]\)/\\\1/g
s/^/"/
s/$/\\n"/
1i\
extern const char helptxt[];\
const char helptxt[] =
$a\
;
