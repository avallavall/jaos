import os
import shutil
import subprocess

from setuptools import setup
from setuptools.command.build_py import build_py

HERE = os.path.dirname(os.path.abspath(__file__))


class build_with_library(build_py):
    def run(self):
        subprocess.check_call(["make", "shared"], cwd=HERE)
        shutil.copy(os.path.join(HERE, "build", "release", "libjaos.so"),
                    os.path.join(HERE, "python", "jaos", "libjaos.so"))
        super().run()


setup(cmdclass={"build_py": build_with_library})
