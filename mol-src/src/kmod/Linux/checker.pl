#
# Unfortunately, the namespace barrier between assembly files and the C-soruce
# is somewhat borken. This happens for instance, if a record is added to the
# symbol table (.long <symname> causes this for instance). This script makes sure
# we don't use local symbols globaly.
#
# Environmental variable NM must set by caller

$bad = 0;

@msyms = `$NM $ARGV[0]` =~ / U (.*)/g or $bad++;
@lsyms = `$NM $ARGV[0]` =~ / [td] (.*)/g or $bad++;

for( $i=0; $i <= $#msyms ; $i++ ) {
    for( $j=0; $j <= $#lsyms ; $j++ ) {
	if( $msyms[$i] eq $lsyms[$j] ) {
	    print "Error: *** Local symbol '".$lsyms[$j]."' appears globally! ***\n";
	    $bad++;
	}
    }
}

exit $bad;


