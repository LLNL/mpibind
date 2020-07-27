from setuptools import setup
import os

here = os.path.abspath(os.path.dirname(__file__))
cffi_dep = "cffi>=1.1"
setup(
    name="mpibind",
    version="0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.1a1",
    description="Python bindings for mpibind",
    setup_requires=[cffi_dep],
    cffi_modules=["cffi_build/mpibind_build.py:ffi"],
    install_requires=[cffi_dep],
)
