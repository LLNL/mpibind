from distutils.core import setup

# https://packaging.python.org/guides/distributing-packages-using-setuptools/#setup-py

longtext = """Python bindings for mpibind, a memory-driven algorithm to map parallel hybrid
              applications to the underlying hardware resources transparently,
              efficiently, and portably."""

setup(
  name='mpibind',
  version='0.5.0',
  author='LLNL',
  author_email='leonborja1@llnl.gov',
  url='https://github.com/LLNL/mpibind',
  description='Memory-First Affinity Scheduler',
  long_description=longtext,
  keywords='affinity, NUMA, hybrid applications, heterogeneous systems',
  py_modules=['mpibind'],
#  install_requires=['cffi'],
  platforms=['posix'],
  license='MIT',
  classifiers=[
    'Development Status :: 4 - Beta',
    'License :: OSI Approved :: MIT License',
    'Operating System :: POSIX :: Linux',
    'Programming Language :: Python :: 3',
  ],
)