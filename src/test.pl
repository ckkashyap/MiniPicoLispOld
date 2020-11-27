#!/usr/bin/perl

use strict;
use warnings;

`../bin/picolisp test.l > /tmp/t1`;
my$diff=`diff /tmp/t1 testoutput.txt`;
if ($diff ne "")
{
    print "PicoLisp does not match baseline --> ";
    print "$diff\n";
    exit;
}

`../bin/picolisp test.l > /tmp/t2`;
$diff=`diff /tmp/t2 testoutput.txt`;
if ($diff ne "")
{
    print "lisp does not match baseline --> ";
    print "$diff\n";
    exit;
}



print "PASS\n";
