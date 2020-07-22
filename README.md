## A Memory-Driven Mapping Algorithm for Heterogeneous Systems

**mpibind** is a memory-driven algorithm to map parallel hybrid
applications to the underlying hardware resources transparently,
efficiently, and portably. Unlike other mappings, its primary design point
is the memory system, including the cache hierarchy. Compute elements
are selected based on a memory mapping and not vice versa. In
addition, *mpibind* embodies a global awareness of hybrid programming
abstractions as well as heterogeneous devices such as accelerators.

### Getting Started


### Contributing

Contributions for bug fixes and new features are welcome and follow
a fork/pull model: Contributors develop on a branch of their
personal fork and create pull requests to merge their changes into the
main repository. 

### Authors

*mpibind* was created by Edgar A. León.

#### Citing *mpibind*

To reference *mpibind* in a publication, please cite one of the
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

*mpibind* is distributed under the terms of the MIT license. All new
contributions must be made under this license. 

See [LICENSE](LICENSE) and [NOTICE](NOTICE) for details.

SPDX-License-Identifier: MIT.

LLNL-CODE-812647.
