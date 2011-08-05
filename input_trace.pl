#!/usr/bin/perl -w

$numArgs = $#ARGV + 1;

if($numArgs != 2){
  print "USAGE: ./input_trace input_file output_file\n";
  exit;
}

$inputFile = $ARGV[0];
$outputFile = $ARGV[1];

if(open(IN, $inputFile)){
  if(open(OUT, ">".$outputFile)){
    while(<IN>){
      chomp $_;
      @tokens = split(/ /, $_);
      print OUT $tokens[0]."\n";
    }
  }
  else{
    print "Unable to open $outputFile\n";
    exit;
  }
}
else{
  print "Unable to open $inputFile\n";
  exit;
}

close(IN);
close(OUT);
