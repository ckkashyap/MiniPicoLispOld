#!/usr/bin/perl

use strict;
use warnings;

while(<>)
{
    next unless /^0x/;

    print "$_";
    
}
