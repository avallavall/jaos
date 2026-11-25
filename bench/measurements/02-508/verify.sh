# verify.sh — 02-508's readings of the change at 300 s: the tree of HEAD
# (~/j508/head, built by trees.sh) against the working tree frozen in
# ~/j508/t-cand, by 02-501's pair.sh on the 34 of cut34.txt (the four
# largest by memory two at a time), and MIPLIB 2017 at 1e10 work units on
# the working tree's build; records under ~/j508/pair34 and ~/j508
R=/mnt/c/Users/vall-/Desktop/projectes/jaos
M=$R/bench/measurements/02-508
T=~/j508/t-cand
rm -rf $T; mkdir -p $T/bench
rsync -a $R/src $R/include $R/cli $R/Makefile $T/
cp $R/bench/*.c $T/bench/
(cd $T && make -s build/cli/jaos build/bench/run > build.log 2>&1) || { echo "cand failed"; exit 1; }
heavy="k1mushroom academictimetablesmall square41 neos-933966"
grep -v '^#' $M/cut34.txt | grep -vxF -f <(printf '%s\n' $heavy) > ~/j508/light30.txt
printf '%s\n' $heavy > ~/j508/heavy4.txt
(cd $R && $T/build/bench/run -j 2 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 -L 10000000000 -o ~/j508/miplib2017-cand.txt > ~/j508/miplib2017-cand.log 2>&1) &
bash $R/bench/measurements/02-501/pair.sh -a ~/j508/head/build/cli/jaos -b $T/build/cli/jaos -n ~/j508/light30.txt -t 300 -j 4 -m 14 -o ~/j508/pair34
cp ~/j508/pair34/record.txt ~/j508/pair34/record-light.txt
bash $R/bench/measurements/02-501/pair.sh -a ~/j508/head/build/cli/jaos -b $T/build/cli/jaos -n ~/j508/heavy4.txt -t 300 -j 2 -m 15 -o ~/j508/pair34
cp ~/j508/pair34/record.txt ~/j508/pair34/record-heavy.txt
wait
echo verify done
