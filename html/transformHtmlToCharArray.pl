#!/usr/bin/perl

use strict;

open(my $input, "<", "website.html")
    or die "Can't open < website.html: $!";

open(my $output, ">", "../src/websiteMgmt.h")
    or die "Can't open < websiteMgmt.h: $!";

print $output "static const char mgtWebsite[] PROGMEM = \"";
while (my $row = <$input>) {
    $row =~ s/\"/\\"/g;
    $row =~ s/\n/\\/g;
    print $output "$row\n";
}
print $output "\";";

close($input);
close($output);