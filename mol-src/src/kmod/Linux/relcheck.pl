#
# A simple script used to compile relocation information needed
# to move critical low-level code to continuous physical 
# memory
#
# (C) 1999, 2001 Samuel Rydh <samuel@ibrium.se>
#

#use POSIX;

# variables set by make:
# $NM, $OBJDUMP
#$NM = nm;
#$OBJDUMP = objdump;

#print `cat warning.txt`;
$output = `$OBJDUMP -r $ARGV[0]` or die "Script failed";
$output =~ s/^[^0].*$//mg;

$bad = 0;


######################################################################
# eliminate external symbols (absolutely addressed) from the
# reloctable list (and detect relative addressing of external
# symbols - we can't use such since the 24bit limit will cause problem)
######################################################################

@ext_syms = (`$NM -g -u $ARGV[0]` or die "nm failed") =~ /(\w*)\n/g;

foreach( @ext_syms ) {
    if( $output =~ /.*PPC_REL.*$_[+\n\-0-9 ].*/ ) {
	print STDERR "*** Error: External symbol '".$_.
	    "' referenced relatively ***\n";
	$bad++;
    } else {
	$output =~ s/^\w*[ \t]\w*PPC_ADDR\w*[ \t]*$_[+\- \t]*\w*[ \t]*$//mg;
	$output =~ s/^\w*[ \t]\w*PPC_REL\w*[ \t]*$_[+\- \t]*\w*[ \t]*$//mg;
    }
}

if( $bad ) {
    $output =~ s/\n\s*\n//g;
    print "\n";
    print $output;
    print "\n\n";
}

# At this point only local symbols are in $output.

if( !$bad && $output =~ /.*PPC.*/ ) {
    print STDERR "*** Error: Local assembly reloctations detected ***\n";
    $bad++;
    $output =~ s/\n\s*\n//g;
    print "\n";
    print $output;
    print "\n\n";
}

if( $bad ) {
    print STDERR "*** There were errors (perhaps in ".$ARGV[0].") ***\n";
}
exit $bad
