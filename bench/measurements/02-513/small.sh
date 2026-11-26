K=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-kennington
D=~/j513/small; mkdir -p $D
for ROUND in 1 2 3 4 5 6 7 8 9 10; do
  for k in cre-b cre-d ken-13 pds-10 cre-a ken-11; do
    for v in base pvb; do ~/j513/$v/build/cli/jaos solve $K/$k.mps --threads 1 > $D/$k.$v.$ROUND 2>&1 & done; wait
    printf '%-8s %s %s %s %s\n' $k $(grep -m1 '^time' $D/$k.base.$ROUND | cut -d' ' -f2) $(grep -m1 '^time' $D/$k.pvb.$ROUND | cut -d' ' -f2) $(grep -m1 '^iterations' $D/$k.base.$ROUND | cut -d' ' -f2) $(grep -m1 '^iterations' $D/$k.pvb.$ROUND | cut -d' ' -f2)
  done
done
