#include "test_utils.h"

static void check_amd_env() {
  mpibind_t* handle;
  hwloc_topology_t topo;

  load_topology(&topo, "../topo-xml/epyc-corona.xml");

  mpibind_init(&handle);
  mpibind_set_topology(handle, topo);
  mpibind_set_ntasks(handle, 1);
  mpibind_set_gpu_optim(handle, 1);
  mpibind_set_greedy(handle, 1);

  mpibind(handle);

  mpibind_set_env_vars(handle);
  //ok(mpibind_get_gpu_type(handle) == MPIBIND_GPU_AMD,
  //   "mpibind correctly identifies AMD gpus");

  int num, i, idx = -1;
  char** env_var_names = mpibind_get_env_var_names(handle, &num);

  for (i = 0; i < num; i++) {
    if (!strcmp(env_var_names[i], "ROCR_VISIBLE_DEVICES")) {
      idx = i;
    }
  }

  ok(idx >= 0, "GPU variable is correct");

  mpibind_finalize(handle);
  hwloc_topology_destroy(topo);
}

static void check_omp_places() {
  mpibind_t* handle;
  hwloc_topology_t topo;

  load_topology(&topo, "../topo-xml/epyc-corona.xml");

  mpibind_init(&handle);
  mpibind_set_topology(handle, topo);
  mpibind_set_ntasks(handle, 1);
  mpibind_set_gpu_optim(handle, 0);
  mpibind_set_smt(handle, 1);
  mpibind_set_greedy(handle, 1);
  mpibind_set_restrict_type(handle, MPIBIND_RESTRICT_CPU);
  mpibind_set_restrict_ids(handle, "6-11");

  mpibind(handle);

  mpibind_set_env_vars(handle);

  int num, i, rc = -1;
  char** env_var_names = mpibind_get_env_var_names(handle, &num);
  char** env_var_values;

  for (i = 0; i < num; i++) {
    if (!strcmp(env_var_names[i], "OMP_PLACES")) {
      env_var_values = mpibind_get_env_var_values(handle, "OMP_PLACES");
      rc = is(env_var_values[0], "{6},{7},{8},{9},{10},{11}",
              "Checking OMP_PLACES mapping");
      break;
    }
  }

  if (rc == -1) {
    fail("failed to find OMP_PLACES variable");
  }

  mpibind_finalize(handle);
  hwloc_topology_destroy(topo);
}

static void check_nvidia_env() {
  mpibind_t* handle;
  hwloc_topology_t topo;

  load_topology(&topo, "../topo-xml/coral-lassen.xml");

  mpibind_init(&handle);
  mpibind_set_topology(handle, topo);
  mpibind_set_ntasks(handle, 1);
  mpibind_set_gpu_optim(handle, 1);
  mpibind_set_greedy(handle, 1);

  mpibind(handle);

  mpibind_set_env_vars(handle);
  //ok(mpibind_get_gpu_type(handle) == MPIBIND_GPU_NVIDIA,
  //   "mpibind correctly identifies NVIDIA gpus");

  int num, i, idx = -1;
  char** env_var_names = mpibind_get_env_var_names(handle, &num);
  for (i = 0; i < num; i++) {
    if (!strcmp(env_var_names[i], "CUDA_VISIBLE_DEVICES")) {
      idx = i;
    }
  }
  ok(idx >= 0, "GPU variable is correct");

  mpibind_finalize(handle);
  hwloc_topology_destroy(topo);
}

int main(int argc, char** argv) {
  plan(NO_PLAN);

  check_amd_env();
  check_nvidia_env();
  check_omp_places();

  done_testing();
  return (0);
}
