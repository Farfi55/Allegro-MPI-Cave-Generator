my $executions = 0;
my $tot_time = 0;

my $results_file = "./results.txt";

open(FILE2, "<", $results_file) or die $!;

for (<FILE2>){
    print $_;
    chomp;
    $tot_time += $_;
    $executions++;
}
close $FILE2;

print "$executions executions in $tot_time ms\n";
print $tot_time/$executions;
print "ms avrage\n";
