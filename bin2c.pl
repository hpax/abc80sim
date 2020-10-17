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
    print STDERR "Usage: $0 input_file [tablename] > output_file\n";
    exit 1;
}

($input_file, $table_name) = @ARGV;

$table_name = $input_file unless (defined($table_name));

$table_name =~ s/\.[^\.]*$//;		# Drop extension
$table_name =~ s/^.*[^A-Za-z0-9_]//;	# Drop any path prefix

open(IN, '<:raw', $input_file)
    or die "$0: unable to open input file $input_file: $!\n";

$total_len = 0;
$data = '';
while (($n = read(IN, $data, 65536, $total_len)) > 0) {
    $total_len += $n;
}
close(IN);

print "#include \"../rom.h\"\n\n";
printf "const uint8_t %s[%d] = {\n", $table_name, $total_len;

$pos = 0;
$linelen = 8;

for ( $i = 0 ; $i < $total_len ; $i++ ) {
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

print "\n};\n";

exit 0;
