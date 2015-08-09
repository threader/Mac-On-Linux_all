#!/usr/bin/perl -w
#
# A simple script used to compile relocation information needed
# to move critical low-level code to continuous physical 
# memory
#
# (C) 2002 Samuel Rydh <samuel@ibrium.se>
#

###########################################################
# XXX: THIS IS AN UGLY HACK.
#
# It works for now, but it might fail in the future if we 
# make use of more advanced addressing modes.
############################################################


$NM = 'nm';
$OTOOL = 'otool';
$GCC = 'cc';
$SYM_PREFIX = '_';

if( $#ARGV < 1 ) {
    print "Usage: symextract objectfile source\n";
    exit 1;
}

$output = `$OTOOL -rv $ARGV[0]` or die "relbuild.pl: otool failed (1)";
$code = `$OTOOL -Vt $ARGV[0]` or die "relbuild.pl: otool failed (2)";

$bad = 0;


######################################################################
# extract action symbol declarations from the specified source files
######################################################################

# find the symbols to export from the files specified on the cmdline 
$basecode = `$GCC -I./include -I../sinclude -E -DACTION_SYM_GREPPING $ARGV[1] 2> /dev/null` 
    or die "preprocessing failed";
@ASYMS = $basecode =~ /ACTION_SYM_START, *(\w*) *, *(\w*) *,ACTION_SYM_END.*/g;

# find exported global symbols
@GSYMS = $basecode =~ /GLOBAL_SYM_START, *(\w*) *,GLOBAL_SYM_END.*/g;

# make sure no action symbols are defined twice (a programming error)
for( $j=0 ; $j <= $#ASYMS ; $j+=2 ) {
    for( $i=$j+2 ; $i <= $#ASYMS ; $i+=2 ) {
	if( $ASYMS[$j] eq $ASYMS[$i] ) {
	    print STDERR "*** Error: Action symbol '".
		   $ASYMS[$j]."' declared twice! ***\n";
	    $bad++;
	}
    }
    # Add global symbol prefix...
    $ASYMS[$j]="$SYM_PREFIX$ASYMS[$j]";
}

# make sure no symbols are exported which are not declared
@esyms = `$NM $ARGV[0]` =~ / T (.*)/g;

for( $j=0 ; $j <= $#esyms ; $j++ ) {
    $match = 0;
    for( $i=0; $i <= $#GSYMS; $i++ ) {
	if( "$SYM_PREFIX$GSYMS[$i]" eq $esyms[$j] ) {
	    $match = 1;
	}
    }
    if( !$match ) {
	$bad++; 
	print STDERR "*** Error: Symbol `".$esyms[$j].
	    "` is not explicitely exported ***\n"; 
    }
}


######################################################################
# Lookup symbol offset by scanning the assembly source
######################################################################

# grep_offset( inst_offs, is_rel )
sub get_offset {
    my $offs = $_[0];
    my $is_rel = $_[1];
    my @match;

    @match = $code =~ /^$offs.*([+-]*0x[0-9,a-f]*).*$/mig;
    if( $#match < 0 ) {
	return 0;
    }

    $offs =~ s/[+]0x//;
    $match[0] =~ s/[+]0x//;

    if( $is_rel ) {
	$match[0] = sprintf("%x",hex($match[0]) - hex($offs))
    }
    return $match[0];
#    print STDERR "get_offs: @match\n";
}


#############################################################################
# Format of 'otool -rv' output:
#	address pcrel length extern type scattered symbolnum
#
#	address:	hexnum/or blank
#	pcrel:		True/False
#	legth:		long (always)
#	extern:		True/False/n/a
#	type:		BR24/BR14/ LO16/HA16/HI16/ PAIRSECTDIF/ VANILLA
#
# Example:
#
#address  pcrel length extern type    scattered symbolnum/value
#00000154 True  long   False  BR14    False     1 (__TEXT,__text)
#00000144 True  long   False  BR14    False     1 (__TEXT,__text)
#0000010c False long   n/a    SECTDIF True      0x0000012c
#         False long   n/a    PAIR    True      0x00000110
#00000108 False long   n/a    SECTDIF True      0x00000134
#         False long   n/a    PAIR    True      0x00000110
#00000100 False long   True   VANILLA False     Action_RELOC_HOOK
#000000fc True  long   False  BR24    False     1 (__TEXT,__text)
#000000f4 False long   True   VANILLA False     Action_VRET
#
##############################################################################

# Remove local BR24 and BR14 entries
$output =~ s/^.*False *BR[12]4 *.*$//mig;

@E = $output =~ /^(\w*) *(True|False) *(\w*) *(True|False)  *(\w*) *(\w*) *(.*)$/mig;
#print STDERR "$output\n";

print "\nstatic reloc_table_t reloc_table[] = {\n";

for($i=0; $i<$#E; $i+=7 ) {
    # Simplify notaion
    for($j=0; $j<7; $j+=1 ) { 
	$F[$j] = $E[$i+$j]; 
    }
    $is_pcrel = $F[1] =~ /True/i;

    # Perform some checks
    if( !($F[2] =~ /long/i) ) {
	die "relbuild.pl: 'long' expected (was $F[2])\n";
    }

    # Eliminate PAIR lines (they don't tell the full story anyway)
    if($i+7<$#E && ($E[$i+11] =~ /PAIR/i)) {
	splice @E,$i+7,7;
    }

    # PC-relative references to external symbols are forbidden
    if($is_pcrel && $F[3] =~ /True/i ) {
	#XXX: LATER ON, WE SHOULD REALLY EXIT HERE
	#die "ERROR: PC-relative reference to external symbol '$F[6]'\n";
	print STDERR "ERROR: PC-relative reference to external symbol '$F[6]'\n";
	next;
    }

    # Remove external symbols (but never remove action symbols)
    if($F[3] =~ /True/i && !($F[6] =~ /Action_.*|Symbol_.*/) ){
	next;
    }

    # Determine any the symbol offset
    $F[2] = get_offset( $F[0], $is_pcrel );

    # Remove 0x prefix
    $F[0] =~ s/0x//;
    $F[2] =~ s/0x//;

    # Translate MACH_O relocation names into ELF ones
    $rel_name="xxxxxxx";
    for($F[4]) {
	$rel_name =
		/VANILLA/	&& "MOL_R_PPC_ADDR32" 
	    ||	/LO16/		&& "MOL_R_PPC_ADDR16_LO"
	    ||	/HA16/		&& "MOL_R_PPC_ADDR16_HA"
	    ||	/HI16/		&& "MOL_R_PPC_ADDR16_HI"
	    ||  die("Unsupported symbol type $_\n");
    }

    # Translate action symbols into numbers
    # (Local symbols get action number 0)
    $ACTION = 0;
    if($F[6] =~ /Symbol_*|Action_*/) {
	$ACTION = "BAD";
	for( $j=0; $j <= $#ASYMS; $j+=2 ) {
	    if( $ASYMS[$j] eq $F[6] ) {
		$ACTION = $ASYMS[$j+1];
	    }
	}
        $ACTION eq "BAD" and die "Undeclared action symbol '$F[6]'\n";
    }

    # We are done!
    printf("  { 0x%04x, $ACTION, %s,\t0x%04x },\t/* %s */\n", 
	   hex($F[0]), $rel_name, hex($F[2]), $F[6] );
}
print "   { 0, -1, 0, 0 }    /* End marker */\n};\n";

exit 0;

