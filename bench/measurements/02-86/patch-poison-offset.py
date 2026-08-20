"""Poisons the reduced model's objective offset in a COPY of the tree.

`TODO.md` carries presolve's `obj_offset` as a naive accumulation and says
nothing has read it for the answer since D169. That is a claim from reading
the code, and this repository's own rule is that a claim from reading is not a
measurement. So the value is replaced with something obviously wrong and the
three sets are run: if nothing moves, nothing reads it.

Two poisons, because they fail differently. A large finite value survives
every `isfinite` guard and would show up in any consumer as a wrong number.
A NaN would show up as a NaN and also exercises the guards. A value that is
truly dead is invisible under both.

Applies to the tree named on the command line, never to the repository.
"""
import sys

d, poison = sys.argv[1], sys.argv[2]
p = d + "/src/presolve.c"
s = open(p, encoding="utf-8").read()

ANCHOR = """    const double accumulated_offset = p->reduced.obj_offset;
    p->reduced = *m;
    p->reduced.obj_offset = m->obj_offset + accumulated_offset;"""

assert s.count(ANCHOR) == 1, s.count(ANCHOR)

VALUE = {"big": "1e300", "nan": "(0.0 / 0.0)"}[poison]

s = s.replace(ANCHOR, ANCHOR + f"""
#ifdef JAOS_DIAG
    /* Whatever the rounds accumulated, thrown away. If the three sets are
     * bit-identical under this, the reduced model's offset reaches no
     * decision and no published number. */
    p->reduced.obj_offset = {VALUE};
#endif""")

open(p, "w", encoding="utf-8").write(s)
print(f"poisoned {p} with {poison} ({VALUE})")
