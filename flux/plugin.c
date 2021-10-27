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
 *    "disable":int,
 *    "verbose":int,
 *    "smt":int,
 *    "greedy":int,
 *    "gpu_optim":int,
 *  }
 *
 *  e.g. to disable mpibind plugin run with `-o mpibind.disable`
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

/* Todo list: 
 * mpibind extra functions:
 *   mpibind_set_restrict_ids (take in hwloc_bitmap_t)
 *   mpibind_get_env_var_names(mph, &nvars): don't include 
 *     variables that have no values, e.g., VISIBLE_DEVICES
 * Flux plugin 
 *   - work on verbose/debug options.
 *       show mapping, number of gpus, etc. 
 *   - test gpus (use new test program in 'affinity') 
 */ 

/*  The name of this plugin. To completely replace the shell's internal
 *   affinity module, set to "affinity", otherwise choose a different name
 *   such as "mpibind". If you don't overwrite the affinity module, you'll 
 *   have to disable the module from within this plugin (see mpibind_init)
 */
#define PLUGIN_NAME FLUX_SHELL_PLUGIN_NAME

#define LONG_STR_SIZE 1024


/*  Return task id for a shell task
 */
static int flux_shell_task_getid(flux_shell_task_t *task)
{
    int id = -1;
    if (flux_shell_task_info_unpack(task, "{ s:i }", "localid", &id) < 0)
        return -1;
    
    return id;
}

/*  Return the current task id when running in task.* context.
 */
static int get_taskid(flux_plugin_t *p)
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
  
  mpibind_t *mph = data;
  int taskid = get_taskid(p);
  char **env_var_names = mpibind_get_env_var_names(mph, &nvars);
  
  for (i=0; i <nvars; i++) {
    env_var_values = mpibind_get_env_var_values(mph, env_var_names[i]);
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
 * 3. Pass a pointer to this new structure to the flux primite that 
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
		     int *pverbose)
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
		   "{s?i s?i s?i s?i}",
		   "smt", psmt,
		   "greedy", pgreedy,
		   "gpu_optim", pgpu_optim,
		   "verbose", pverbose);
  else
    /* Check if options were given to mpibind. 
       If no options, proceed with default parameters */ 
    if ( strcmp(json_str, "1") != 0 ) {
      /* Options given to mpibind. Parse short syntax */ 
      char *token, *value;
      char buf[LONG_STR_SIZE];
      strcpy(buf, json_str);

      /* The json string starts with double quotes, e.g., "smt:2" */
      token = strtok(buf, "\":");
      
      /* On/off parameters should not be used in conjuction with 
	 any other parameters */ 
      while ( token != NULL ) {
	value = strtok(NULL, ",");
	
	if ( !strcmp(token, "smt") ) 
	  *psmt = atoi(value);
	else if ( !strcmp(token, "greedy") ) 
	  *pgreedy = atoi(value);
	else if ( !strcmp(token, "gpu_optim") ) 
	  *pgpu_optim = atoi(value);
	else if ( !strcmp(token, "verbose") )
	  *pverbose = atoi(value);
	else if ( !strcmp(token, "off") )
	  disabled = 1; 
	else if ( !strcmp(token, "on") )
	  disabled = 0;
	else {
	  shell_die(1, "Unknown mpibind parameter '%s'", token);
	  return false;
	}
	token = strtok(NULL, ":");
      }
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
  mpibind_t *mph = arg;
  hwloc_topology_t topo = mpibind_get_topology(mph);
  
  mpibind_finalize(mph);
  hwloc_topology_destroy(topo);
}

/* 
 * Get the OS PU ids of a set of logical Core IDs. 
 */
static
int get_pus_of_lcores(hwloc_topology_t topo, char *lcores, char *pus)
{
  int depth, i;
  hwloc_obj_t core; 
  hwloc_bitmap_t lcore_set, pu_set;
  
  if ( !(lcore_set = hwloc_bitmap_alloc()) ||
       !(pu_set = hwloc_bitmap_alloc()) ) { 
    shell_log_errno("hwloc_bitmap_alloc");
    return 1;
  }
  
  /*  Parse cpus as bitmap list */
  if ( hwloc_bitmap_list_sscanf(lcore_set, lcores) < 0)  {
    shell_log_error("Failed to read core list: %s", lcores);
    return 1; 
  }

  depth = hwloc_get_type_depth(topo, HWLOC_OBJ_CORE);
  if ( depth == HWLOC_TYPE_DEPTH_UNKNOWN ) {
    shell_log_error("Core depth is HWLOC_TYPE_DEPTH_UNKNOWN");
    return 1; 
  }
  if ( depth == HWLOC_TYPE_DEPTH_MULTIPLE ) {
    shell_log_error("Core depth is HWLOC_TYPE_DEPTH_MULTIPLE");
    return 1; 
  }

  /* Find the logical Cores and aggregate their PUs */ 
  i = hwloc_bitmap_first(lcore_set);
  while (i >= 0) {
    
    core = hwloc_get_obj_by_depth(topo, depth, i);
    if ( !core ) {
      shell_log_error("Logical core %d not in topology", i);
      return 1; 
    }
    if ( !core->cpuset ) {
      shell_log_error("Logical core %d cpuset is null", i);
      return 1; 
    }
    
    hwloc_bitmap_or(pu_set, pu_set, core->cpuset);
    i = hwloc_bitmap_next(lcore_set, i);
  }

  /* Write result as a string */ 
  hwloc_bitmap_list_snprintf(pus, LONG_STR_SIZE, pu_set);
  
  hwloc_bitmap_free(lcore_set);
  hwloc_bitmap_free(pu_set);
  
  return 0; 
}

/* 
 * Structure to pass parameters from flux_plugin_init() 
 * to mpibind_shell_init(). 
 */ 
struct usr_opts {
  int smt;
  int greedy;
  int gpu_optim;
  int verbose; 
}; 

/* 
 * The entry function to the mpibind plugin. 
 * This function initializes and calls mpibind. 
 */ 
static
int mpibind_shell_init(flux_plugin_t *p, const char *s,
		       flux_plugin_arg_t *arg, void *data)
{
  int ntasks; 
  char *cores, *gpus, *pus;
  hwloc_topology_t topo;
  mpibind_t *mph = NULL;
  struct usr_opts *opts = data; 
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

  /* Make sure OS and PCI devices are not filtered out */ 
  if ( mpibind_filter_topology(topo) < 0 )
    return shell_log_errno("mpibind_filter_topology");
  
  if (hwloc_topology_load(topo) < 0)
    return shell_log_errno("hwloc_topology_load");
  

  /* Current model uses logical Cores to specify where this 
     job should run. Need to get the OS cpus (including all 
     the CPUs of an SMT core) to tell mpibind what it can use. */ 
  pus = malloc(LONG_STR_SIZE);
  if (get_pus_of_lcores(topo, cores, pus) != 0) {
    shell_log_error("get_pus_of_lcores failed\n");
    return -1; 
  }

  shell_debug("flux given cores: %s\n", cores);
  shell_debug("\tderived pus: %s\n", pus); 
  shell_debug("flux given gpus: %s", gpus);
  shell_debug("# available cores: %d",
	       hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_CORE));
  shell_debug("# available pus: %d",
	       hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_PU));
  
  if ( mpibind_set_ntasks(mph, ntasks) != 0 ||
       mpibind_set_topology(mph, topo) != 0 ||
       mpibind_set_restrict_ids(mph, pus) != 0 ||
       (opts->smt >= 0 && mpibind_set_smt(mph, opts->smt) != 0) ||
       (opts->greedy >= 0 && mpibind_set_greedy(mph, opts->greedy) != 0) ||
       (opts->gpu_optim >= 0 && mpibind_set_gpu_optim(mph, opts->gpu_optim) != 0) ) {
    shell_log_errno("Unable to set mpibind parameters");
    return -1;
  }

  shell_debug("user opts: ntasks=%d restrict=%s greedy=%d smt=%d "
	      "gpu_optim=%d verbose=%d",
	      ntasks, pus, opts->greedy, opts->smt,
	      opts->gpu_optim, opts->verbose);
    
  /* Set mpibind handle in shell aux data for auto-destruction */
  flux_shell_aux_set(shell, "mpibind", mph, (flux_free_f) mpibind_destroy);
  
  /* Set handlers for 'task.exec', called for each task before exec(2), 
     and 'task.init' */
  if (flux_plugin_add_handler(p, "task.init", mpibind_task_init, mph) < 0
      || flux_plugin_add_handler(p, "task.exec", mpibind_task, mph) < 0) {
    shell_die_errno(1, "flux_plugin_add_handler");
    return -1;
  }  
  
  /* Get the mpibind mapping! */ 
  if (mpibind(mph) != 0) {
    shell_die_errno(1, "mpibind");
    return -1;
  }
 
  if (opts->verbose) {
    /* Can't print to stdout within the Flux shell environment */ 
    //mpibind_print_mapping(mph);
    char outbuf[LONG_STR_SIZE]; 
    /* Use VISIBLE_DEVICES IDs to enumerate the GPUs
       since users are used to this enumeration 
       (as opposed to mpibind's enumeration) */ 
    mpibind_set_gpu_ids(mph, MPIBIND_ID_VISDEVS);
    mpibind_mapping_snprint(outbuf, LONG_STR_SIZE, mph);
    shell_log("\n%s", outbuf);
  }
  
  /* Set env variables now for the purposes of task.init */
  if (mpibind_set_env_vars(mph) != 0) {
    shell_die_errno(1, "mpibind_set_env_vars");
    return -1;
  }
  
  /* Clean up */
  free(pus);
  free(opts); 
  
  return 0;
}

#if 0
static const struct flux_plugin_handler handlers[] = {
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
 *     Doing later would not have an effect. 
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
  opts->smt = -1;
  opts->greedy = -1;
  opts->gpu_optim = -1;
  opts->verbose = 0;
  
  /* Get mpibind user-specified options */ 
  if ( !mpibind_getopt(shell, &opts->smt, &opts->greedy,
		       &opts->gpu_optim, &opts->verbose) ) {
    shell_debug("mpibind disabled");
    return;
  }
  
  if ( flux_plugin_add_handler(p, "shell.init", mpibind_shell_init, opts) < 0 )
    shell_die(1, "failed to register shell.init handler");
  
  /* Todo: Remove debug messages since the the built-in cpu and gpu 
     affinity plugins will log when they are disabled */ 
  shell_debug("mpibind: disabling cpu-affinity");
  if (flux_shell_setopt_pack(shell, "cpu-affinity", "s", "off") < 0)
    shell_die_errno(1, "flux_shell_setopt_pack: cpu-affinity=off");
  
  shell_debug("mpibind: disabling gpu-affinity");
  if (flux_shell_setopt_pack(shell, "gpu-affinity", "s", "off") < 0) 
    shell_die_errno(1, "flux_shell_setopt_pack: gpu-affinity=off");
}

