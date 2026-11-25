# sweep.sh variant... — in each ~/j36/<variant> tree: netlib, netlib-infeas (J=4), kennington (J=2) against the committed baselines
R=/mnt/c/Users/vall-/Desktop/projectes/jaos
one() {
  v=$1; H=~/j36/$v
  for d in instances instances-infeas instances-kennington; do ln -sfn $R/bench/$d $H/bench/$d; done
  cd $H
  make netlib J=4 > $H/sw-netlib.log 2>&1; echo "$v netlib exit=$?"
  make netlib-infeas J=4 > $H/sw-inf.log 2>&1; echo "$v infeas exit=$?"
  make netlib-kennington J=2 > $H/sw-kenn.log 2>&1; echo "$v kennington exit=$?"
  grep -hE "^baseline|^gate" $H/sw-netlib.log $H/sw-inf.log $H/sw-kenn.log | sed "s/^/$v /"
}
export -f one; export R
printf "%s\n" "$@" | xargs -n 1 -P 3 bash -c 'one "$0"'
echo SWEEPDONE
