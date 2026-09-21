import os
import shutil
import subprocess

from setuptools import Distribution, setup
from setuptools.command.build_py import build_py

try:
    from setuptools.command.bdist_wheel import bdist_wheel
except ImportError:
    try:
        from wheel.bdist_wheel import bdist_wheel
    except ImportError:
        bdist_wheel = None

HERE = os.path.dirname(os.path.abspath(__file__))


def make_args():
    if "CC" in os.environ or shutil.which("gcc-14"):
        return ["make", "shared"]
    return ["make", "shared", "CC=gcc"]


def library():
    given = os.environ.get("JAOS_WHEEL_LIBRARY")
    if given:
        return os.path.abspath(given)
    subprocess.check_call(make_args(), cwd=HERE)
    return os.path.join(HERE, "build", "release", "libjaos.so")


class build_with_library(build_py):
    def run(self):
        src = library()
        shutil.copy(src, os.path.join(HERE, "python", "jaos",
                                      os.path.basename(src)))
        super().run()


class BinaryDistribution(Distribution):
    def has_ext_modules(self):
        return True


cmdclass = {"build_py": build_with_library}

if bdist_wheel is not None:
    class bdist_wheel_any_python(bdist_wheel):
        def get_tag(self):
            _, _, plat = super().get_tag()
            return "py3", "none", os.environ.get("JAOS_WHEEL_PLAT", plat)

    cmdclass["bdist_wheel"] = bdist_wheel_any_python

setup(cmdclass=cmdclass, distclass=BinaryDistribution)
