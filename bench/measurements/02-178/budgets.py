#!/usr/bin/env python3
"""D273. The two budgets an exact verifier has to meet, joined from the two
files that own them.

A verifier that eliminates a block of k rows holds k*k numbers at once and
forms about k**3/3 products of them. So there are two limits, not one:

  1. the WIDTH of a number, which is the Hadamard bound of the block once it
     is made integral. bench/measurements/02-178/integral.txt owns it.
  2. the SIZE of the block, k, because the memory is k*k numbers of that
     width and the time is k**3/3 multiplications of them.
     bench/measurements/02-177/blocks.txt owns k.

Neither file can answer on its own and neither number is restated here: this
joins them and prints the product. Nothing is measured in this script.

  python3 bench/measurements/02-178/budgets.py
"""
import sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
BLOCKS = os.path.join(ROOT, "bench", "measurements", "02-177", "blocks.txt")
INTEGRAL = os.path.join(HERE, "integral.txt")

LIMBS = 128                 # JM_EXACT_LIMBS, src/jaos_internal.h
CAP_BITS = 32 * LIMBS


def read_blocks(path):
    """instance -> (rows, largest block)"""
    out = {}
    for line in open(path):
        f = line.split()
        if len(f) != 9 or not f[0].endswith(".mps"):
            continue
        out[f[0]] = (int(f[1]), int(f[4]))
    return out


def read_integral(path):
    """instance -> (rawblock bits, intblock bits, rows needing a scale)"""
    out = {}
    for line in open(path):
        f = line.split()
        if len(f) != 10 or not f[0].endswith(".mps"):
            continue
        out[f[0]] = (float(f[3]), float(f[5]), int(f[7]))
    return out


def main():
    for p in (BLOCKS, INTEGRAL):
        if not os.path.exists(p):
            sys.exit("missing %s" % p)
    blocks = read_blocks(BLOCKS)
    integral = read_integral(INTEGRAL)

    shared = sorted(set(blocks) & set(integral))
    print("# the two budgets, joined from 02-177/blocks.txt and "
          "02-178/integral.txt")
    print("# k:      the largest block, from blocks.txt")
    print("# bits:   that block's Hadamard bound once integral, from "
          "integral.txt")
    print("# limbs:  ceil(bits / 32), what one number costs")
    print("# mem:    k*k numbers of that width, dense, in MiB")
    print("# mults:  about k**3/3 products of them")
    print("# width:  does bits fit the 4096 the file has")
    print("# both:   width, and the block small enough to hold and to run")
    print()
    print("%-14s %8s %10s %7s %12s %12s %6s %5s" %
          ("instance", "k", "bits", "limbs", "mem_MiB", "mults", "width",
           "both"))

    fit_width = fit_both = 0
    MEM_CEILING_MIB = 1024.0        # what one call may hold
    MULT_CEILING = 1e10             # products, a stand-in for "a few minutes"

    rows = []
    for name in shared:
        _, k = blocks[name]
        _, bits, _ = integral[name]
        limbs = max(1, int((bits + 31) // 32))
        per = limbs * 4 + 8
        mem = (k * k * per) / (1024.0 * 1024.0)
        mults = (k ** 3) / 3.0
        w = bits <= CAP_BITS
        b = w and mem <= MEM_CEILING_MIB and mults <= MULT_CEILING
        fit_width += w
        fit_both += b
        rows.append((name, k, bits, limbs, mem, mults, w, b))

    for name, k, bits, limbs, mem, mults, w, b in rows:
        print("%-14s %8d %10.1f %7d %12.1f %12.3g %6s %5s" %
              (name, k, bits, limbs, mem, mults,
               "yes" if w else "NO", "yes" if b else "NO"))

    print()
    print("%d instances joined" % len(rows))
    print("width alone says yes on %d" % fit_width)
    print("width and block size together say yes on %d "
          "(memory ceiling %.0f MiB, %.0e products)"
          % (fit_both, MEM_CEILING_MIB, MULT_CEILING))


if __name__ == "__main__":
    main()
