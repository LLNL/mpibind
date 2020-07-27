# Mpibind Tests


The current iteration of the test suite is designed to generate a set of tests based on a given topology, then compare the resultant mappings to a file that defines expected output. Generating the tests involving gathering basic information about a topology, and using that information to tweak each test to be suitable for the topology.

An example of the answers file is below:

```

# Line that start with a pound are comments!
# The first non-commented line should be the number of tests.
3

# 1: Map one task to every core
Map one task to every core
"1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1"
"6,54;6,54;7,55;7,55;8,56;8,56;9,57;9,57;10,58;10,58;11,59;11,59;12,60;12,60;13,61;13,61;14,62;14,62;15,63;15,63;16,64;16,64;17,65;17,65;30,78;30,78;31,79;31,79;32,80;32,80;33,81;33,81;34,82;34,82;35,83;35,83;42,90;42,90;43,91;43,91;44,92;44,92;45,93;45,93;46,94;46,94;47,95;47,95"
"0;0;0;0;0;0;0;0;0;0;0;0;1;1;1;1;1;1;1;1;1;1;1;1;2;2;2;2;2;2;2;2;2;2;2;2;3;3;3;3;3;3;3;3;3;3;3;3"

# 2: Map 1 task greedily
Map 1 task greedily
"96"
"0-95"
"0-3"

# 3: Map two tasks greedily
Map two tasks greedily
"48;48"
"0-23,48-71;24-47,72-95"
"0-1;2-3"

```

The first non-blank, non-commented line in the answer file is the number of tests in the file. After that, each answer consists of four line: a description, the expected thread mapping, the expected cpu mapping, and the expected gpu_mapping for the tasks of a given mpibind run. Each of the 4 lines are wrapped in quotes. These strings are directly compared to the output of mpibind when given the corresponding input parameters.

## Test Details

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
2. Error checking
    * Passing NULL in place of the handle to all of the setter and getter functions.
    * Trying to run mpibind with an invalid number of threads (e.g. -1)
    * Trying to run mpibind with an invalid number of tasks (e.g. -1)
    * Trying to run mpibind with an invalid SMT level (e.g. -1 or 8 on a machine with SMT-4
3. Environment Varibles
    * Check that AMD and NVIDIA gpus can be properly detected
    * Check that the OMP_PLACES variable is formatted correctly