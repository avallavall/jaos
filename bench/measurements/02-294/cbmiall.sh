#!/bin/bash
S=$(cd "$(dirname "$0")" && pwd)
DIR=${DIR:-$HOME/cblib-mip}
arm=$1; lim=${2:-10000000000}
ls -S $DIR/*.cbf.gz | xargs -P 3 -I{} bash $S/cbmi1.sh {} $arm $lim | sort > $S/cbmi-$arm.txt
echo "$arm done: $(wc -l < $S/cbmi-$arm.txt) lines"
