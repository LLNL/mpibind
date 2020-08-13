from setuptools import setup
import os

here = os.path.abspath(os.path.dirname(__file__))
cffi_dep = "cffi>=1.0.0"
setup(
    name="mpibind",
    version="0.0.1",
    description="Python bindings for mpibind",
    setup_requires=[cffi_dep],
    cffi_modules=["_mpibind/build.py:ffibuilder"],
    install_requires=[cffi_dep],
    url="https://github.com/LLNL/mpibind",
    author="",
    author_email="",
    license="",
    package_dir={
        "": here,
    },
    packages=["_mpibind", "mpibind"]
)
