#include <flux/shell.h>
#include <hwloc.h>
#include <jansson.h>
#include <mpibind.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG 0

/* mpibind.c - flux-shell mpibind plugin
 *
 *  flux-shell plugin to implement task binding and affinity using libmpibind.
 *
 *  OPTIONS
 *
 *  {
 *    "disable":i,
 *    "verbose":i,
 *    "smt":i,
 *    "greedy":i,
 *    "gpu_optim":i,
 *  }
 *
 *  e.g. to disable mpibind plugin run with `-o mpibind.disable`
 *
 *  OPERATION
 *
 *  Register with name "affinity" in flux_plugin_init() (see below) to override
 *  the shell's builtin affinity plugin. In shell.init callback, parse any
 *  relevant shell options, and if disabled is set, immediately return.
 *  Otherwise, gather number of local tasks and assigned core list and call
 *  mpibind(3) to generate mapping. Register for the 'task.exec' and
 *  'task.init' callback and pass the mpibind mapping to this function for use.
 */

/*  The name of this plugin. To completely replace the shell's internal
 *   affinity module, set to "affinity", otherwise choose a different name
 *   such as "mpibind".
 */
#define PLUGIN_NAME "affinity"

/*  Return task id for a shell task
 */
static int flux_shell_task_getid (flux_shell_task_t *task)
{
    int id = -1;
    if (flux_shell_task_info_unpack (task, "{ s:i }", "localid", &id) < 0)
        return -1;
    return id;
}

/*  Return the current task id when running in task.* context.
 */
static int get_taskid (flux_plugin_t *p)
{
    flux_shell_t *shell;
    flux_shell_task_t *task;

    if (!(shell = flux_plugin_get_shell (p)))
        return -1;
    if (!(task = flux_shell_current_task (shell)))
        return -1;
    return flux_shell_task_getid (task);
}

/*  Run hwloc_topology_restrict() with common flags for this module.
 */
static int topology_restrict (hwloc_topology_t topo, hwloc_cpuset_t set)
{
    if (hwloc_topology_restrict (topo, set, 0) < 0)
        return (-1);
    return (0);
}

/*  Restrict hwloc topology object to current processes binding.
 */
static int topology_restrict_current (hwloc_topology_t topo)
{
    int rc = -1;
    hwloc_bitmap_t rset = hwloc_bitmap_alloc ();
    if (!rset || hwloc_get_cpubind (topo, rset, HWLOC_CPUBIND_PROCESS) < 0)
        goto out;
    rc = topology_restrict (topo, rset);
out:
    if (rset)
        hwloc_bitmap_free (rset);

#if DEBUG
    int num = hwloc_get_nbobjs_by_type(topo, HWLOC_OBJ_OSDEV_GPU);
    fprintf(stderr, "Number of visible gpu devices: %d\n", num);
#endif

    return (rc);
}

/* Apply mpibind affinity for task `taskid`
 */
static int mpibind_apply (mpibind_t *mph, int taskid)
{
    hwloc_bitmap_t *core_sets = mpibind_get_cpus (mph);
    hwloc_topology_t topo = mpibind_get_topology (mph);

    if (hwloc_set_cpubind (topo, core_sets[taskid], 0) < 0)
        shell_log_errno ("hwloc_set_cpubind");
    return 0;
}

/* Set an environment variable for a task
 */
static int plugin_task_setenv (flux_plugin_t *p, const char *var, const char *val)
{
    flux_shell_t *shell = flux_plugin_get_shell (p);
    flux_shell_task_t *task = flux_shell_current_task (shell);
    flux_cmd_t *cmd = flux_shell_task_cmd (task);
    if (cmd)
        return flux_cmd_setenvf (cmd, 1, var, "%s", val);
    return 0;
}

/* task.init handler. Sets environment variables for each task based on
   mpibind results
 */
static int mpibind_task_init (flux_plugin_t *p,
                              const char *topic,
                              flux_plugin_arg_t *arg,
                              void *data)
{
    int num_names, i, taskid = get_taskid (p);
    mpibind_t *mph = data;
    char **env_var_values;
    char **env_var_names = mpibind_get_env_var_names (mph, &num_names);

    for (i = 0; i < num_names; i++) {
        env_var_values = mpibind_get_env_var_values (mph, env_var_names[i]);
        if (env_var_values[taskid]) {
#if DEBUG
            //fprintf (stderr, "%s\n", getenv ("SLURMD_NODENAME"));
            fprintf (stderr,
                     "Setting %s=%s\n",
                     env_var_names[i],
                     env_var_values[taskid]);
#endif
            plugin_task_setenv (p, env_var_names[i], env_var_values[taskid]);
        }
    }

    return 0;
}

/* task.exec handler. Enforces mpibind mappings for each task
 */
static int mpibind_task (flux_plugin_t *p,
                         const char *topic,
                         flux_plugin_arg_t *arg,
                         void *data)
{
    mpibind_t *mph = data;
    int taskid = get_taskid (p);
    if (taskid < 0) {
        shell_die (1, "failed to determine local taskid");
        return -1;
    }
    if (mpibind_apply (mph, taskid) < 0) {
        shell_die (1, "failed to apply mpibind affinity");
    }
    return 0;
}

static bool mpibind_getopt (flux_shell_t *shell,
                            int *psmt,
                            int *pgreedy,
                            int *pgpu_optim,
                            int *pverbose
                            )
{
    int rc, disabled = 0;
    char mpibind_params[512] = {'\0'};
    // pointer to match signature of flux_shell_getopt
    char* param_ptr = mpibind_params;
    json_t *mpibind_opts = NULL;
    json_error_t err;

    rc = flux_shell_getopt(shell, "mpibind", &param_ptr);
    if (rc == -1){
        shell_die_errno(1, "flux_shell_getopt");
        return 0;
    }
    else if (rc == 0){
        // mpibind disabled since it's not specified
        return 0; 
    }


    mpibind_opts = json_loads(mpibind_params, 0, &err);

    if(mpibind_opts){
        /* Take parameters from json */
        json_unpack_ex(mpibind_opts, &err, JSON_DECODE_ANY, 
                                    "{s?i s?i s?i s?i s?i}",
                                    "disable",
                                    &disabled,
                                    "smt",
                                    psmt,
                                    "greedy",
                                    pgreedy,
                                    "gpu_optim",
                                    pgpu_optim,
                                    "verbose",
                                    pverbose);
    }
    else{
        /* Parse series of parameters */
        char buf[512] = {'\0'};
        char *token, *value;
        strcpy(buf, param_ptr);

        token = strtok(buf, "\":");

        while( token != NULL){
        
            value = strtok(NULL, ",");

            if(!strcmp(token, "smt")){
                *psmt = atoi(value);
            }
            else if(!strcmp(token, "greedy")){
                *pgreedy = atoi(value);
            }
            else if(!strcmp(token, "gpu_optim")){
                *pgpu_optim = atoi(value);
            }
            else if(!strcmp(token, "verbose")){
                *pverbose = atoi(value);
            }
            else if(!strcmp(token, "disabled")){
                disabled = atoi(value);
            }
            else{
                shell_die(1, "Unknown mpibind parameters");
                return 0;
            }
            token = strtok(NULL, ":");
        }
    }

    return disabled == 0;
}

/* Destroy mpibind tasks and related variables
 */
static void mpibind_destroy (void *arg)
{
    mpibind_t *mph = arg;
    // topology isn't freed by mpibind_finalize()
    hwloc_topology_t topo = mpibind_get_topology (mph);
    hwloc_topology_destroy (topo);
    mpibind_finalize (mph);
}

/* Get the cpuset of the a set of cores given a topology
 */
static hwloc_cpuset_t derive_pus (hwloc_topology_t topo, char *cores)
{
    int depth, i;
    hwloc_cpuset_t coreset = NULL;
    hwloc_cpuset_t resultset = NULL;

    if (!(coreset = hwloc_bitmap_alloc ()) || !(resultset = hwloc_bitmap_alloc ())) {
        shell_log_errno ("hwloc_bitmap_alloc");
        goto err;
    }

    /*  Parse cpus as bitmap list
     */
    if (hwloc_bitmap_list_sscanf (coreset, cores) < 0) {
        shell_log_error ("affinity: failed to read core list: %s", cores);
        goto err;
    }

    /*  Find depth of type core in this topology:
     */
    depth = hwloc_get_type_depth (topo, HWLOC_OBJ_CORE);
    if (depth == HWLOC_TYPE_DEPTH_UNKNOWN || depth == HWLOC_TYPE_DEPTH_MULTIPLE) {
        shell_log_error ("hwloc_get_type_depth (CORE) returned nonsense");
        goto err;
    }

    /*  Get the union of all allocated cores' cpusets into sa->cpuset
     */
    i = hwloc_bitmap_first (coreset);
    while (i >= 0) {
        hwloc_obj_t core = hwloc_get_obj_by_depth (topo, depth, i);
        if (!core) {
            shell_log_error ("affinity: core%d not in topology", i);
            goto err;
        }
        if (!core->cpuset) {
            shell_log_error ("affinity: core%d cpuset is null", i);
            goto err;
        }
        hwloc_bitmap_or (resultset, resultset, core->cpuset);
        i = hwloc_bitmap_next (coreset, i);
    }
    hwloc_bitmap_free (coreset);
    return resultset;
err:
    if (coreset)
        hwloc_bitmap_free (coreset);
    if (resultset)
        hwloc_bitmap_free (resultset);
    return NULL;
}

static int mpibind_shell_init (flux_plugin_t *p,
                               const char *s,
                               flux_plugin_arg_t *arg,
                               void *data)
{
    int ntasks;
    char *cores, *gpus;
    int smt = 0;
    int greedy = 1;
    int gpu_optim = 1;
    int verbose = 0;
    mpibind_t *mph = NULL;
    hwloc_topology_t *topo = calloc (1, sizeof (hwloc_topology_t));

    flux_shell_t *shell = flux_plugin_get_shell (p);

    if (!mpibind_getopt (shell, &smt, &greedy, &gpu_optim, &verbose)) {
        shell_debug ("mpibind disabled");
        fprintf (stderr, "mpibind: disabled\n");
        return 0;
    }

    if (mpibind_init (&mph) != 0 || mph == NULL) {
        shell_die (1, "mpibind_init failed");
        return 0;
    }

    /*  Get number of local tasks and assigned cores from "rank info" object
     */
    if (flux_shell_rank_info_unpack (shell,
                                     -1,
                                     "{s:i, s:{s:s, s:s}}",
                                     "ntasks",
                                     &ntasks,
                                     "resources",
                                     "cores",
                                     &cores,
                                     "gpus",
                                     &gpus)
        < 0) {
        shell_die_errno (1, "flux_shell_rank_info_unpack");
        return -1;
    }

    // load topology
    hwloc_topology_init (topo);
    hwloc_topology_set_all_types_filter (*topo, HWLOC_TYPE_FILTER_KEEP_STRUCTURE);
    hwloc_topology_set_type_filter (*topo,
                                    HWLOC_OBJ_OS_DEVICE,
                                    HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
    hwloc_topology_load (*topo);
    topology_restrict_current (*topo);

    fprintf (stderr, "gpus: %s\n", gpus);

#if DEBUG
    fprintf (stderr,
             "num cores: %d\n",
             hwloc_get_nbobjs_by_type (*topo, HWLOC_OBJ_CORE));
    fprintf (stderr, "num pus: %d\n", hwloc_get_nbobjs_by_type (*topo, HWLOC_OBJ_PU));
#endif

    char *pu_set = calloc (256, sizeof (char));
    hwloc_cpuset_t resultset = derive_pus (*topo, cores);
    hwloc_bitmap_list_snprintf (pu_set, 256, resultset);

    /*  If mpibind is active, and is not named "affinity" (which already
     *   overrides built-in affinity plugin) disable cpu-affinity explicitly.
     */
    if (strcmp (PLUGIN_NAME, "affinity") != 0) {
        shell_debug ("disabling built-in affinity module");
        if (flux_shell_setopt_pack (shell, "cpu-affinity", "s", "off") < 0) {
            shell_die_errno (1, "flux_shell_setopt: cpu-affinity=off");
            return -1;
        }
    }
    
    /* Disable gpu-affinity */
    shell_debug ("disabling built-in gpu-affinity module");
    if (flux_shell_setopt_pack (shell, "gpu-affinity", "s", "off") < 0) {
        shell_die_errno (1, "flux_shell_setopt: gpu-affinity=off");
        return -1;
    }

    if (mpibind_set_ntasks (mph, ntasks) != 0 || mpibind_set_topology (mph, *topo) != 0
        || mpibind_set_restrict_ids (mph, pu_set) != 0
        || (smt >= 0 && mpibind_set_smt (mph, smt) != 0)
        || (greedy >= 0 && mpibind_set_greedy (mph, greedy) != 0)
        || (gpu_optim >= 0 && mpibind_set_gpu_optim (mph, gpu_optim) != 0)) {
        shell_log_errno ("Unable to set mpibind parameters");
        return -1;
    }

    /*  Set mpibind handle in shell aux data for auto-destruction
     */
    flux_shell_aux_set (shell, "mpibind", mph, (flux_free_f)mpibind_destroy);

    /*  Set handlers for 'task.exec' (called for each task before exec(2))
        and 'task.init'
     */
    if (flux_plugin_add_handler (p, "task.init", mpibind_task_init, mph) < 0
        || flux_plugin_add_handler (p, "task.exec", mpibind_task, mph) < 0) {
        shell_die_errno (1, "flux_plugin_add_handler");
        return -1;
    }

#if DEBUG
    fprintf (stderr,
             "input: ntasks=%d restrict=%s greedy=%d smt=%d gpu_optim=%d verbose=%d\n",
             ntasks,
             pu_set,
             greedy,
             smt,
             gpu_optim,
             verbose);
#endif

    if (mpibind (mph)) {
        shell_die_errno (1, "mpibind");
        return -1;
    }

    if (verbose) {
        mpibind_print_mapping(mph);
    }

    // set env variables now for the purposes of task.init
    mpibind_set_env_vars (mph);
    return 0;
}

static const struct flux_plugin_handler handlers[] = {
    {"shell.init", mpibind_shell_init, NULL},
    {NULL, NULL, NULL},
};

void flux_plugin_init (flux_plugin_t *p)
{
    fprintf (stderr, "mpibind init plugin\n");
    if (flux_plugin_register (p, PLUGIN_NAME, handlers) < 0)
        shell_die (1, "failed to register handlers");
}

/* vi: ts=4 sw=4 expandtab
 */
