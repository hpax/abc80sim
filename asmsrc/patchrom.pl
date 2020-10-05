#!/usr/bin/perl

use strict;
use integer;

my($infile, $mapfile, $outfile) = @ARGV;

unlink($outfile);

open(my $in, '<:raw', $infile)
    or die "$0: cannot open input file: $infile: $!";
open(my $map, '<', $mapfile)
    or die "$0: cannot open map file: $mapfile: $!";

my $fileroot;
$fileroot = $mapfile;
$fileroot =~ s/\.map$//;

my $data;
read($in, $data, 65536);
close($in);

my @patches = ();
my $origin  = 0;

while (defined(my $l = <$map>)) {
    if ($l =~ /^__head\s*=?\s*\$?([0-9A-Fa-f]+)/) {
	$origin = hex $1;
    } elsif ($l =~ /^__(\w+)_head\s*=?\s*\$?([0-9A-Fa-f]+)/) {
	my $patchname = $1;
	my $patchaddr = hex $2;
	push(@patches, [$patchname, $patchaddr]);
    }
}
close($map);

foreach my $p (@patches) {
    if ($p->[1] < $origin) {
	die "$0:$infile: patch ", $p->[0], " starts before file origin\n";
    }

    my $patchfile = $fileroot.'_'.$p->[0].'.bin';
    open(my $patch, '<:raw', $patchfile)
	or die "$0:$infile: cannot find open file $patchfile: $!\n";
    my $patchdata;
    read($patch, $patchdata, 65536);
    close($patch);

    substr($data, $p->[1] - $origin, length($patchdata)) = $patchdata;
}

open(my $out, '>:raw', $outfile)
    or die "$0: cannot open output file: $outfile: $!\n";
print $out $data;
close($out);
