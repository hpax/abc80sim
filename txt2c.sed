#!/bin/sed
s/\([\\"]\)/\\\1/g
s/^/"/
s/$/\\n"/
1i\
extern const char MODULE[];\
const char MODULE[] =
$a\
;
