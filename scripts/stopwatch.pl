#!/usr/bin/perl

use strict;
use warnings;

# if($#ARGV != 2){
#     die("Wrong number of arguments\ntry: stopwatch.pl EXECUTABLE");
# }

my $executable = $ARGV[0];
my $results_file = "./results.txt";
print $executable;

open(my $FILE, ">>", $results_file) or die $!;

my $exitflag = 0;

$SIG{INT} = sub { $exitflag=1 };

while(!$exitflag)
{
    my @exe_out = qx($executable);
    sleep(10);    
    print $FILE "@exe_out";
}

close $FILE;



