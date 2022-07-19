#!/usr/bin/perl

use strict;
use warnings;


if($#ARGV + 1 != 2){
	die "Usage: perl runner.pl <n_times> <command>\n";
}

my $times = shift or die "Missing argument: times_to_run";
my $executable = shift or die "Missing argument: command_to_run";

for (1..$times) {
	
	
	# executes the program
	# if the program exited with an error, exit the loop
	if (system($executable) != 0) {
		last;
	}

	print "$_\n";
}

