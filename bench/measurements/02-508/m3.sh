# m3.sh SET ARM... — builds each arm's tree under ~/j508/t-ARM from the
# working tree (A as it is; A+V with variants.py's V on top) and runs SET
# (miplib or miplib2017) on it with bench/run, J at once (2 by default),
# against the set's baseline when it has one; records in ~/j508/SET-ARM.txt
R=/mnt/c/Users/vall-/Desktop/projectes/jaos
M=$R/bench/measurements/02-508
set=$1; shift
J=${J:-2}
for a in "$@"; do
  T=~/j508/t-$a
  if [ ! -x $T/build/bench/run ]; then
    rm -rf $T; mkdir -p $T/bench
    rsync -a $R/src $R/include $R/cli $R/Makefile $T/
    cp $R/bench/*.c $T/bench/
    case $a in A+*) python3 $M/variants.py $T ${a#A+} || exit 1 ;; esac
    (cd $T && make -s build/cli/jaos build/bench/run > build.log 2>&1) || { echo "$a failed"; exit 1; }
  fi
done
for a in "$@"; do
  T=~/j508/t-$a
  case $set in
    miplib) (cd $R && $T/build/bench/run -j $J -m bench/miplib.manifest -e mip -d bench/instances-miplib -b bench/miplib.baseline -o ~/j508/$set-$a.txt > ~/j508/$set-$a.log 2>&1) ;;
    miplib2017) (cd $R && $T/build/bench/run -j $J -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 -L 10000000000 -o ~/j508/$set-$a.txt > ~/j508/$set-$a.log 2>&1) ;;
  esac
  echo "$a: $(grep '^baseline\|^gate' ~/j508/$set-$a.log | tr '\n' ' ')"
done
