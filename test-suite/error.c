#include "test_utils.h"
#define XML_PATH "../topo-xml/coral-lassen.xml"

/**Test passing null to all setters and getter functions**/
int test_null_handle() {
  diag("Testing passing a null handle to setters and getters");
  mpibind_t *handle = NULL;
  int count; //for mpibind_get_env_var_names

  ok(mpibind_set_ntasks(handle, 4) == 1,
     "mpibind_set_ntasks fails when handle == NULL");
  ok(mpibind_set_nthreads(handle, 4) == 1,
     "mpibind_set_nthreads fails when handle == NULL");
  ok(mpibind_set_greedy(handle, 1) == 1,
     "mpibind_set_greedy fails when handle == NULL");
  ok(mpibind_set_gpu_optim(handle, 1) == 1,
     "mpibind_set_gpu_optim fails when handle == NULL");
  ok(mpibind_set_smt(handle, 1) == 1,
     "mpibind_set_smt fails when handle == NULL");
  ok(mpibind_set_restrict_ids(handle, NULL) == 1,
     "mpibind_set_restrict_ids fails when handle == NULL");
  ok(mpibind_set_restrict_type(handle, 1) == 1,
     "mpibind_set_restrict_type fails when handle == NULL");
  ok(mpibind_set_topology(handle, NULL) == 1,
     "mpibind_set_topology fails when handle == NULL");
  ok(mpibind_set_env_vars(handle) == 1,
     "mpibind_set_end_vars fails when handle == NULL");

  ok(mpibind_get_ntasks(handle) == -1,
     "mpibind_get_ntasks return -1 when handle == NULL");
  ok(mpibind_get_greedy(handle) == -1,
     "mpibind_get_greedy return -1 when handle == NULL");
  ok(mpibind_get_gpu_optim(handle) == -1,
     "mpibind_get_gpu_optim return -1 when handle == NULL");
  ok(mpibind_get_smt(handle) == -1,
     "mpibind_get_smt return -1 when handle == NULL");
  ok(mpibind_get_restrict_ids(handle) == NULL,
     "mpibind_get_ntasks return NULL when handle == NULL");
  ok(mpibind_get_restrict_type(handle) == -1,
     "mpibind_get_restrict_type return -1 when handle == NULL");

  ok(mpibind_get_nthreads(handle) == NULL,
     "mpibind_get_nthreads returns NULL when handle == NULL");
  ok(mpibind_get_cpus(handle) == NULL,
     "mpibind_get_cpus returns NULL when handle == NULL");
  ok(mpibind_get_gpus(handle) == NULL,
     "mpibind_get_gpus returns NULL when handle == NULL");
  //ok(mpibind_get_gpu_type(handle) == -1,
  //   "mpibind_get_gpu_type returns NULL when handle == NULL");
  ok(mpibind_get_topology(handle) == NULL,
     "mpibind_get_topology returns NULL when handle == NULL");
  ok(mpibind_get_env_var_values(handle, NULL) == NULL,
     "mpibind_get_env_var_values returns NULL when handle == NULL");
  ok(mpibind_get_env_var_names(handle, &count) == NULL,
     "mpibind_get_env_var_names returns NULL when handle == NULL");
  ok(mpibind_finalize(handle) == 1,
     "mpibind_finalize fails when handle == NULL");

  return 0;
}

int test_mpibind_errors() {
  mpibind_t *handle;
  hwloc_topology_t topo;

  // setup topology
  hwloc_topology_init(&topo);
  hwloc_topology_set_xml(topo, XML_PATH);
  hwloc_topology_set_all_types_filter(topo, HWLOC_TYPE_FILTER_KEEP_STRUCTURE);
  hwloc_topology_set_type_filter(topo, HWLOC_OBJ_OS_DEVICE,
                                 HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  hwloc_topology_load(topo);

  mpibind_init(&handle);

  int ntasks = 5;
  mpibind_set_ntasks(handle, ntasks);
  mpibind_set_nthreads(handle, 4);
  mpibind_set_greedy(handle, 0);
  mpibind_set_gpu_optim(handle, 0);
  mpibind_set_smt(handle, 2);

  diag("Testing error handling in mpibind()");

  mpibind_set_nthreads(handle, -4);
  ok(mpibind(handle) == 1, "Mapping fails if nthreads is invalid");

  mpibind_set_nthreads(handle, 4);
  mpibind_set_ntasks(handle, -1);
  ok(mpibind(handle) == 1, "Mapping fails if ntasks is invalid");

  mpibind_set_ntasks(handle, 4);
  mpibind_set_smt(handle, -1);
  ok(mpibind(handle) == 1, "Mapping fails if smt is invalid");

  mpibind_set_smt(handle, 16);
  ok(mpibind(handle) == 1, "Mapping fails if smt is valid but too high");

  // TODO: ERROR CODES RELATED TO RESTRICT SETS
  todo("Error codes related to restrict sets");
  return 0;
}

int main(int argc, char **argv) {
  plan(NO_PLAN);
  test_null_handle();
  test_mpibind_errors();
  done_testing();
  return (0);
}
