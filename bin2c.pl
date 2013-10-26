#!/usr/bin/perl
## -----------------------------------------------------------------------
##   
##   Copyright 1998-2013 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# bin2c.pl: binary file to C source converter
#

eval { use bytes; };
eval { binmode STDIN; };

if ( $#ARGV != 0 ) {
    print STDERR "Usage: $0 input_file [offset] > output_file\n";
    exit 1;
}

($input_file, $offset) = @ARGV;

$table_name = $input_file;
$table_name =~ s/\.[^\.]*$//;	# Drop extension

unless (defined ($offset)) {
    $offset = 0x404b if ($table_name eq 'abcdev');
    $offset = 0x6000 if ($table_name =~ /dos$/);
    $offset = 0x7800 if ($table_name eq 'printrom');
}

open(IN, '<', $input_file)
    or die "$0: unable to open input file $input_file: $!\n";

print "#include \"rom.h\"\n\n";

printf "static const unsigned char data[] = {\n", $table_name;

$pos = 0;
$linelen = 8;

$total_len = 0;

while ( ($n = read(IN, $data, 4096)) > 0 ) {
    $total_len += $n;
    for ( $i = 0 ; $i < $n ; $i++ ) {
	$byte = substr($data, $i, 1);
	if ( $pos >= $linelen ) {
	    print ",\n\t";
	    $pos = 0;
	} elsif ( $pos > 0 ) {
	    print ", ";
	} else {
	    print "\t";
	}
	printf("0x%02x", unpack("C", $byte));
	$pos++;
    }
}

print "\n};\n\n";
printf "const struct rom %s = { data, %u, %u };\n",
    $table_name, $offset, $total_len;

# @st = stat STDIN;
# printf "\nunsigned int %s_len = %u;\n", $table_name, $total_len;
# printf "\nint %s_mtime = %d;\n", $table_name, $st[9];

exit 0;
