# the pair of 02-510: HEAD (with round lines) against the working tree (with round lines), root96 split by memory
R=/mnt/c/Users/vall-/Desktop/projectes/jaos
M=$R/bench/measurements/02-510
P=$R/bench/measurements/02-501/pair.sh
A=/home/vall/j510/rounds/build/cli/jaos
B=/home/vall/j510/wt/build/cli/jaos
O=/home/vall/j510/pair1
bash $P -a $A -b $B -n $M/heavy-b.txt -t 300 -j 2 -m 14 -o $O/b
bash $P -a $A -b $B -n $M/heavy-a.txt -t 300 -j 3 -m 13 -o $O/a
bash $P -a $A -b $B -n $M/rest.txt -t 300 -j 6 -m 13 -o $O/rest
bash $P -a $A -b $B -n $M/rest.txt -t 300 -j 2 -m 13 -o $O/rest
echo pair1 done
bash $P -a $A -b $B -n $M/solved20.txt -t 300 -j 2 -m 13 -o $O/solved
bash $P -a $A -b $B -n $M/heavy-b.txt -t 300 -j 1 -m 13 -o $O/b
