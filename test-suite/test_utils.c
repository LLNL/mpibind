#include "test_utils.h"
#define TEST_DEBUG 0
#define BUF_SIZE 1024

#if 0
/**
 * Prints mpibind_t imput parameters
 **/
static void print_test_params(mpibind_t *handle) {
  printf(
      "tasks: %d\t threads: %d\n greedy: %d\n gpu_optim: %d\n smt: %d\n "
      "restr_type: %d\t restrict_ids %s",
      handle->ntasks, handle->in_nthreads, handle->greedy, handle->gpu_optim,
      handle->smt, handle->restr_type, handle->restr_set);
}
#endif

/*
 * mpibind relies on Core objects. If the topology
 * doesn't have them, use an appropriate replacement.
 * Make sure to always use get_core_type and get_core_depth
 * instead of HWLOC_OBJ_CORE and its depth.
 * Todo: In the future, I may need to have similar functions
 * for NUMA domains.
 */
static int get_core_depth(hwloc_topology_t topo) {
  return hwloc_get_type_or_below_depth(topo, HWLOC_OBJ_CORE);
}

/**
 * Initialize a test struct to default values.
 * This mimics the behavior of mpibind_init
 * **/
int mpibind_test_t_init(mpibind_test_t *hdl) {
  if (hdl == NULL) {
    return 1;
  }

  hdl->ntasks = 0;
  hdl->in_nthreads = 0;
  hdl->greedy = 1;
  hdl->gpu_optim = 1;
  hdl->smt = 0;
  hdl->restr_set = NULL;
  hdl->restr_type = MPIBIND_RESTRICT_CPU;
  hdl->topo = NULL;
  hdl->expected = NULL;

  return 0;
}

/**
 * Prints the current state of an mpibind_test_t object
 * **/
void mpibind_test_t_print(mpibind_test_t *params) {
  printf("ntasks: %d\t in_nthreads: %d\n", params->ntasks, params->in_nthreads);
  printf("greedy: %d\n", params->greedy);
  printf("gpu_optim: %d\n", params->gpu_optim);
  printf("smt: %d\n", params->smt);
  printf("restr_set: %s\t restrict_type: %d\n", params->restr_set,
         params->restr_type);
  printf("\n");
}

/**
 * Frees an answer
 * **/
void mpibind_test_ans_t_free(mpibind_test_ans_t *t) {
  if (!t) {
    perror("mpibind_test_ans_t == NULL");
  }

  free(t->description);
  free(t->thread_mapping);
  free(t->gpu_mapping);
  free(t->cpu_mapping);
  free(t);
}

/**
 * Frees a test object
 * **/
void mpibind_test_t_free(mpibind_test_t *t) {
  if (!t) {
    perror("mpibind_test_t == NULL");
  }

  if (t->restr_set) {
    free(t->restr_set);
  }
  if (t->expected) {
    mpibind_test_ans_t_free(t->expected);
    free(t->expected);
  }
  free(t);
}

/**
 * Helper function to check the cpu, gpu, and thread mappings
 * **/
void check_mapping(mpibind_t *handle, mpibind_test_ans_t *expected) {
  char *separator = ";";
  char thread_map_info[BUF_SIZE] = {'\0'};
  char cpu_map_info[BUF_SIZE] = {'\0'};
  char gpu_map_info[BUF_SIZE] = {'\0'};

  hwloc_bitmap_t* cpus = mpibind_get_cpus(handle);
  hwloc_bitmap_t* gpus = mpibind_get_gpus(handle);
  int* threads = mpibind_get_nthreads(handle);
  int num_tasks = mpibind_get_ntasks(handle);

  // Concat string array into single string
  int i = 0;
  while (i < num_tasks) {
    sprintf(thread_map_info + strlen(thread_map_info), "%d",
            threads[i]);
    hwloc_bitmap_list_snprintf(cpu_map_info + strlen(cpu_map_info),
                               sizeof(cpu_map_info), cpus[i]);
    hwloc_bitmap_list_snprintf(gpu_map_info + strlen(gpu_map_info),
                               sizeof(gpu_map_info), gpus[i]);

    // Separate entries with the specified separator
    if (++i != num_tasks) {
      strcat(thread_map_info, separator);
      strcat(cpu_map_info, separator);
      strcat(gpu_map_info, separator);
    }
  }

  // Check mappings against expected output
  if (strcmp(expected->cpu_mapping, cpu_map_info) ||
      strcmp(expected->gpu_mapping, gpu_map_info) ||
      strcmp(expected->thread_mapping, thread_map_info)) {
    fail("%s", expected->description);
    diag(
        "\tThreads: \"%s\" (provided answer: \"%s\")\n\tCPU: "
        "\"%s\" (provided answer: \"%s\")\n\tGPU: \"%s\" (provided answer: "
        "\"%s\")",
        thread_map_info, expected->thread_mapping, cpu_map_info,
        expected->cpu_mapping, gpu_map_info, expected->gpu_mapping);
  } else {
    pass("%s", expected->description);
  }
}

/**
 * Runs a set of tests and compares them to their answers.
 * **/
void run_test(hwloc_topology_t topo, mpibind_test_t *params,
              mpibind_test_ans_t *expected) {
  mpibind_t *handle;
  hwloc_topology_t t;

  // dup the topology so the original doesn't accidentally get destroyed
  mpibind_test_t_print(params);
  hwloc_topology_dup(&t, topo);
  mpibind_init(&handle);
  mpibind_set_topology(handle, t);
  mpibind_set_ntasks(handle, params->ntasks);
  mpibind_set_nthreads(handle, params->in_nthreads);
  mpibind_set_greedy(handle, params->greedy);
  mpibind_set_gpu_optim(handle, params->gpu_optim);
  mpibind_set_smt(handle, params->smt);
  mpibind_set_restrict_type(handle, params->restr_type);

  mpibind_set_restrict_ids(handle, params->restr_set);

  if (mpibind(handle)) {
    fail("%s\n\tEncountered error running mpibind", expected->description);
  } else {
    check_mapping(handle, expected);
    mpibind_finalize(handle);
  }
  hwloc_topology_destroy(t);
}

/**
 * Generate unit test information from a topology.
 * This will take high level information and create an array of
 * of objects containing parameters for each of the tests. The number
 * of tests created is passed back via num_test_ptr
 * **/
mpibind_test_t **generate_test_information(hwloc_topology_t topo,
                                           int *num_test_ptr) {
  int num_tests = 12;  // number of tests being created
  mpibind_test_t **tests = calloc(num_tests, sizeof(mpibind_test_t *));
  *num_test_ptr = num_tests;

  // find the number of components at each level
  int num_numas = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_NUMANODE);
  int num_cores = hwloc_get_nbobjs_by_depth(topo, get_core_depth(topo));

#if TEST_DEBUG
  int num_gpus = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_OS_DEVICE);
  int num_pus = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_PU);

  printf("Num numas: %d \n", num_numas);
  printf("Num gpus: %d \n", num_gpus);
  printf("Num cores: %d \n", num_cores);
  printf("Num PUs: %d \n", num_pus);
#endif

  // get id of first numa domain for use in tests
  int numa_id =
      hwloc_get_next_obj_by_type(topo, HWLOC_OBJ_NUMANODE, NULL)->os_index;

  // get cpuset of first core for use in tests. If the cores have been
  // collapsed into PUs, then get the id of the first PU instead
  hwloc_cpuset_t core_cpuset;
  if (hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_CORE) > 0) {
    core_cpuset = hwloc_bitmap_dup(
        hwloc_get_next_obj_by_depth(topo, get_core_depth(topo), NULL)->cpuset);
  } else {
    core_cpuset = hwloc_bitmap_alloc();
    hwloc_bitmap_set(core_cpuset, hwloc_get_next_obj_by_depth(
                                      topo, get_core_depth(topo), NULL)
                                      ->os_index);
  }

  // get first PU for testing
  int pu_id = hwloc_bitmap_first(core_cpuset);

  // find max arity btwn all the cores on the machine
  int max_arity = 1;
  hwloc_obj_t obj;
  if ((obj = hwloc_get_next_obj_by_depth(topo, get_core_depth(topo), NULL))) {
    max_arity = obj->arity == 0 ? 1 : obj->arity;
  }

  mpibind_test_t **ptr = tests;
  mpibind_test_t *handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  // 1: Map one task to every core
  mpibind_test_t_init(handle);
  handle->ntasks = num_cores;
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  // 2: Map one task greedily
  mpibind_test_t_init(handle);
  handle->ntasks = 1;
  handle->greedy = 1;
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  // 3: Map two tasks greedily
  mpibind_test_t_init(handle);
  handle->ntasks = 2;
  handle->greedy = 1;
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  mpibind_test_t_init(handle);
  // 4: Mapping such that ntasks < #NUMA nodes but nworkers > #NUMA nodes
  handle->in_nthreads = num_numas * 2;
  handle->ntasks = (num_numas == 1) ? 1 : num_numas - 1;
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  mpibind_test_t_init(handle);
  // 5: Restrict x tasks a single core (x == machine's smt level)
  handle->ntasks = max_arity;
  handle->restr_type = MPIBIND_RESTRICT_CPU;
  handle->restr_set = calloc(50, sizeof(char));
  hwloc_bitmap_list_snprintf(handle->restr_set, sizeof(handle->restr_set),
                             core_cpuset);
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  mpibind_test_t_init(handle);
  // 6: Map two tasks at smt 1
  handle->ntasks = 2;
  handle->smt = 1;
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  mpibind_test_t_init(handle);
  // 7: Map two tasks at max_smt
  handle->ntasks = 2;
  handle->smt = max_arity;
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  mpibind_test_t_init(handle);
  // 8: Map two tasks at (max_smt - 1)
  handle->ntasks = 2;
  handle->smt = (max_arity == 1) ? 1 : max_arity - 1;
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  mpibind_test_t_init(handle);
  // 9: Map two tasks, but restrict them to a single NUMA domain
  handle->ntasks = 2;
  handle->restr_type = MPIBIND_RESTRICT_MEM;
  handle->restr_set = calloc(10, sizeof(char));
  sprintf(handle->restr_set, "%d", numa_id);
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  mpibind_test_t_init(handle);
  // 11: Map num_numa tasks without GPU optimization
  handle->ntasks = num_numas;
  handle->gpu_optim = 0;
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  mpibind_test_t_init(handle);
  // 12: Map num_numa tasks with GPU optimization
  handle->ntasks = num_numas;
  handle->gpu_optim = 1;
  *ptr++ = handle;

  handle = calloc(1, sizeof(mpibind_test_t));
  mpibind_test_t_init(handle);
  // 12: Map smt tasks to a single pu
  handle->ntasks = 8;
  handle->gpu_optim = 0;
  handle->restr_type = MPIBIND_RESTRICT_CPU;
  handle->restr_set = calloc(10, sizeof(char));
  sprintf(handle->restr_set, "%d", pu_id);
  *ptr++ = handle;

  hwloc_bitmap_free(core_cpuset);
  return tests;
}

/** load an xml file into a topology **/
void load_topology(hwloc_topology_t *topo, char *xml_file) {
  hwloc_topology_init(topo);
  if (hwloc_topology_set_xml(*topo, xml_file) < 0)
    perror("Failed to set topology");
  hwloc_topology_set_all_types_filter(*topo, HWLOC_TYPE_FILTER_KEEP_STRUCTURE);
  hwloc_topology_set_type_filter(*topo, HWLOC_OBJ_OS_DEVICE,
                                 HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  hwloc_topology_load(*topo);
}

/**
 * Parse a single answer from an answer file
 * An answer consists of three consectutive lines with each line containing
 * the thread mapping, cpu mapping, and gpu mapping in that order
 * */
static int parse_answer(mpibind_test_ans_t *expected, FILE *fp) {
  if (!expected) {
    return 1;
  }

  char buf[BUF_SIZE];
  char *tmp;

  // Skip newlines and comments
  do {
    if (!fgets(buf, sizeof(buf), fp)) {
      perror("error reading from file");
      exit(1);
    }
  } while (!strcmp(buf, "\n") || buf[0] == '#');

  // Since we were using fgets, we need to take out the newline if present
  // else it would added to the description
  if ((tmp = strchr(buf, '\n')) >= 0) {
    *tmp = '\0';
  }

  // Copy description
#if TEST_DEBUG
  printf("%s\n", buf);
#endif
  expected->description = calloc(BUF_SIZE, sizeof(char));
  strcpy(expected->description, buf);

  if (!fgets(buf, sizeof(buf), fp)) {
    perror("error reading from file");
    exit(1);
  }

  // Copy thread mapping
#if TEST_DEBUG
  printf("%s\n", buf);
#endif

  expected->thread_mapping = calloc(BUF_SIZE, sizeof(char));
  if (sscanf(buf, "\"%[^\"]\"", expected->thread_mapping) < 0) {
    perror("error parsing answers");
    exit(1);
  }

  if (!fgets(buf, sizeof(buf), fp)) {
    perror("error reading from file");
    exit(1);
  }

  // Copy cpu mapping
#if TEST_DEBUG
  printf("%s\n", buf);
#endif
  expected->cpu_mapping = calloc(BUF_SIZE, sizeof(char));
  if (sscanf(buf, "\"%[^\"]\"", expected->cpu_mapping) < 0) {
    perror("error parsing answers");
    exit(1);
  }

  if (!fgets(buf, sizeof(buf), fp)) {
    perror("error reading from file");
    exit(1);
  }

  // Copy gpu mapping
#if TEST_DEBUG
  printf("%s\n", buf);
#endif
  expected->gpu_mapping = calloc(BUF_SIZE, sizeof(char));
  if (sscanf(buf, "\"%[^\"]\"", expected->gpu_mapping) < 0) {
    perror("error parsing answers");
    exit(1);
  }

  return 0;
}

mpibind_test_ans_t **load_answers(char *filename, int *num_tests_ptr) {
  int num_tests;
  char buf[BUF_SIZE];
  FILE *fp = fopen(filename, "r");

  if (!fp) {
    diag("Failed to open answers file");
    return NULL;
  }

  // Remove initial whitespace
  do {
    if (!fgets(buf, sizeof(buf), fp)) {
      perror("error reading from file");
      exit(1);
    }
  } while (!strcmp(buf, "\n") || buf[0] == '#');

  // Get number of tests
  sscanf(buf, "%d\n", &num_tests);
#if TEST_DEBUG
  printf("load_answers: num_tests = %d\n", num_tests);
#endif
  *num_tests_ptr = num_tests;

  mpibind_test_ans_t **answers =
      calloc(num_tests, sizeof(mpibind_test_ans_t *));

  // Parse answers into return pointer
  int i;
  for (i = 0; i < num_tests; i++) {
    answers[i] = calloc(1, sizeof(mpibind_test_ans_t));
    parse_answer(answers[i], fp);
  }

  // Don't forget to close the file
  fclose(fp);
  return answers;
}

/** Performs a unit test using a given topology xml and an answer file.
 * Test drivers should call this method
 * **/
void unit_test_topology(char *topology_filename, char *answer_filename) {
  int num_answers, num_tests;
  hwloc_topology_t topo;

  load_topology(&topo, topology_filename);
  diag("testing %s", topology_filename);

  mpibind_test_ans_t **answers = load_answers(answer_filename, &num_answers);
  if (!answers) {
    BAIL_OUT("ERROR: could not process answers file");
  }

  mpibind_test_t **tests = generate_test_information(topo, &num_tests);

  if (num_answers != num_tests) {
    BAIL_OUT("ERROR: num. answers: %d, num. tests: %d\n", num_answers,
             num_tests);
  }

  int i;
  for (i = 0; i < num_tests; i++) {
    run_test(topo, tests[i], answers[i]);
  }

  // cleanup everything
  for (i = 0; i < num_tests; i++) {
    mpibind_test_ans_t_free(answers[i]);
    mpibind_test_t_free(tests[i]);
  }
  free(answers);
  free(tests);
  hwloc_topology_destroy(topo);
}