#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <slurm/spank.h>
#include <hwloc.h>
#include <mpibind.h>

/*
 * Notes
 *
 * In some cases, the plugin hangs when not using an XML
 * topology file. The hang is triggered by hwloc when enabling
 * OS devices and discovering the topology. This is not an
 * mpibind or hwloc issue.
 * See TOSS-6198.
 *
 */

/*
 * SPANK errors
 *
 * ESPANK_ERROR
 * ESPANK_BAD_ARG,
 * ESPANK_NOT_TASK,
 * ESPANK_ENV_EXISTS,
 * ESPANK_ENV_NOEXIST,
 * ESPANK_NOSPACE,
 * ESPANK_NOT_REMOTE,
 * ESPANK_NOEXIST,
 * ESPANK_NOT_EXECD,
 * ESPANK_NOT_AVAIL,
 * ESPANK_NOT_LOCAL,
 *
 */

/*
 * Spank plugin name and version
 */
SPANK_PLUGIN(mpibind, 2);

#define STR(x) #x
#define XSTR(x) STR(x)
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#define PRINT_DEBUG(...) if (opt_debug) fprintf(stderr, __VA_ARGS__)

#define LONG_STR_SIZE 2048
#define PRINT_MAP_BUF_SIZE (1024*5)

/*
 * S_ALLOC_CORES can be one of:
 * (1) S_JOB_ALLOC_CORES
 *     mpibind will use all the cores allocated to the job,
 *     even when the user restricts the program to run
 *     on a subset of cores, e.g., with --cpus-per-task.
 * (2) S_STEP_ALLOC_CORES
 *     mpibind will use only those cores allocated to the
 *     job step even when the job has more cores available to it.
 */
//#define S_ALLOC_CORES  S_JOB_ALLOC_CORES
#define S_ALLOC_CORES  S_STEP_ALLOC_CORES

/************************************************
 * Global variables
 ************************************************/

/* mpibind options */
/* -1 indicates not set by user, i.e., use mpibind defaults */
static int opt_gpu = -1;
static int opt_smt = -1;
/* Enable greedy by default */
static int opt_greedy = 1;

/* mpibind plugin options */
static int opt_verbose = 0;
static int opt_debug = 0;
static int opt_enable = 1;

/* plugstack.conf: if 'default_off' then 1, else 0 */
static int opt_conf_disabled = 0;
/* Only set affinity if this job has exclusive access to this node.
   Set via plugstack.conf: if 'exclusive_only_off' then 0, else 1 */
static int opt_exclusive_only = 1;
/* if '--mpibind=on' then 1, elif '--mpibind=off' then 0, else -1 */
static int opt_user_specified = -1;

/* mpibind vars */
static mpibind_t *mph = NULL;
static hwloc_topology_t topo = NULL;

/************************************************
 * Forward declarations
 ************************************************/
static int parse_user_options(int val, const char *optarg, int remote);

/************************************************
 * Spank plugin options
 ************************************************/
struct spank_option spank_options [] =
  {
   { "mpibind", "[args]",
     "Memory-driven mapping algorithm for supercomputers "
     "(args=help for more info)",
     2, 0, (spank_opt_cb_f) parse_user_options
   },
   SPANK_OPTIONS_TABLE_END
  };

/************************************************
 * Functions
 ************************************************/

/*
 * Slurm items:
 *
 * S_TASK_ID: Local task id (int *)
 * S_JOB_LOCAL_TASK_COUNT: Number of local tasks (uint32_t *)
 * S_JOB_NNODES: Total number of nodes in job (uint32_t *)
 * S_JOB_NODEID: Relative id of this node (uint32_t *)
 * S_JOB_STEPID: Slurm job step id (uint32_t *)
 *
 */
static
void print_spank_items(spank_t sp)
{
  /*
     S_JOB_ALLOC_CORES
     Job allocated cores in list format (char **)
     Ex: S_JOB_ALLOC_CORES=0-111
     Ex: S_JOB_ALLOC_CORES=0-1,56
     This is what I can use to replace SLURM_CPUS_ON_NODE,
     Its value does not depend on the number of tasks or
     cpus-per-task requested.
     It works for core-scheduled clusters as well--its value
     has the subset of cores associated with the job/allocation
     and does not change across job steps.
     Since the value is given as a list I need to count the cores.
  */
#define SI_NAME1 S_JOB_ALLOC_CORES
  const char *s1;
  if (spank_get_item(sp, SI_NAME1, &s1) != ESPANK_SUCCESS) {
    slurm_error("mpibind: Failed to get " XSTR(SI_NAME1));
    return;
  }
  PRINT(XSTR(SI_NAME1) "=%s\n", s1);

  /*
     S_STEP_ALLOC_CORES
     Step alloc'd cores in list format  (char **)
     Ex: S_STEP_ALLOC_CORES=0-2,56-58
     Value depends on the number of tasks and cpus-per-task
     requested.
  */
#define SI_NAME2 S_STEP_ALLOC_CORES
  const char *s2;
  if (spank_get_item(sp, SI_NAME2, &s2) != ESPANK_SUCCESS) {
    slurm_error("mpibind: Failed to get " XSTR(SI_NAME2));
    return;
  }
  PRINT(XSTR(SI_NAME2) "=%s\n", s2);

  /*
     S_STEP_CPUS_PER_TASK (uint32_t *)
     CPUs allocated per task (=1 if --overcommit option is used)
     Ex: S_STEP_CPUS_PER_TASK=3
     Matches cpus-per-task requested or default value (1)
  */
#define SI_NAME3 S_STEP_CPUS_PER_TASK
  uint32_t vu32;
  if (spank_get_item(sp, SI_NAME3, &vu32) != ESPANK_SUCCESS) {
    slurm_error ("mpibind: Failed to get " XSTR(SI_NAME3));
    return;
  }
  PRINT(XSTR(SI_NAME3) "=%d\n", vu32);

  /*
     S_JOB_NCPUS
     Number of CPUs used by this job (uint16_t *)
     Ex: S_JOB_NCPUS=6
     Matches ntasks*cpus-per-task
     If no cpus-per-task given, it provides all of the CPUs on node.
     I can't use as a replacement of SLURM_CPUS_ON_NODE because
     value depends on cpus-per-task.
   */
#define SI_NAME4 S_JOB_NCPUS
  uint16_t vu16;
  if (spank_get_item(sp, SI_NAME4, &vu16) != ESPANK_SUCCESS) {
    slurm_error ("mpibind: Failed to get " XSTR(SI_NAME4));
    return;
  }
  PRINT(XSTR(SI_NAME4) "=%d\n", vu16);
}

/*
 * Print Slurm context, e.g., local, remote.
 */
#if 0
static
void print_context(char *note)
{
  int nc = 0;
  char str[128];

  if (note != NULL)
    nc += snprintf(str, sizeof(str), "%s: ", note);

  switch(spank_context()) {
  case S_CTX_LOCAL:
    nc += snprintf(str+nc, sizeof(str)-nc,
		   "Local context (srun)");
    break;
  case S_CTX_REMOTE:
    nc += snprintf(str+nc, sizeof(str)-nc,
		   "Remote context (slurmstepd)");
    break;
  case S_CTX_ALLOCATOR:
    nc += snprintf(str+nc, sizeof(str)-nc,
		   "Allocator context (sbatch/salloc)");
    break;
  case S_CTX_SLURMD:
    nc += snprintf(str+nc, sizeof(str)-nc,
		   "slurmd context");
    break;
  case S_CTX_JOB_SCRIPT:
    nc += snprintf(str+nc, sizeof(str)-nc,
		   "Prolog/epilog context");
    break;
  case S_CTX_ERROR:
    nc += snprintf(str+nc, sizeof(str)-nc,
		   "Error obtaining current context");
    break;
  default :
    nc += snprintf(str+nc, sizeof(str)-nc,
		   "Invalid context");
  }
  nc+= snprintf(str+nc, sizeof(str)-nc, "\n");

  PRINT(str);
}
#endif

static
void print_user_options()
{
  PRINT("Options: enable=%d "
	  "conf_disabled=%d user_specified=%d excl_only=%d "
	  "verbose=%d debug=%d "
	  "gpu=%d smt=%d greedy=%d\n",
	  opt_enable,
	  opt_conf_disabled, opt_user_specified, opt_exclusive_only,
	  opt_verbose, opt_debug,
	  opt_gpu, opt_smt, opt_greedy);
}

/*
 * User optios passed via '--mpibind' on srun command line
 */
static
int parse_user_options(int val, const char *arg, int remote)
{
  //fprintf(stderr, "mpibind option specified\n");
  const char delim[] = ",";

  /* '--mpibind' with no args enables the plugin */
  if (arg == NULL) {
    opt_user_specified = 1;
    return 0;
  }

  /* Don't overwrite the original string */
  char *str = strdup(arg);

  /* First token */
  char *token = strtok(str, delim);

  char *msg;
  /* Options not implemented yet */
  int master=-1, omp_places=-1, omp_proc_bind=-1, visdevs=-1;

  while (token != NULL) {
    //fprintf(stderr, "%s\n", token);
    msg = mpibind_parse_option(token,
			       &opt_debug,
			       &opt_gpu,
			       &opt_greedy,
			       &master,
			       &omp_places,
			       &omp_proc_bind,
			       &opt_smt,
			       &opt_user_specified,
			       &opt_verbose,
			       &visdevs);

    if (msg) {
      PRINT("%s\n", msg);
      free(msg);
      free(str);
      exit(0);
    }

    token = strtok(NULL, delim);
  }

  free(str);

  return 0;
}

/*
 * Parse plugstack.conf options.
 */
static
int parse_conf_options(int argc, char *argv[], int remote)
{
  int i;

  for (i=0; i<argc; i++) {
    //fprintf(stderr, "confopt=%s\n", argv[i]);

    if (strcmp(argv[i], "default_off") == 0)
      opt_conf_disabled = 1;
    else if (strcmp(argv[i], "exclusive_only_off") == 0)
      opt_exclusive_only = 0;
    else {
      PRINT("mpibind: Invalid plugstack.conf argument %s\n",
	      argv[i]);
      return -1;
    }
  }

  return 0;
}

/*
 * Get the total number of cores on this node.
 * This number is for actual cores regardless of SMT mode.
 */
static
int get_ncores_on_node()
{
  FILE *fp = popen("grep '' /sys/devices/system/cpu/cpu*/topology/core_cpus_list | awk -F: '{print $2}' | sort -u | wc -l", "r");

  int ncores=-1;
  if (fp == NULL || fscanf(fp, "%d", &ncores) != 1)
    PRINT("Could not get the number of cores on node\n");

  PRINT_DEBUG("Number of cores on this node: %d\n", ncores);

  pclose(fp);

  return ncores;
}

static
int restrict_to_allocated_cores(spank_t sp, hwloc_topology_t topo)
{
  int rc = 0;

  /* List of allocated cores */
  char *cores;
  if (spank_get_item(sp, S_ALLOC_CORES, &cores) != ESPANK_SUCCESS) {
    slurm_error("mpibind: Failed to get " XSTR(S_ALLOC_CORES));
    return -1;
  }

  /* Do not try to restrict the topology to the allocated cores
     if Slurm has already done that. This is dependent on the
     Slurm resource selection plugin:
     - CR_core: Only the allocated cores are visible
       in slurm_spank_user_init
     - CR_core_memory: All of the cores of the node are visible
       in slurm_spank_user_init */
  int ncores_alloc = mpibind_range_nints(cores);
  int ncores_hwloc = hwloc_get_nbobjs_by_depth(topo,
					       mpibind_get_core_depth(topo));
  if (ncores_hwloc <= ncores_alloc)
    return 0;

  PRINT_DEBUG("Restricting topology to allocated cores %s\n", cores);

  char pus[LONG_STR_SIZE];
  if (mpibind_cores_to_pus(topo, cores, pus, LONG_STR_SIZE) != 0)
    return -1;

  hwloc_bitmap_t cpus = hwloc_bitmap_alloc();
  hwloc_bitmap_list_sscanf(cpus, pus);
  if (hwloc_topology_restrict(topo, cpus,
			      HWLOC_RESTRICT_FLAG_REMOVE_CPULESS) < 0) {
    PRINT("Error: hwloc_topology_restrict %s\n", pus);
    rc = -1;
  }
  hwloc_bitmap_free(cpus);

  return rc;
}

static
int get_ncores_allocated(spank_t sp)
{
  /* Generate the list of allocated cores */
  char *str;
  if (spank_get_item(sp, S_ALLOC_CORES, &str) != ESPANK_SUCCESS) {
    slurm_error("mpibind: Failed to get " XSTR(S_ALLOC_CORES));
    return -1;
  }

  /* Get the number of cores from the list */
  return mpibind_range_nints(str);
}

static
uint32_t get_nnodes(spank_t sp)
{
  uint32_t nnodes = 0;
  spank_err_t rc;

  if ( (rc=spank_get_item(sp, S_JOB_NNODES, &nnodes)) !=
       ESPANK_SUCCESS )
    PRINT("mpibind: Failed to get node count: %s",
	    spank_strerror(rc));

  return nnodes;
}

static
uint32_t get_nodeid(spank_t sp)
{
  uint32_t nodeid = -1;
  spank_err_t rc;

  if ( (rc=spank_get_item(sp, S_JOB_NODEID, &nodeid)) !=
       ESPANK_SUCCESS )
    PRINT("mpibind: Failed to get node id: %s",
	    spank_strerror(rc));

  return nodeid;
}

static
uint32_t get_local_ntasks(spank_t sp)
{
  uint32_t ntasks = 0;
  spank_err_t rc;

  if ( (rc=spank_get_item(sp, S_JOB_LOCAL_TASK_COUNT, &ntasks)) !=
       ESPANK_SUCCESS )
    PRINT("mpibind: Failed to get local task count: %s",
	    spank_strerror(rc));

  return ntasks;
}

static
int get_local_taskid(spank_t sp)
{
  int taskid = -1;
  spank_err_t rc;

  if ( (rc=spank_get_item(sp, S_TASK_ID, &taskid)) !=
       ESPANK_SUCCESS )
    PRINT("mpibind: Failed to get local task id: %s",
	    spank_strerror(rc));

  return taskid;
}

static
int get_omp_num_threads(spank_t sp)
{
  const char var[] = "OMP_NUM_THREADS";
  char str[16];
  int val;

  if (spank_getenv(sp, var, str, sizeof(str)) != ESPANK_SUCCESS)
    return -1;

  if ((val = atoi(str)) <= 0)
    return -1;

  return val;
}

/*
 * Return 1 if this is an salloc command.
 * salloc: job_stepid=0xfffffffa
 * sbatch: job_stepid=0xfffffffb
 */
static
int job_is_alloc(spank_t sp)
{
  uint32_t stepid;

  if (spank_get_item(sp, S_JOB_STEPID, &stepid) != ESPANK_SUCCESS) {
    slurm_error ("mpibind: Failed to get S_JOB_STEPID");
    return -1;
  }

  if (stepid >= 0xfffffffa) {
    //fprintf(stderr, "mpibind: stepid is 0x%x\n", stepid);
    return 1;
  }

  return 0;
}

static
int job_is_exclusive(spank_t sp)
{
  int ncores_node = get_ncores_on_node();
  int ncores_alloc = get_ncores_allocated(sp);

  return (ncores_alloc >= ncores_node);
}

/*
 * Determine if mpibind should be on or off.
 */
static
int mpibind_is_on(int exclusive)
{
  /* User specifying on or off has the highest priority */
  if (opt_user_specified > -1)
    return opt_user_specified;

  /* User did not specify to turn it on or off */
  /* And plugstack config turned it off */
  else if (opt_conf_disabled)
    return 0;

  /* At plugstack config, mpibind is on */
  /* And mpibind is on on all jobs */
  else if (!opt_exclusive_only)
    return 1;

  /* Turn on only on exclusive jobs */
  /* And the job is using the node exclusively */
  else if (exclusive)
    return 1;
  else
    return 0;
}

static
int clean_up(mpibind_t *mph, hwloc_topology_t topo)
{
  int rc=0;

  if ((rc=mpibind_finalize(mph)) != 0)
    PRINT("mpibind: mpibind_finalize failed\n");

  hwloc_topology_destroy(topo);

  return rc;
}

/************************************************
 * SPANK callback functions
 ************************************************/

/*
 * Local context (srun)
 * Called once.
 *
 * This code might be executing with root priviledges.
 *
 * Do not initialize the mpibind or hwloc topology handles
 * here, because sbatch jobs would not complete. Perhaps,
 * due to priviledge execution.
 *
 * Cannot print to the console.
 */
int slurm_spank_init(spank_t sp, int ac, char *argv[])
{
#if 0
  char name[] = "slurm_spank_init";
  print_context(name);
#endif

  if (!spank_remote(sp))
    return ESPANK_SUCCESS;

  return ESPANK_SUCCESS;
}

/*
 * Local context (srun)
 * Called once.
 *
 * Cannot print to the console, but output does show
 * in the sbatch output file.
 */
int slurm_spank_exit(spank_t sp, int ac, char *argv[])
{
#if 0
  char name[] = "slurm_spank_exit";
  print_context(name);
#endif

  if (!spank_remote(sp))
    return ESPANK_SUCCESS;

  return ESPANK_SUCCESS;
}

/*
 * Remote context (slurmstepd)
 * Called once per node.
 *
 */
int slurm_spank_user_init(spank_t sp, int ac, char *argv[])
{
#if 0
  char name[] = "slurm_spank_user_init";
  print_context(name);
#endif

  if (!spank_remote(sp))
    return ESPANK_SUCCESS;

  /* I could do this in slurm_spank_init, but can't
     print to the console there if there's an error */
  if (parse_conf_options(ac, argv, spank_remote(sp)) < 0) {
    opt_enable = 0;
    return ESPANK_ERROR;
  }

  /* Disable mpibind in salloc/sbatch commands */
  /* The side effect of calling mpibind with salloc is that
     env vars set by mpibind continue to be set (and presumably
     the cpu bindings). For example, if OMP_NUM_THREADS is set to
     all CPUs, that has an effect on subsequent srun calls with
     mpibind. To check whether salloc is calling mpibind after
     the nodes are allocated, check the value of OMP_NUM_THREADS */
  if (job_is_alloc(sp) == 1) {
    opt_enable = 0;
    PRINT_DEBUG("Disabling mpibind for salloc/sbatch\n");
    return ESPANK_SUCCESS;
  }

  /* Test config options without changing plugstack.conf */
  //opt_exclusive_only = 1;
  //opt_conf_disabled = 0;

  char header[16];
  uint32_t nodeid = get_nodeid(sp);
  uint32_t nnodes = get_nnodes(sp);
  sprintf(header, "Node %d/%d", nodeid, nnodes);

  if (nodeid == 0 && opt_debug) {
    print_spank_items(sp);
    print_user_options();
  }

  /* Determine if mpibind should be on or off */
  int exclusive = job_is_exclusive(sp);
  if ( !(opt_enable = mpibind_is_on(exclusive)) ) {
    PRINT_DEBUG("mpibind is off\n");
    return ESPANK_SUCCESS;
  }

  uint32_t ntasks = get_local_ntasks(sp);
  int nthreads = get_omp_num_threads(sp);

  /*
   * Set the topology
   */
#if 1
  /* Allocate hwloc's topology handle */
  if ( hwloc_topology_init(&topo) < 0 ) {
    opt_enable = 0;
    slurm_error("hwloc_topology_init");
    return ESPANK_ERROR;
  }

  /* Use topology file if provided.
     Some LLNL systems require a topology file to overcome
     Bug TOSS-6198 */
  char xml[512];
  xml[0] = '\0';
  if (spank_getenv(sp, "MPIBIND_TOPOFILE", xml, sizeof(xml))
      == ESPANK_SUCCESS && xml[0] != '\0') {

    /* Only load the topology file if the job is exclusive.
       Non-exclusive jobs use only a subset of cores:
       mpibind should keep to that subset instead of using
       all the cores in the topology specified in the XML file */
    if (exclusive) {
      if (hwloc_topology_set_xml(topo, xml) < 0)
	slurm_spank_log("mpibind: hwloc_topology_set_xml failed with %s",
			xml);

      if (nodeid == 0)
	PRINT_DEBUG("mpibind: Loaded topology from %s\n", xml);
    }
  }

  if (mpibind_load_topology(topo) != 0) {
    opt_enable = 0;
    slurm_error("mpibind: mpibind_load_topology");
    return ESPANK_ERROR;
  }

  /* Restrict the topology to the cores allocated for the job.
     Note that restricting to current binding does not work
     because, in some Slurm configurations, slurm_spank_user_init
     runs on the whole node */
  if (!exclusive && restrict_to_allocated_cores(sp, topo) < 0)
    slurm_spank_log("mpibind: Failed to restrict topology to allocated cores");
#endif

  /*
   * Read in MPIBIND environment variables
   */
#if 1
  char restr_str[LONG_STR_SIZE];
  int restr_type = -1;
  if (spank_getenv(sp, "MPIBIND_RESTRICT_TYPE", restr_str, sizeof(restr_str))
      == ESPANK_SUCCESS) {
    if (strcmp(restr_str, "cpu") == 0)
      restr_type = MPIBIND_RESTRICT_CPU;
    else if (strcmp(restr_str, "mem") == 0)
      restr_type = MPIBIND_RESTRICT_MEM;
  }

  if (spank_getenv(sp, "MPIBIND_RESTRICT",
		   restr_str, sizeof(restr_str)) == ESPANK_SUCCESS) {
    if (mpibind_parse_restrict_ids(restr_str, sizeof(restr_str)) == 1)
      restr_str[0] = '\0';
  } else
    restr_str[0] = '\0';
#endif

  /*
   * Everything is set for mpibind
   */
#if 1
  /* Allocate mpibind's global handle */
  if ( mpibind_init(&mph) != 0 || mph == NULL ) {
    opt_enable = 0;
    slurm_error("mpibind: Init failed");
    return ESPANK_ERROR;
  }

  /* Set mpibind input parameters */
  if ( mpibind_set_ntasks(mph, ntasks) != 0 ||
       (nthreads > 0 && mpibind_set_nthreads(mph, nthreads) != 0) ||
       (opt_smt > 0 && mpibind_set_smt(mph, opt_smt) != 0) ||
       (opt_greedy >= 0 && mpibind_set_greedy(mph, opt_greedy) != 0) ||
       (opt_gpu >= 0 && mpibind_set_gpu_optim(mph, opt_gpu) != 0) ||
       (restr_type >= 0 && mpibind_set_restrict_type(mph, restr_type) != 0) ||
       (restr_str[0] && mpibind_set_restrict_ids(mph, restr_str) != 0) ) {
    opt_enable = 0;
    slurm_error("mpibind: Unable to set input parameters");
    return ESPANK_ERROR;
  }

  mpibind_set_topology(mph, topo);

  PRINT_DEBUG("%s: ntasks=%d nthreads=%d greedy=%d gpu=%d "
	      "topo=%p exclusive=%d restr_type=%d restr_ids=%s\n",
	      header,
	      mpibind_get_ntasks(mph),
	      nthreads,
	      mpibind_get_greedy(mph),
	      mpibind_get_gpu_optim(mph),
	      mpibind_get_topology(mph),
	      exclusive,
	      mpibind_get_restrict_type(mph),
	      mpibind_get_restrict_ids(mph));

  /*
   * Get the mpibind mapping!
   */
  if (mpibind(mph) != 0) {
    opt_enable = 0;
    slurm_error("mpibind: Mapping function failed");
    return ESPANK_ERROR;
  }

  /* Set values of env variables for task_init */
  if (mpibind_set_env_vars(mph) != 0) {
    opt_enable = 0;
    slurm_error("mpibind: Failed to set environment variables");
    return ESPANK_ERROR;
  }

  if (opt_verbose) {
    char buf[PRINT_MAP_BUF_SIZE];
    int ngpus = mpibind_get_num_gpus(mph);

    /* Use VISIBLE_DEVICES IDs to enumerate the GPUs
       since users are used to this enumeration
       (as opposed to mpibind's enumeration) */
    mpibind_set_gpu_ids(mph, MPIBIND_ID_SMI);

    // This call had an issue with buffer overrun. Now fixed!
    mpibind_mapping_snprint(buf, PRINT_MAP_BUF_SIZE, mph);

    if (nodeid == 0 || opt_verbose > 1) {
      PRINT("mpibind: %d GPUs on this node\n", ngpus);
      PRINT("%s", buf);
    }
  }
#endif

  return ESPANK_SUCCESS;
}

/*
 * Remote context (slurmstepd)
 * Called once per task
 */
int slurm_spank_task_init(spank_t sp, int ac, char *argv[])
{
#if 0
  char name[] = "slurm_spank_task_init";
  print_context(name);
#endif

  if (!spank_remote(sp) || !opt_enable)
    return ESPANK_SUCCESS;

  char header[16];
  int taskid = get_local_taskid(sp);
  uint32_t ntasks = get_local_ntasks(sp);
  sprintf(header, "Task %d/%d", taskid, ntasks);

  /* Bind this task to the calculated cpus */
  if (mpibind_apply(mph, taskid) != 0) {
    slurm_error("mpibind: Failed to apply mapping");
    return ESPANK_ERROR;
  }

  /* Export environment variables, e.g., *VISIBLE_DEVICES */
  int nvars, i;
  char **env_var_values;
  char **env_var_names = mpibind_get_env_var_names(mph, &nvars);

  for (i=0; i<nvars; i++) {
    env_var_values = mpibind_get_env_var_values(mph, env_var_names[i]);
    if (env_var_values[taskid]) {
      //      fprintf(stderr, "%s: setting %s=%s\n", header,
      //	      env_var_names[i], env_var_values[taskid]);

      if (spank_setenv(sp, env_var_names[i], env_var_values[taskid], 1)
	  != ESPANK_SUCCESS) {
	slurm_error("mpibind: Failed to set %s in environment\n",
		    env_var_names[i]);
      }
    }
  }

#if 1
  clean_up(mph, topo);
#endif

  return ESPANK_SUCCESS;
}
