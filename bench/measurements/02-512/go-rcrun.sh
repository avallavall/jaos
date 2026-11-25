# go-rcrun.sh TAG RCAP1 RCAP inst:work... — the rcap tree under a fixed work limit (time limit 1200 s as a guard)
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
T=$1; export RCAP1=$2 RCAP=$3; shift 3
for a in "$@"; do n=${a%%:*}; w=${a##*:}
 ( ulimit -v 4000000; exec ~/j510/rcap/build/cli/jaos solve $I/$n.mps --threads 1 --log summary --time-limit 1200 --work-limit $w ) > ~/j510/logs/$n.rc$T 2>&1 &
done; wait
for a in "$@"; do n=${a%%:*}; f=~/j510/logs/$n.rc$T; printf "%-22s %s " $n $T; grep -E '^(status|nodes|bound|incumbent)' $f | tr '\n' ' '; grep -c 'rcap: round' $f | tr '\n' ' '; grep '^round [0-9]* cuts' $f | wc -l; done
