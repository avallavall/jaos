import sys

from . import JaosError, Model, SolveStatus, version

EXIT = {
    SolveStatus.OPTIMAL: 0,
    SolveStatus.INFEASIBLE: 1,
    SolveStatus.UNBOUNDED: 2,
}
USAGE = """usage: python -m jaos solve FILE [--opt NAME=VALUE]... [--params FILE]
       python -m jaos version
FILE is MPS or LP, plain or .gz. Prints one fact per line; the exit code is
the verdict: 0 optimal, 1 infeasible, 2 unbounded, 3 stopped by a limit,
4 numerical failure, 5 usage."""


def _fail(msg):
    sys.stderr.write("jaos: %s\n" % msg)
    return 5


def main(argv=None):
    argv = sys.argv[1:] if argv is None else list(argv)
    if not argv or argv[0] in ("-h", "--help", "help"):
        print(USAGE)
        return 0 if argv else 5
    if argv[0] == "version":
        print(version())
        return 0
    if argv[0] != "solve":
        return _fail("unknown command '%s'" % argv[0])
    args = argv[1:]
    path = None
    opts = []
    params = None
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--opt":
            if i + 1 >= len(args) or "=" not in args[i + 1]:
                return _fail("--opt takes NAME=VALUE")
            opts.append(args[i + 1].split("=", 1))
            i += 2
        elif a == "--params":
            if i + 1 >= len(args):
                return _fail("--params takes a file")
            params = args[i + 1]
            i += 2
        elif a.startswith("-"):
            return _fail("unknown option '%s'" % a)
        elif path is None:
            path = a
            i += 1
        else:
            return _fail("solve takes one file")
    if path is None:
        return _fail("solve needs a file")
    m = Model()
    try:
        lower = path.lower()
        if lower.endswith(".lp") or lower.endswith(".lp.gz"):
            m.read_lp(path)
        else:
            m.read_mps(path)
        if params is not None:
            m.read_options(params)
        for name, value in opts:
            m.set_option(name, value)
        status = m.solve()
    except JaosError as e:
        return _fail(str(e))
    print("status %s" % status.name.lower())
    if status is SolveStatus.OPTIMAL:
        print("objective %.17g" % m.objective())
    print("iterations %d" % m.iterations)
    print("work %d" % m.work_units)
    if status in EXIT:
        return EXIT[status]
    return 4 if status is SolveStatus.NUMERICAL_ERROR else 3


if __name__ == "__main__":
    sys.exit(main())
