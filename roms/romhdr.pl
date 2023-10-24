#!/usr/bin/perl

use integer;
use strict;

foreach my $r (@ARGV) {
    next unless (-f $r);
    my @st = stat(_);
    my $n = $r;
    $n =~ s/^(\..)?/rom_/;
    $n =~ s/\.[A-Za-z0-9]+$//;
    $n =~ s/[^A-Za-z0-9]+/_/g;
    printf "extern const unsigned char %s[%u];\n", $n, $st[7];
}

exit 0;
