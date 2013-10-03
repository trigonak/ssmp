#!/usr/bin/awk -f

BEGIN {
    maxv=0;
    minv=1000000;
}

// {
    for (i = 0; i < NF; i++) {
	sum[i]+=$i;
	if ($i > maxv) {
	    maxv=$i;
	}
	if ($i < minv && $i > 0) {
	    minv=$i;
	}
    }
}

END {
    for (i = 0; i < NF; i++) {
	print i " : avg = " sum[i] / (NR-1)
	tsum += sum[i]
    }
    print "TOTAL avg: " tsum / (NR*(NR-1));
    print "MAX   val: " maxv;
    print "MIN   val: " minv;
    print "MAX/MIN  : " maxv/minv;

}