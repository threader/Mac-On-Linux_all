#!/usr/bin/perl -w
#
# A simple perl script to extract the version number
# from include/version.

#$a=`cat include/version.h`;
#$c=`cat patchlevel.h`;

$a = `cat $ARGV[0]`;
if( $#ARGV == 1 ) {
    $c = `cat $ARGV[1]`;
}

@A = $a =~ /#define[\t ][\t ]*MAJOR_VERSION[\t ][\t ]*(\w*)/g;
@B = $a =~ /#define[\t ][\t ]*MINOR_VERSION[\t ][\t ]*(\w*)/g;
if( $#ARGV == 1 ) {
    @C = $c =~ /#define[\t ][\t ]*PATCH_LEVEL[\t ][\t ]*(\w*)/g;
}

if( $#ARGV == 1 ) {
    print $A[0].".".$B[0].".".$C[0]."\n";
} else {
    print $A[0].".".$B[0]."\n";
}

