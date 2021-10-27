## A Memory-Driven Mapping Algorithm for Heterogeneous Systems

`mpibind` is a memory-driven algorithm to map parallel hybrid
applications to the underlying hardware resources transparently,
efficiently, and portably. Unlike other mappings, its primary design point
is the memory system, including the cache hierarchy. Compute elements
are selected based on a memory mapping and not vice versa. In
addition, mpibind embodies a global awareness of hybrid programming
abstractions as well as heterogeneous systems with accelerators.

### Getting started 

The easiest way to get `mpibind` is using
[spack](https://github.com/spack/spack).  

```
spack install mpibind

# On systems with NVIDIA GPUs
spack install mpibind+cuda

# On systems with AMD GPUs
spack install mpibind+rocm

# More details
spack info mpibind
```

Alternatively, one can build the package manually as described below. 

### Building and installing 

This project uses GNU Autotools.

```
$ ./bootstrap

$ ./configure --prefix=<install_dir>

$ make

$ make install
```

The resulting library is `<install_dir>/lib/libmpibind` and a simple program using it is `src/main.c`


### Test suite 

```
$ make check
```

### Dependencies 

* `GNU Autotools` is the build system. 

* `hwloc` version 2 is required to detect the machine topology.

  Before building mpibind, make sure `hwloc` can be detected with `pkg-config`:
  ```
  pkg-config --variable=libdir --modversion hwloc
  ```
  If this fails, add hwloc's pkg-config directory to `PKG_CONFIG_PATH`, e.g.,
  ```
  export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:<hwloc-prefix>/lib/pkgconfig
  ```

* `libtap` is required to build the test suite.

  To verify `tap` can be detected with `pkg-config`, follow a
  similar procedure as for `hwloc` above. 


### Contributing

Contributions for bug fixes and new features are welcome and follow
the GitHub
[fork and pull model](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/about-collaborative-development-models).
Contributors develop on a branch of their personal fork and create
pull requests to merge their changes into the main repository. 

The steps are similar to those of the Flux framework:

1. [Fork](https://help.github.com/en/github/getting-started-with-github/fork-a-repo) `mpibind`.
2. [Clone](https://help.github.com/en/github/getting-started-with-github/fork-a-repo#keep-your-fork-synced)
your fork: `git clone git@github.com:[username]/mpibind.git`
3. Create a topic branch for your changes: `git checkout -b new_feature`
4. Create feature or add fix (and add tests if possible)
5. Make sure everything still passes: `make check`
6. Push the branch to your GitHub repo: `git push origin new_feature`
7. Create a pull request against `mpibind` and describe what your
changes do and why you think it should be merged. List any
outstanding *todo* items. 


### Authors

`mpibind` was created by Edgar A. León.

### Citing mpibind

To reference mpibind, please cite one of the
following papers:

* Edgar A. León and Matthieu Hautreux. *Achieving Transparency Mapping
  Parallel Applications: A Memory Hierarchy Affair*. In International
  Symposium on Memory Systems, MEMSYS'18, Washington, DC,
  October 2018. ACM. 

* Edgar A. León. *mpibind: A Memory-Centric Affinity Algorithm for
  Hybrid Applications*. In International Symposium on Memory Systems,
  MEMSYS'17, Washington, DC, October 2017. ACM.

* Edgar A. León, Ian Karlin, and Adam T. Moody. *System Noise
  Revisited: Enabling Application Scalability and Reproducibility with
  SMT*. In International Parallel & Distributed Processing Symposium,
  IPDPS'16, Chicago, IL, May 2016. IEEE.
  
Other references: 

* J. P. Dahm, D. F. Richards, A. Black, A. D. Bertsch, L. Grinberg, I. Karlin, S. Kokkila-Schumacher, E. A. León, R. Neely, R. Pankajakshan, and O. Pearce. *Sierra Center of Excellence: Lessons learned*. In IBM Journal of Research and Development, vol. 64, no. 3/4, May-July 2020.

* Edgar A. León. *Cross-Architecture Affinity of Supercomputers*. In International Supercomputing Conference (Research Poster), ISC’19, Frankfurt, Germany, June 2019. 

* Edgar A. León. *Mapping MPI+X Applications to Multi-GPU
  Architectures: A Performance-Portable Approach*. In GPU Technology
  Conference, GTC'18, San Jose, CA, March 2018. 
  

[Bibtex file](doc/mpibind.bib). 


### License

`mpibind` is distributed under the terms of the MIT license. All new
contributions must be made under this license. 

See [LICENSE](LICENSE) and [NOTICE](NOTICE) for details.

SPDX-License-Identifier: MIT.

LLNL-CODE-812647.
