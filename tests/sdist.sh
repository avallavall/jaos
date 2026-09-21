#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/.." || exit 1

ROOT=$(mktemp -d) || exit 1
trap 'rm -rf "$ROOT"' EXIT

fail=0
pass() { echo "ok   $1"; }
flunk() { echo "FAIL $1"; fail=1; }
run() { "$@" > "$ROOT/log" 2>&1; }
show() { sed 's/^/     /' "$ROOT/log"; }

want=$(sed -n 's/^#define JAOS_VERSION_STRING "\(.*\)"$/\1/p' include/jaos.h)

if run python3 -m venv "$ROOT/tools" && run "$ROOT/tools/bin/pip" install build; then
    pass "a venv with build"
else
    flunk "cannot make a venv with build"; show; exit 1
fi

if run "$ROOT/tools/bin/python" -m build --sdist --wheel --outdir "$ROOT/dist" .; then
    pass "python -m build writes the sdist and the wheel"
else
    flunk "python -m build failed"; show; exit 1
fi

sdist=$(ls "$ROOT"/dist/jaos-*.tar.gz 2>/dev/null | head -1)
wheel=$(ls "$ROOT"/dist/jaos-*.whl 2>/dev/null | head -1)
[ -n "$sdist" ] && pass "sdist $(basename "$sdist")" || flunk "no sdist"
[ -n "$wheel" ] && pass "wheel $(basename "$wheel")" || flunk "no wheel"

case "$(basename "$wheel")" in
    *-py3-none-linux_*.whl|*-py3-none-manylinux*.whl) pass "the wheel is tagged for its platform and any Python 3" ;;
    *) flunk "the wheel's tag is $(basename "$wheel")" ;;
esac

for f in src/model.c include/jaos.h Makefile; do
    if tar -tzf "$sdist" | grep -q "/$f\$"; then
        pass "the sdist carries $f"
    else
        flunk "the sdist does not carry $f"
    fi
done

for kind in sdist wheel; do
    file=$sdist
    [ $kind = wheel ] && file=$wheel
    if run python3 -m venv "$ROOT/clean-$kind" \
        && run "$ROOT/clean-$kind/bin/pip" install "$file"; then
        pass "pip installs the $kind into a clean venv"
    else
        flunk "pip could not install the $kind"; show; continue
    fi
    got=$(cd "$ROOT" && env -u JAOS_LIBRARY "$ROOT/clean-$kind/bin/python" \
        -c "import jaos; print(jaos.version())" 2>&1)
    if [ "$got" = "$want" ]; then
        pass "the installed $kind reports version $want"
    else
        flunk "the installed $kind reports '$got', want '$want'"
    fi
done

exit $fail
