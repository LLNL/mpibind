# mpibind tests

The current iteration of the test suite is designed to generate a set of tests
based on a given topology, then compare the resultant mappings to a file that
defines expected output (see `expected` directory). Generating the tests
involves gathering basic information about a topology, and using that
information to tweak each test to be suitable for the topology.

An example of the answers file is below:

```
# Line that start with a pound are comments!
# Each answer description consist of 6 lines:
# 1. A comment with the test number
# 2. A comment describing the parameters used for the test in JSON format
# 3. The test description
# 4. the thread mapping
# 5. the cpu mapping
# 6. the gpu_mapping
# The mapping for each task is separated by a defined character.
# This separator can be changed in test_utils.c::parse_answer()

# 1:
# {"params": {"ntasks": 40, "in_nthreads": 0, "greedy": 1, "gpu_optim": 1, "smt": 0, "restr_set": null, "restrict_type": 0}}
Map one task to every core
"1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1"
"8;12;16;20;24;28;32;36;40;44;48;52;56;60;64;68;72;76;80;84;96;100;104;108;112;116;120;124;128;132;136;140;144;148;152;156;160;164;168;172"
"0;0;0;0;0;0;0;0;0;0;1;1;1;1;1;1;1;1;1;1;2;2;2;2;2;2;2;2;2;2;3;3;3;3;3;3;3;3;3;3"

```

The Python tests parse the params comment to initialize mpibind handles correctly.

## Test details

1. Valid mpibind configurations
    * Map one task to every core
    * Map one task greedily
    * Map two tasks greedily
    * Mapping such that ntasks < #NUMA nodes but nworkers > #NUMA nodes (this makes sure mpibind accounts * for the number of threads as well
    * Restrict x tasks a single core (x == machine's smt level)
    * Map two tasks at smt 1
    * Map two tasks at smt (max_smt - 1)
    * Map two tasks, but restrict them to a single NUMA domain
    * Map number_numas tasks without GPU optimization
    * Map number_numas tasks with GPU optimization
    * Map 8 tasks to a single PU
2. Error checking
    * Passing NULL in place of the handle to all of the setter and getter functions.
    * Trying to run mpibind with an invalid number of threads (e.g. -1)
    * Trying to run mpibind with an invalid number of tasks (e.g. -1)
    * Trying to run mpibind with an invalid SMT level (e.g. -1 or 8 on a machine with SMT-4
3. Environment Varibles
    * Check that AMD and NVIDIA gpus can be properly detected
    * Check that the OMP_PLACES variable is formatted correctly

## Debugging 

The tests are fired off by running `make check` from the top directory. One can
use `V=1` to show the verbose compilation lines and `VERBOSE=1` to show any
libtap error(s). 
```
make V=1 VERBOSE=1 check 
```

The <test>.t files are libtool scripts that call the `.libs/<test>.h` binaries. 

The expected mappings are in the `expected` directory. 