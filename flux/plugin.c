#define FLUX_SHELL_PLUGIN_NAME "mpibind"

#include <stdio.h>
#include <stdlib.h>
#include <mpibind.h>
#include <flux/shell.h>
#include <jansson.h>

/* mpibind.c - flux-shell mpibind plugin
 *
 *  flux-shell plugin to implement task binding and affinity using libmpibind.
 *
 *  OPTIONS
 *
 *  {
 *    "on",
 *    "off",
 *    "verbose":int,
 *    "smt":int,
 *    "greedy":int,
 *    "gpu_optim":int,
 *    "master":int
 *  }
 *
 * Examples:
 *   Disable mpibind plugin: '-o mpibind=off'
 *   Enable SMT2 and verbosity: '-o mpibind=smt:2,verbose:1'
 *   Enable debugging messages: '-o verbose'
 *
 *  OPERATION
 *
 *  In shell.init callback, parse any
 *  relevant shell options, and if disabled is set, immediately return.
 *  Otherwise, gather number of local tasks and assigned core list and call
 *  mpibind(3) to generate mapping. Register for the 'task.exec' and
 *  'task.init' callback and pass the mpibind mapping to this function for use.
 */

/*
 * Flux output options:
 * verbose: Only the task.exec callback is called from the task,
 *          after fork(2) but before exec(2). This is the one
 *          callback from which a shell plugin can print to stdout/
 *          stderr (fflush needed if stdout) and it will be folded
 *          in with the output of the task (since the plugin is
 *          running within the forked task).
 *          For output other than the task.exec callback,
 *          use shell_log().
 * debug:   Use shell_debug (verbose level 1) or
 *          shell_trace (verbose level 2).
 *
 */

/*
 * Todo:
 *
 * mpibind functions:
 *   mpibind_set_restrict_ids: take in hwloc_bitmap_t
 *   mpibind_get_env_var_names(mph, &nvars): don't include
 *     variables that have no values, e.g., VISIBLE_DEVICES
 * Flux plugin
 *   - work on verbose/debug options.
 *       show mapping, number of gpus, etc.
 */

/*  The name of this plugin. To completely replace the shell's internal
 *   affinity module, set to "affinity", otherwise choose a different name
 *   such as "mpibind". If you don't overwrite the affinity module, you'll
 *   have to disable the module from within this plugin (see mpibind_init)
 */
#define PLUGIN_NAME FLUX_SHELL_PLUGIN_NAME
#define LONG_STR_SIZE 2048
#define MAX_NUMA_DOMAINS 32
#define PRINT_MAP_BUF_SIZE (1024*5)

/*
 * Structure to pass parameters from flux_plugin_init()
 * to mpibind_shell_init().
 */
struct usr_opts {
  int smt;
  int greedy;
  int gpu_optim;
  int verbose;
  int master;
  int omp_proc_bind;
  int omp_places;
  int visible_devices;
};

/*
 * Structure to pass parameters from flux_plugin_add_handler()
 * to mpibind_task_init()
 */
struct handle_and_opts {
  mpibind_t *mph;
  struct usr_opts *opts;
};

/*  Return task id for a shell task
 */
static
int flux_shell_task_getid(flux_shell_task_t *task)
{
    int id = -1;
    if (flux_shell_task_info_unpack(task, "{ s:i }", "localid", &id) < 0)
        return -1;

    return id;
}

/*  Return the current task id when running in task.* context.
 */
static
int get_taskid(flux_plugin_t *p)
{
    flux_shell_t *shell;
    flux_shell_task_t *task;

    if ( !(shell = flux_plugin_get_shell(p)) )
        return -1;
    if ( !(task = flux_shell_current_task(shell)) )
        return -1;

    return flux_shell_task_getid(task);
}

/*
 * Set an environment variable for a task.
 */
static
int plugin_task_setenv(flux_plugin_t *p, const char *var, const char *val)
{
  flux_shell_t *shell = flux_plugin_get_shell(p);
  flux_shell_task_t *task = flux_shell_current_task(shell);
  flux_cmd_t *cmd = flux_shell_task_cmd(task);

  if (cmd)
    return flux_cmd_setenvf(cmd, 1, var, "%s", val);

  return 0;
}

/*
 * Handler for task.init.
 * Sets environment variables for each task based on the mpibind mapping.
 */
static
int mpibind_task_init(flux_plugin_t *p, const char *topic,
		       flux_plugin_arg_t *arg, void *data)
{
  int nvars, i;
  char **env_var_values;

  //mpibind_t *mph = data;
  struct handle_and_opts *hdl = data;
  //todo: hdl->opts

  int taskid = get_taskid(p);
  char **env_var_names = mpibind_get_env_var_names(hdl->mph,
						   &nvars);

  for (i=0; i<nvars; i++) {
    /* Tell mpibind to not set a variable */
    if ( (!strcmp(env_var_names[i], "OMP_PLACES") &&
	  hdl->opts->omp_places) ||
	 (!strcmp(env_var_names[i], "OMP_PROC_BIND") &&
	  hdl->opts->omp_proc_bind) ||
	 (strstr(env_var_names[i], "VISIBLE_DEVICES") &&
	  hdl->opts->visible_devices) )
      continue;

    env_var_values = mpibind_get_env_var_values(hdl->mph,
						env_var_names[i]);
    if (env_var_values[taskid]) {
      shell_debug("task %2d: setting %s=%s\n", taskid,
		  env_var_names[i], env_var_values[taskid]);
      plugin_task_setenv(p, env_var_names[i], env_var_values[taskid]);
    }
  }

  return 0;
}

/*
 * Printing mpibind's mapping:
 * To get rid of the shell_log() prefix, e.g.,
 * "0.211s: flux-shell[0]: mpibind:", I could print the mapping
 * out in the task.exec callback, but this would require changes:
 * 1. Create a structure with the mpibind handle and a pointer
 *    to the user opts structure.
 * 2. Pass a pointer to this new structure to the task.exec callback
 *    instead of the mpibind handle. Don't free the user opts struct yet.
 * 3. Pass a pointer to this new structure to the flux primitive that
 *    registers the mpibind_destroy function. Modify mpibind_destroy
 *    accordingly so that it frees the user opts structure.
 * 4. In the task.exec callback use mpibind_print_mapping_task
 *    to print the mapping for the task based on the value of
 *    opt->verbose.
 */

/*
 * Handler for task.exec.
 * Applies mpibind mappings for each task.
 */
static
int mpibind_task(flux_plugin_t *p, const char *topic,
		  flux_plugin_arg_t *arg, void *data)
{
  mpibind_t *mph = data;
  int taskid = get_taskid(p);

  if (taskid < 0) {
    shell_die(1, "failed to determine local taskid");
    return -1;
  }

  /* Can't print the mapping here without reading the
     user option 'verbose' (see notes above) */
#if 0
  if (taskid == 0) {
    char outbuf[LONG_STR_SIZE];
    mpibind_snprint_mapping(mph, outbuf, sizeof(outbuf));
    fprintf(stderr, "%s", outbuf);
  }
#endif

  if (mpibind_apply(mph, taskid) < 0) {
    shell_die(1, "failed to apply mpibind affinity");
  }

  return 0;
}

/*
 * Parse mpibind options from the command line
 * either as JSON or a short syntax.
 */
static
bool mpibind_getopt(flux_shell_t *shell,
		    int *psmt, int *pgreedy, int *pgpu_optim,
		    int *pverbose, int *pmaster,
		    int *pomp_proc_bind, int *pomp_places,
		    int *pvisible_devices)
{
  int rc;
  int disabled = 0;
  char *json_str = NULL;
  json_t *opts = NULL;
  json_error_t err;

  rc = flux_shell_getopt(shell, "mpibind", &json_str);
  if (rc < 0) {
    shell_die_errno(1, "flux_shell_getopt");
    return false;
  } else if (rc == 0) {
    /* '-o mpibind' not given, enable by default */
    return true;
  }
  opts = json_loads(json_str, 0, &err);

  if ( opts )
    /* Take parameters from json */
    json_unpack_ex(opts, &err, JSON_DECODE_ANY,
		   "{s?i s?i s?i s?i s?i}",
		   "smt", psmt,
		   "greedy", pgreedy,
		   "gpu_optim", pgpu_optim,
		   "verbose", pverbose,
		   "master", pmaster);
  else
    /* Check if options were given to mpibind.
       If no options, proceed with default parameters */
    if ( strcmp(json_str, "1") != 0 ) {
      /* Options given to mpibind. Parse short syntax */
      int len;
      char str[LONG_STR_SIZE];
      char *token, *end_str;

      /* The json string is encompased in quotes,
	 e.g., "verbose,smt:2"
	 Don't copy the quotes */
      //strcpy(str, json_str);
      len = strlen(json_str) - 2;
      memcpy(str, json_str+1, len);
      str[len] = '\0';
      //shell_debug("str=%s str_len=%ld json_len=%ld",
      //	  str, strlen(str), strlen(json_str));

      /* Get the first comma-separated token */
      token = strtok_r(str, ",", &end_str);

      char *msg;
      int debug=-1, turn_on=-1;

      while (token != NULL) {
	//shell_debug("token = %s", token);
	msg = mpibind_parse_option(token,
				   &debug,
				   pgpu_optim,
				   pgreedy,
				   pmaster,
				   pomp_places,
				   pomp_proc_bind,
				   psmt,
				   &turn_on,
				   pverbose,
				   pvisible_devices);

	if (msg)
	  /* Todo:
	     Terminate the shell without printing the message twice */
	  shell_die(1, "%s", msg);

	/* Get the next comma-separated token */
	token = strtok_r(NULL, ",", &end_str);
      }

      if (turn_on != -1)
	/* Flip boolean value */
	disabled = (turn_on + 1) % 2;
    }

  /* Clean up */
  json_decref(opts);
  free(json_str);

  return disabled == 0;
}

/*
 * Free mpibind resources.
 * hwloc topology isn't freed by mpibind_finalize.
 */
static
void mpibind_destroy(void *arg)
{
  //mpibind_t *mph = arg;
  struct handle_and_opts *hdl = arg;
  hwloc_topology_t topo = mpibind_get_topology(hdl->mph);

  mpibind_finalize(hdl->mph);
  hwloc_topology_destroy(topo);

  free(hdl->opts);
  free(hdl);
}

/*
 * Distribute workers over domains
 * Input:
 *   wks: number of workers
 *  doms: number of domains
 * Output: wk_arr of length doms
 */
#if 0
static
void distrib(int wks, int doms, int *wk_arr) {
  int i, avg, rem;

  avg = wks / doms;
  rem = wks % doms;

  for (i=0; i<doms; i++)
    if (i < rem)
      wk_arr[i] = avg+1;
    else
      wk_arr[i] = avg;
}
#endif

#if 0
static
void print_array(int *arr, int size, char *label)
{
  int i, nc=0;
  char str[LONG_STR_SIZE];

  for (i=0; i<size; i++)
    nc += snprintf(str+nc, sizeof(str)-nc, "[%d]=%d ", i, arr[i]);

  shell_debug("%s: %s\n", label, str);
}
#endif

/*  Restrict hwloc topology to the cpu affinity mask of the current
 *  proces. This is required for handling nested jobs in Flux, since
 *  the nested job will be bound to a subset of total resources, but
 *  assigned cores are "reranked" to zero-origin (i.e. a Flux job
 *  using logical cores 3,4 will remap these cores to 0-1 locally.
 *
 *  This function restricts the hwloc topology to cores 3,4 so that
 *  logical cores 0,1 now correspond to OS logical cores 3,4.
 */
static
int topo_restrict(hwloc_topology_t topo)
{
  hwloc_bitmap_t rset = NULL;
  int rc = -1;
  if (!(rset = hwloc_bitmap_alloc())
      || hwloc_get_cpubind (topo, rset, HWLOC_CPUBIND_PROCESS) < 0
      || hwloc_topology_restrict (topo, rset,
				  HWLOC_RESTRICT_FLAG_REMOVE_CPULESS) < 0)
    goto out;
  rc = 0;
out:
  hwloc_bitmap_free (rset);
  return rc;
}

/*
 *
 * Returns a string with the PUs that need to be used to
 * restrict the topology. It takes into account the values of
 * enviroment variables as indicated by 'restr' and 'restr_type'
 * as well as the 'cores' the RM has allocated for this job.
 *
 * The output string needs to be freed.
 */
static
char* calc_restrict_cpus(hwloc_topology_t topo, char *cores,
			 const char *restr, const char *restr_type)
{
  /* Current model uses logical Cores to specify where this
     job should run. Need to get the OS cpus (including all
     the CPUs of an SMT core) to tell mpibind what it can use. */
  char *pus = malloc(LONG_STR_SIZE);

  /* Get the PUs associated with the given cores */
  if (mpibind_cores_to_pus(topo, cores, pus, LONG_STR_SIZE) != 0) {
    shell_debug("cores_to_pus failed");
    return NULL;
  }

  shell_debug("Flux given cores: %s", cores);
  shell_debug("Derived pus: %s", pus);
  shell_debug("Total #cores: %d",
	hwloc_get_nbobjs_by_depth(topo,
				  mpibind_get_core_depth(topo)));
  shell_debug("Total #pus: %d",
	hwloc_get_nbobjs_by_type(topo,
				 HWLOC_OBJ_PU));

  if (restr != NULL) {
    /* Need non-const pointer */
    char restr_str[LONG_STR_SIZE];
    strncpy(restr_str, restr, LONG_STR_SIZE-1);

    /* Get the restrict CPU or NUMA IDs directly or from a file */
    if (mpibind_parse_restrict_ids(restr_str, LONG_STR_SIZE) == 0) {
      shell_debug("Restrict IDs read: <%s>", restr_str);

      hwloc_bitmap_t genset = hwloc_bitmap_alloc();
      hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();

      /* Calculate the restrict CPU set */
      if (restr_type != NULL && strcmp(restr_type, "mem") == 0) {
	hwloc_bitmap_list_sscanf(genset, restr_str);
	hwloc_cpuset_from_nodeset(topo, cpuset, genset);
      } else {
	hwloc_bitmap_list_sscanf(cpuset, restr_str);
      }

      char str3[LONG_STR_SIZE];
      hwloc_bitmap_list_snprintf(str3, sizeof(str3), cpuset);
      shell_debug("Restrict IDs to PUs: <%s>", str3);

      /* Get the intersection of input pus and restrict var */
      hwloc_bitmap_list_sscanf(genset, pus);
      hwloc_bitmap_and(genset, genset, cpuset);

      if ( !hwloc_bitmap_iszero(genset) )
	hwloc_bitmap_list_snprintf(pus, LONG_STR_SIZE, genset);
      else
	shell_debug("Restrict yields empty set thus ignoring");

      hwloc_bitmap_free(genset);
      hwloc_bitmap_free(cpuset);
    }
  }

  return pus;
}

/*
 * The entry function to the mpibind plugin.
 * This function initializes and calls mpibind.
 */
static
int mpibind_shell_init(flux_plugin_t *p, const char *s,
		       flux_plugin_arg_t *arg, void *data)
{
  int i, ntasks;
  char *cores, *gpus;
  hwloc_topology_t topo;
  mpibind_t *mph = NULL;
  struct usr_opts *opts = data;
  bool restrict_topo = true;
  const char *xml;
  flux_shell_t *shell = flux_plugin_get_shell(p);

  if ( mpibind_init(&mph) != 0 || mph == NULL ) {
    shell_die(1, "mpibind_init failed");
    return 0;
  }

  /* Flux design current model for custom binding needs improvement.
     Possible options:
     1. The current model is to pass logical core IDs of the cores
        where the job runs. But, what if the resources assigned to
        a job are the second hardware threads of each SMT core?
        Another problem occurs when Flux gives a set of cores and a
        set of GPUs that are not in the same domain. In this case
        mpibind will be restricted to the hardware domain "reachable"
        by the cores and if the assigned GPUs are not in that domain
        then mpibind cannot see them.
     2. Pass a list of OS cpu ids. These can be Cores or PUs, it does
        not matter.
     3. Have Flux bind the job (not individual workers) to the assigned
        resources. Let plugins discover what resources are available and
	map workers using their custom logic.
	This can be accomplish with Linux 'tuna', 'cgroups' or with
        soft bindings like hwloc's.
	But this would depend on who calls the plugins, some process
        outside of the assigned resources? If so, this would not work.
  */

  /*  Get number of local tasks and assigned cores from "rank info" object */
  if (flux_shell_rank_info_unpack(shell,
				   -1,
				   "{s:i, s:{s:s, s:s}}",
				   "ntasks", &ntasks,
				   "resources",
				   "cores", &cores,
				   "gpus", &gpus) < 0) {
    shell_die_errno(1, "flux_shell_rank_info_unpack");
    return -1;
  }

  if (hwloc_topology_init(&topo) < 0)
    return shell_log_errno("hwloc_topology_init");

  /* Prefer loading topology from the Flux job shell, since this XML has
   * already been restricted to the resources available to this job, and
   * also avoids the need for a topo file or loading HWLOC directly.
   *
   * Fall back to MPIBIND_TOPOFILE if getting the topology fails or
   * FLUX_MPIBIND_USE_TOPOFILE is set in the job environment.
   */
  if (!flux_shell_getenv(shell, "FLUX_MPIBIND_USE_TOPOFILE")
      && flux_shell_get_hwloc_xml(shell, &xml) == 0) {

    if (flux_shell_get_hwloc_xml(shell, &xml) < 0)
      return shell_log_errno("failed to get hwloc XML from job shell");

    shell_debug("Loaded topology from job shell");
    if (hwloc_topology_set_xmlbuffer(topo, xml, strlen (xml)) < 0)
      return shell_log_errno ("hwloc_topology_set_xmlbuffer");

    xml = "shell-provided";

    /* No need to further restrict topology since this is already done
     * in the topology XML fetched from the job shell.
     */
    restrict_topo = false;
  }
  else {
    /*
     * The fetch of HWLOC XML from the job shell failed: fall back to
     * MPIBIND_TOPOFILE if set, if not, then hwloc topology will be loaded
     * directly from the system (slow).
     */
    xml = flux_shell_getenv(shell, "MPIBIND_TOPOFILE");
    if ((xml = flux_shell_getenv(shell, "MPIBIND_TOPOFILE"))
        && xml[0] != '\0') {
      if (hwloc_topology_set_xml(topo, xml) < 0)
        return shell_log_errno("hwloc_topology_set_xml(%s)", xml);
      shell_debug("Loaded topology from %s", xml);
    }
  }

#if 1
  if (mpibind_load_topology(topo) != 0)
    return shell_log_errno("mpibind_load_topology");
#else
  /* Make sure the OS binding functions are actually called */
  /* Could also use HWLOC_THISSYSTEM=1, but that applies
     globally to all hwloc clients */
  if (hwloc_topology_set_flags(topo, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM) < 0)
    return shell_log_errno("hwloc_topology_set_flags");

  /* Make sure OS and PCI devices are not filtered out */
  if (mpibind_filter_topology(topo) < 0)
    return shell_log_errno("mpibind_filter_topology");

  if (hwloc_topology_load(topo) < 0)
    return shell_log_errno("hwloc_topology_load");

  if (hwloc_topology_is_thissystem(topo) == 0) {
    shell_log_error("Binding is not enforced");
    return 1;
  }
#endif

  if (restrict_topo && topo_restrict (topo) < 0)
    return shell_log_errno ("Failed to restrict topology");

  shell_debug("Flux given gpus: <%s>", gpus);

  /*
   * One may restrict the PUs/NUMAs where the application runs.
   * Among others, useful for thread or core specialization.
   */
  char *pus =
    calc_restrict_cpus(topo, cores,
		       flux_shell_getenv(shell,
					 "MPIBIND_RESTRICT"),
		       flux_shell_getenv(shell,
					 "MPIBIND_RESTRICT_TYPE"));

  if ( mpibind_set_ntasks(mph, ntasks) != 0 ||
       mpibind_set_topology(mph, topo) != 0 ||
       (opts->master <= 0 && mpibind_set_restrict_ids(mph, pus) != 0) ||
       (opts->smt >= 0 && mpibind_set_smt(mph, opts->smt) != 0) ||
       (opts->greedy >= 0 && mpibind_set_greedy(mph, opts->greedy) != 0) ||
       (opts->gpu_optim >= 0 && mpibind_set_gpu_optim(mph, opts->gpu_optim) != 0) ) {
    shell_log_errno("Unable to set mpibind parameters");
    return -1;
  }

  /* Tell mpibind the user set the number of threads */
  int nthreads = 0;
  const char *str = flux_shell_getenv(shell, "OMP_NUM_THREADS");
  if (str != NULL) {
    nthreads = atoi(str);
    if (nthreads > 0)
      mpibind_set_nthreads(mph, nthreads);
  }

  shell_debug("user opts: ntasks=%d nthreads=%d restrict=%s "
	      "greedy=%d smt=%d gpu_optim=%d verbose=%d master=%d "
	      "visible_devices=%d omp_proc_bind=%d omp_places=%d "
	      "xml=%s ",
	      ntasks, nthreads, pus, opts->greedy, opts->smt,
	      opts->gpu_optim, opts->verbose, opts->master,
	      opts->visible_devices,
	      opts->omp_proc_bind, opts->omp_places, xml);

  struct handle_and_opts *hdl = malloc(sizeof(struct handle_and_opts));
  hdl->mph = mph;
  hdl->opts = opts;

  /* Set mpibind handle in shell aux data for auto-destruction */
  flux_shell_aux_set(shell, "mpibind", hdl, (flux_free_f) mpibind_destroy);

  /* Set handlers for 'task.exec', called for each task before exec(2),
     and 'task.init' */
  if (flux_plugin_add_handler(p, "task.init", mpibind_task_init, hdl) < 0
      || flux_plugin_add_handler(p, "task.exec", mpibind_task, mph) < 0) {
    shell_die_errno(1, "flux_plugin_add_handler");
    return -1;
  }

  /* Get the mpibind mapping! */
  if (mpibind(mph) != 0) {
    shell_die_errno(1, "mpibind");
    return -1;
  }

  /* Debug: Print out the cpus assigned to each task */
  char outbuf[PRINT_MAP_BUF_SIZE];
  hwloc_bitmap_t *cpus = mpibind_get_cpus(mph);
  for (i=0; i<ntasks; i++) {
    hwloc_bitmap_list_snprintf(outbuf, PRINT_MAP_BUF_SIZE, cpus[i]);
    shell_debug("task %2d: cpus %s", i, outbuf);
  }

  if (opts->verbose) {
    /* Can't print to stdout within the Flux shell environment */
    //mpibind_print_mapping(mph);

    /* Use VISIBLE_DEVICES IDs to enumerate the GPUs
       since users are used to this enumeration
       (as opposed to mpibind's enumeration) */
    mpibind_set_gpu_ids(mph, MPIBIND_ID_SMI);
    mpibind_mapping_snprint(outbuf, PRINT_MAP_BUF_SIZE, mph);
    shell_log("\n%s", outbuf);
  }

  /* Set env variables now for the purposes of task.init */
  if (mpibind_set_env_vars(mph) != 0) {
    shell_die_errno(1, "mpibind_set_env_vars");
    return -1;
  }

  /* Clean up */
  free(pus);
  /* Can't free opts here since it will be used by mpibind_task_init */
  //free(opts);

  return 0;
}

#if 0
static
const struct flux_plugin_handler handlers[] = {
    { "shell.init", mpibind_shell_init, NULL },
    { NULL, NULL, NULL },
};

void flux_plugin_init (flux_plugin_t *p)
{
    if (flux_plugin_register (p, PLUGIN_NAME, handlers) < 0)
        shell_die (1, "failed to register handlers");
}
#endif

/*
 * Register the mpibind plugin.
 * Notes:
 *   - Register the entry point: mpibind_shell_init
 *   - Disable conflicting plugins, e.g., cpu-affinity, here.
 *     Doing it later would not have an effect.
 *   - Do not disable conflicting plugins if mpibind was set to off.
 *     This forces parsing the mpibind user options here.
 */
void flux_plugin_init(flux_plugin_t *p)
{
  if ( !p )
    shell_die_errno(EINVAL, "flux_plugin_init: NULL input");

  if ( flux_plugin_set_name(p, PLUGIN_NAME) )
    shell_die(1, "flux_plugin_set_name");

  flux_shell_t *shell = flux_plugin_get_shell(p);

  if ( !shell )
    shell_die_errno(1, "flux_plugin_get_shell");

  struct usr_opts *opts = malloc(sizeof(struct usr_opts));
  /* mpibind parameters
     When value is -1, use mpibind default value */
  opts->smt = -1;
  opts->greedy = -1;
  opts->gpu_optim = -1;
  /* flux plugin parameters */
  opts->verbose = 0;
  // master = 0: Stay within flux-given node resources.
  // master = 1: mpibind takes all the resources of a node.
  //
  // Default is master=0: Overwritting Flux behavior may
  // have unexpected consequences.
  // This option is actually not necessary, because Flux
  // users should use '--exclusive' in their run commands
  // if they want mpibind to apply to all of the resources
  // of a node. But, I'm keeping this option just in case.
  opts->master = 0;
  /* By default mpibind sets the environment variables, i.e.,
     (do not disable setting the variables) */
  opts->omp_proc_bind = 0;
  opts->omp_places = 0;
  opts->visible_devices = 0;

  /* Get mpibind user-specified options */
  if ( !mpibind_getopt(shell,
		       &opts->smt,
		       &opts->greedy,
		       &opts->gpu_optim,
		       &opts->verbose,
		       &opts->master,
		       &opts->omp_proc_bind,
		       &opts->omp_places,
		       &opts->visible_devices) ) {
    shell_debug("mpibind disabled");
    return;
  }

  if ( flux_plugin_add_handler(p, "shell.init", mpibind_shell_init, opts) < 0 )
    shell_die(1, "failed to register shell.init handler");

  /* I could remove debug messages since the built-in cpu and gpu
     affinity plugins will log when they are disabled */
  shell_debug("disabling cpu-affinity");
  if (flux_shell_setopt_pack(shell, "cpu-affinity", "s", "off") < 0)
    shell_die_errno(1, "flux_shell_setopt_pack: cpu-affinity=off");

  shell_debug("disabling gpu-affinity");
  if (flux_shell_setopt_pack(shell, "gpu-affinity", "s", "off") < 0)
    shell_die_errno(1, "flux_shell_setopt_pack: gpu-affinity=off");
}
