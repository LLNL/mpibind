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
 * Slurm items: 
 * 
 * S_TASK_ID: Local task id (int *)                       
 * S_JOB_LOCAL_TASK_COUNT: Number of local tasks (uint32_t *)     
 * S_JOB_NNODES: Total number of nodes in job (uint32_t *)
 * S_JOB_NODEID: Relative id of this node (uint32_t *) 
 * S_JOB_STEPID: Slurm job step id (uint32_t *) 
 *
 * S_JOB_ALLOC_CORES: Job allocated cores in list format (char **) 
 * S_STEP_ALLOC_CORES: Step alloc'd cores in list format  (char **)
 * S_JOB_NCPUS: Number of CPUs used by this job (uint16_t *)
 * S_STEP_CPUS_PER_TASK: CPUs allocated per task (=1 if --overcommit
 *                       option is used, uint32_t *)
 */ 


/* 
 * Spank plugin name and version 
 */ 
SPANK_PLUGIN(mpibind, 2); 


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
/* True if enabled via --mpibind=on */
static int opt_user_on = 0; 
/* True if disabled via plugstack.conf or --mpibind=off */ 
static int opt_disable = 0; 
/* Only set affinity if this job has exclusive access to this node.
   Set via plugstack.conf */
static int opt_exclusive_only = 0;

/* mpibind vars */ 
static mpibind_t *mph = NULL;
static hwloc_topology_t topo = NULL;

static
const char mpibind_help[] = 
  "\n\
Automatically map tasks/threads/GPU kernels to heterogeneous hardware\n\
\n\
Usage: --mpibind=[args]\n\
  \n\
where args is a comma separated list of one or more of the following:\n\
  gpu[:0|1]         Enable(1)/disable(0) GPU-optimized mappings\n\
  greedy[:0|1]      Allow(1)/disallow(0) multiple NUMAs per task\n\
  help              Display this message\n\
  off               Disable mpibind\n\
  on                Enable mpibind\n\
  smt:<k>           Enable worker use of SMT-<k>\n\
  v[erbose]         Print affinty for each task\n\
\n";


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

/*
 * Return 1 if the job has the nodes exclusively (not shared). 
 */ 
static
int job_is_exclusive(hwloc_topology_t topo)
{
  int cpus, pus;
  
  /* Total number of online CPUs */
  if ((cpus = (int) sysconf(_SC_NPROCESSORS_ONLN)) <= 0) {
    slurm_error("Failed to get number of processors: %m\n");
    return -1;
  }

  /* Number of allowed PUs */ 
  if ((pus = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_PU)) <= 0) {
    fprintf(stderr, "mpibind: Invalid #PUs=%d\n", pus);
    return 0;
  }

  if (opt_debug) 
    fprintf(stderr, "CPUs: total=%d allowed=%d\n", cpus, pus); 
  
  return (pus == cpus); 
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
  
  fprintf(stderr, str);
}
#endif 

static
void print_user_options()
{
  fprintf(stderr, "Options: enable=%d disable=%d user_on=%d excl_only=%d "
	  "verbose=%d debug=%d gpu=%d smt=%d greedy=%d \n",
	  opt_enable, opt_disable, opt_user_on, opt_exclusive_only, 
	  opt_verbose, opt_debug, opt_gpu, opt_smt, opt_greedy); 
}

static
int parse_option(const char *opt, int remote)
{  
  if (strcmp(opt, "off") == 0) {
    opt_disable = 1;
  }
  else if (strcmp(opt, "on") == 0) {
    opt_user_on = 1; 
  }
  else if (strncmp(opt, "gpu", 3) == 0) {
    opt_gpu = 1;
    /* Parse options if any */ 
    sscanf(opt+3, ":%d", &opt_gpu);
    if (opt_gpu < 0 || opt_gpu > 1)
      goto fail; 
  }
  else if (sscanf(opt, "smt:%d", &opt_smt) == 1) {
    if (opt_smt <= 0)
      goto fail; 
  }
  else if (strncmp(opt, "greedy", 6) == 0) {
    opt_greedy = 1;
    /* Parse options if any */
    sscanf(opt+6, ":%d", &opt_greedy);
    if (opt_greedy < 0 || opt_greedy > 1)
      goto fail;
  }
  else if (strcmp(opt, "verbose") == 0) {
    opt_verbose = 1;
  }
  else if (strncmp(opt, "v", 1) == 0) {
    opt_verbose = 1;
    /* Parse options if any */
    sscanf(opt+1, ":%d", &opt_verbose);
    if (opt_verbose < 0)
      goto fail;    
  }
  else if (strcmp(opt, "debug") == 0) {
    opt_debug = 1; 
  }
  else {
    fprintf(stderr, mpibind_help);
    exit(0);
  }
  
  return 0; 

 fail:
  slurm_error("mpibind: Invalid option: %s", opt);
  return -1;
}

static
int parse_user_options(int val, const char *arg, int remote)
{
  //fprintf(stderr, "mpibind option specified\n");
  
  const char delim[] = ","; 
  
  /* 'mpibind' with no args, enables the plugin */ 
  if (arg == NULL) {
    opt_enable = 1; 
    return 0;
  }
  
  /* Don't overwrite the original string */ 
  char *str = strdup(arg); 

  /* First token */ 
  char *token = strtok(str, delim);
  
  while (token != NULL) {
    //fprintf(stderr, "%s\n", token);
    parse_option(token, remote); 
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
    
    if (strcmp(argv[i], "off") == 0)
      opt_disable = 1;
    else if (strcmp(argv[i], "exclusive_only") == 0)
      opt_exclusive_only = 1;
    else { 
      fprintf(stderr, "mpibind: Invalid plugstack.conf argument %s\n",
	      argv[i]);
      return -1; 
    }
  }

  return 0;
}

static
uint32_t get_nnodes(spank_t sp)
{
  uint32_t nnodes = 0; 
  spank_err_t rc; 
  
  if ( (rc=spank_get_item(sp, S_JOB_NNODES, &nnodes)) !=
       ESPANK_SUCCESS ) 
    fprintf(stderr, "mpibind: Failed to get node count: %s",
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
    fprintf(stderr, "mpibind: Failed to get node id: %s",
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
    fprintf(stderr, "mpibind: Failed to get local task count: %s",
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
    fprintf(stderr, "mpibind: Failed to get local task id: %s",
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

static
int clean_up(mpibind_t *mph, hwloc_topology_t topo)
{
  int rc=0;
  
  if ((rc=mpibind_finalize(mph)) != 0)
    fprintf(stderr, "mpibind: mpibind_finalize failed\n");
  
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
  if (!spank_remote(sp))
    return ESPANK_SUCCESS; 
  
#if 0
  char name[] = "slurm_spank_init";
  print_context(name);
#endif
  
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
  if (!spank_remote(sp))
    return ESPANK_SUCCESS; 

#if 0
  char name[] = "slurm_spank_exit";
  print_context(name); 
#endif
  
  return ESPANK_SUCCESS;
}

/*
 * Remote context (slurmstepd)
 * Called once per node. 
 * 
 */
int slurm_spank_user_init(spank_t sp, int ac, char *argv[])
{
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
    if (opt_debug)
      fprintf(stderr, "Disabling mpibind for salloc/sbatch\n"); 
  }
  
#if 0
  char name[] = "slurm_spank_user_init"; 
  print_context(name);
#endif

  if (!opt_enable || opt_disable)
    return ESPANK_SUCCESS;

#if 1
  /* Allocate mpibind's global handle */ 
  if ( mpibind_init(&mph) != 0 || mph == NULL ) {
    slurm_error("mpibind_init");
    return ESPANK_ERROR;
  }
  //fprintf(stderr, "%s: mph=%p\n", name, mph);
  
  /* Allocate hwloc's topology handle */
  if ( hwloc_topology_init(&topo) < 0 ) { 
    slurm_error("hwloc_topology_init");
    return ESPANK_ERROR;
  }
#endif 
  
  char header[16];  
  uint32_t nodeid = get_nodeid(sp); 
  uint32_t nnodes = get_nnodes(sp);
  sprintf(header, "Node %d/%d", nodeid, nnodes);

  if (nodeid == 0 && opt_debug)
    print_user_options();
  
  uint32_t ntasks = get_local_ntasks(sp);
  int nthreads = get_omp_num_threads(sp);

  /* Set mpibind input parameters */ 
  if ( mpibind_set_ntasks(mph, ntasks) != 0 ||
       (nthreads > 0 && mpibind_set_nthreads(mph, nthreads) != 0) ||
       (opt_smt > 0 && mpibind_set_smt(mph, opt_smt) != 0) ||
       (opt_greedy >= 0 && mpibind_set_greedy(mph, opt_greedy) != 0) ||
       (opt_gpu >= 0 && mpibind_set_gpu_optim(mph, opt_gpu) != 0) ) {
    slurm_error("mpibind: Unable to set input parameters");
    return ESPANK_ERROR; 
  }

  /* 
   * Set the topology
   */
#if 1
  /* Use topology file if provided */ 
  char xml[512];
  xml[0] = '\0'; 
  if (spank_getenv(sp, "MPIBIND_TOPOFILE", xml, sizeof(xml))
      == ESPANK_SUCCESS && xml[0] != '\0') {
    
    if (hwloc_topology_set_xml(topo, xml) < 0)
      slurm_error("hwloc_topology_set_xml(%s)", xml);

    if (nodeid == 0 && opt_debug)
      fprintf(stderr, "mpibind: Loaded topology from %s\n", xml); 
  }

  /* Make sure OS binding functions are actually called */
  /* Could also use HWLOC_THISSYSTEM=1, but that applies
     globally to all hwloc clients */
  if (hwloc_topology_set_flags(topo, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM)
      < 0) {
    slurm_error("hwloc_topology_set_flags");
    return ESPANK_ERROR;
  }

  /* Make sure OS and PCI devices are not filtered out */
  if (mpibind_filter_topology(topo) < 0) {
    slurm_error("mpibind_filter_topology");
    return ESPANK_ERROR;
  }
  
  if (hwloc_topology_load(topo) < 0) {
    slurm_error("hwloc_topology_load");
    return ESPANK_ERROR;
  }

  if (hwloc_topology_is_thissystem(topo) == 0)
    slurm_error("mpibind: Binding will not be enforced");
  
  mpibind_set_topology(mph, topo);
#endif 

  /* Topology needs to be loaded already */ 
  int exclusive = job_is_exclusive(topo); 
  
  if (opt_debug)
    fprintf(stderr, "%s: ntasks=%d nthreads=%d greedy=%d gpu=%d "
	    "topo=%p, exclusive=%d\n",
	    header,
	    mpibind_get_ntasks(mph),
	    nthreads, 
	    mpibind_get_greedy(mph),
	    mpibind_get_gpu_optim(mph),
	    mpibind_get_topology(mph),
	    exclusive);

  if (!exclusive && opt_exclusive_only && !opt_user_on) {
    if (opt_debug)
      fprintf(stderr, "%s: Job doesn't have exclusive access to node\n",
	      header);
    
    opt_enable = 0;
    clean_up(mph, topo);
    
    return ESPANK_SUCCESS;
  }
  
  /*
   * Get the mpibind mapping!
   */
#if 1
  if (mpibind(mph) != 0) {
    slurm_error("mpibind()");
    return ESPANK_ERROR;
  }

  /* Set values of env variables for task_init */
  if (mpibind_set_env_vars(mph) != 0) {
    slurm_error("mpibind_set_env_vars");
    return ESPANK_ERROR;
  }
  
  if (opt_verbose) {
    char buf[1024];
    int ngpus = mpibind_get_num_gpus(mph);
    /* Use VISIBLE_DEVICES IDs to enumerate the GPUs
       since users are used to this enumeration
       (as opposed to mpibind's enumeration) */
    mpibind_set_gpu_ids(mph, MPIBIND_ID_VISDEVS);
    mpibind_mapping_snprint(buf, 1024, mph);
    if (nodeid == 0 || opt_verbose > 1) { 
      fprintf(stderr, "mpibind: %d GPUs on this node\n", ngpus);
      fprintf(stderr, "%s", buf);
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
  if (!spank_remote(sp))
    return ESPANK_SUCCESS; 

#if 0
  char name[] = "slurm_spank_task_init";
  print_context(name);
#endif 

  if (!opt_enable || opt_disable)
    return ESPANK_SUCCESS; 
  
  char header[16];  
  int taskid = get_local_taskid(sp); 
  uint32_t ntasks = get_local_ntasks(sp);
  sprintf(header, "Task %d/%d", taskid, ntasks);

  /* Bind this task to the calculated cpus */ 
  if (mpibind_apply(mph, taskid) != 0) {
    slurm_error("mpibind_apply");
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
	fprintf(stderr, "mpibind: Failed to set %s in environment\n",
		env_var_names[i]);
      }   
    }
  }

#if 1
  clean_up(mph, topo); 
  /* Clean up */ 
  /* mpibind_finalize(mph); */
  /* hwloc_topology_destroy(topo); */
#endif
  
  return ESPANK_SUCCESS;  
}

