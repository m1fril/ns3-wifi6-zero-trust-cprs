set terminal postscript eps color enh "Times-BoldItalic"
set output "wifi-11a-p-1500-adhoc-r-OfdmRate54Mbps-min-10-max-10-step-5-throughput.eps"
set title "Frame size 1500 bytes"
set xlabel "Number of competing stations"
set ylabel "Throughput (Mbps)"
set xrange [10:10]
set xtics 5
set grid xtics ytics
set mytics
set style line 1 linewidth 5
set style line 2 linewidth 5
set style line 3 linewidth 5
set style line 4 linewidth 5
set style line 5 linewidth 5
set style line 6 linewidth 5
set style line 7 linewidth 5
set style line 8 linewidth 5
set style increment user
plot "-"  title "ns-3" with yerrorlines, "-"  title "Bianchi" with linespoints
10 38.3714 nan
e
10 28.1519
e
