# the pair of 02-512: 02-511's tree against the cap, both with round lines
R=/mnt/c/Users/vall-/Desktop/projectes/jaos
M=$R/bench/measurements/02-512
P=$R/bench/measurements/02-501/pair.sh
A=/home/vall/j510/c3/build/cli/jaos
B=/home/vall/j510/c2f/build/cli/jaos
O=/home/vall/j510/pair2f
cat $M/heur.txt $M/heur2.txt > $M/list.txt
bash $P -a $A -b $B -n $M/list.txt -t 300 -j 6 -m 13 -o $O/heur
bash $P -a $A -b $B -n $M/list.txt -t 300 -j 2 -m 13 -o $O/heur
bash $P -a $A -b $B -n $M/solved20.txt -t 300 -j 6 -m 13 -o $O/solved
echo pair2f done
