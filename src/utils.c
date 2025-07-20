/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <hwloc.h>
#include "mpibind-priv.h"
#include "mpibind.h"


/************************************************
 * Functions defined in internals.c
 ************************************************/
char *trim(char *str); 


/************************************************
 * Utility functions provided by mpibind 
 * targeting developers rather than end users. 
 * End user functions are defined in mpibind.c
 ************************************************/

/*
 * Calculate the number of ints in a given range. 
 * Output is 0 when range is invalid. 
 * Example:
 *   Input range: 0-10,18
 *   Output: 12
 * If the input string contains a valid range, 
 * the string will be overwritten with it. 
 * Example: 
 *   Input: '  1,3,4-8 x'
 *   Ouput: '1,3,4-8'
 */
int mpibind_range_nints(char *arg)
{
  if (arg == NULL)
    return 0;

  const char name[] = "mpibind_range_nints"; 
  const char delim[] = ",";
  
  /* Don't overwrite the input string when parsing it */
  char *str = strdup(arg);
  
  /* First token */
  char *token = strtok(str, delim);

  /* Output string */ 
  int nc=0, size=strlen(arg)+1;
  char out[size];

  int num=0, rc, begin, end;
  while (token != NULL) {
    rc = sscanf(token, "%d-%d", &begin, &end);
    //PRINT("token=%s rc=%d begin=%d end=%d\n", token, rc, begin, end);
    
    if (rc == 2 && end >= begin) {
      num += end - begin + 1;
      nc += snprintf(out+nc, size-nc, "%d-%d", begin, end); 
    } else if (rc == 1) {
      num++;
      nc += snprintf(out+nc, size-nc, "%d", begin); 
    } else {
      //PRINT("%s: Invalid range '%s'\n", name, token);
      num = 0;
      goto outlab; 
    }
    
    token = strtok(NULL, delim);
    if (token != NULL)
      nc += snprintf(out+nc, size-nc, ","); 
  }

  /* Copy at most n *bytes* */ 
  strncpy(arg, out, size); 
  if (VERBOSE >= 1)
    PRINT("%s: %s -> %d\n", name, arg, num);

 outlab:
  free(str);
  return num;
}


/* 
 * Get the restrict CPU or NUMA IDs from the input string. 
 * If the string is a file path rather than an int range,
 * then look for the int range in the given file. 
 * 
 * Function returns 0 if a valid int rage is found
 * and 1 otherwise. 
 */ 
int mpibind_parse_restrict_ids(char *restr, int len)
{
  if (restr == NULL)
    return 1; 

  /* Check if this is a file path or an int range */
  if (mpibind_range_nints(restr) > 0)
    /* String has the restrict IDs */
    return 0; 
  
  /* String has the name of a file that contains the IDs */
  char *str = trim(restr);
  FILE *fp = fopen(str, "r");
  if (fp == NULL)
    return 1;
  
  int rc = 1; 
  char line[len];
  while ( feof(fp) == 0 ) {
    if (fgets(line, sizeof(line), fp) != NULL) {
      //printf("%lu: <%s>\n", strlen(str), str);
      
      if (mpibind_range_nints(line) > 0) {
	strncpy(restr, line, len);
	restr[len-1] = '\0';
	rc = 0;
	break; 
      }
    }
  }
  
  fclose(fp);
  return rc; 
}


/*
 * Parse mpibind plugin options
 * 
 * Return NULL when option is valid, 
 * otherwise a string with error message. 
 */
char* mpibind_parse_option(const char *opt,
			   int *debug,
			   int *gpu,
			   int *greedy,
			   int *master,
			   int *omp_places,
			   int *omp_proc_bind,
			   int *smt,
			   int *turn_on,
			   int *verbose,
			   int *visdevs)
{
  int rc = 0;
  
  if (strcmp(opt, "debug") == 0) {
    *debug = 1; 
  }
  else if (strncmp(opt, "gpu", 3) == 0) {
    *gpu = 1;
    /* Parse options if any */ 
    sscanf(opt+3, ":%d", gpu);
    if (*gpu < 0 || *gpu > 1)
      rc = 2;
  }
  else if (strncmp(opt, "greedy", 6) == 0) {
    *greedy = 1;
    /* Parse options if any */
    sscanf(opt+6, ":%d", greedy);
    if (*greedy < 0 || *greedy > 1)
      rc = 2;
  }
  else if (strncmp(opt, "h", 1) == 0) {
    rc = 1;
  }
  else if (strncmp(opt, "master", 6) == 0) {
    *master = 1;
    sscanf(opt+6, ":%d", master);
    if (*master < 0 || *master > 1)
      rc = 2;
  }
  else if (strcmp(opt, "off") == 0) {
    *turn_on = 0;
  }
  else if (strcmp(opt, "omp_places") == 0) {
    *omp_places = 1;
  }
  else if (strcmp(opt, "omp_proc_bind") == 0) {
    *omp_proc_bind = 1; 
  }
  else if (strcmp(opt, "on") == 0) {
    *turn_on = 1;
  }
  else if (sscanf(opt, "smt:%d", smt) == 1) {
    if (*smt <= 0)
      rc = 2;
  }
  else if (strcmp(opt, "verbose") == 0) {
    *verbose = 1;
  }
  else if (strcmp(opt, "visdevs") == 0) {
    *visdevs = 1;
  }
  else if (strncmp(opt, "v", 1) == 0) {
    *verbose = 1;
    /* Parse options if any */
    sscanf(opt+1, ":%d", verbose);
    if (*verbose < 0)
      rc = 2;
  }
  else {
    rc = 3;
  }

  if (rc > 0) {
    char *str = malloc(sizeof(char) * LONG_STR_SIZE);
    str[0] = '\0';
    
    if (rc == 1)
      snprintf(str, LONG_STR_SIZE, usage_str);
    else if (rc == 2)
      snprintf(str, LONG_STR_SIZE, "Invalid option value '%s'", opt);
    else if (rc == 3)
      snprintf(str, LONG_STR_SIZE, "Unknown option '%s'", opt);
    
    return str;
  } else
    return NULL; 
}


/*
 * Get the OS PU ids of a set of logical Core ids.
 */
int mpibind_cores_to_pus(hwloc_topology_t topo, char *cores,
			 char *pus, int pus_str_size)
{
  hwloc_bitmap_t core_set, pu_set;
  if ( !(core_set = hwloc_bitmap_alloc()) ||
       !(pu_set = hwloc_bitmap_alloc()) ) {
    PRINT("hwloc_bitmap_alloc failed\n");
    return 1;
  }

  /*  Parse cpus into a bitmap list */
  if ( hwloc_bitmap_list_sscanf(core_set, cores) < 0)  {
    PRINT("Failed to read core list: %s\n", cores);
    return 1;
  }
  
  /* Don't use HWLOC_OBJ_CORE directly:
     A flattened topology may not have objects of that type!
     Get the type or depth using mpibind functions */
  int core_depth = mpibind_get_core_depth(topo);

  hwloc_obj_t core;
  int i = hwloc_bitmap_first(core_set);
  while (i >= 0) {
    core = hwloc_get_obj_by_depth(topo, core_depth, i);
    if ( !core ) {
      PRINT("Logical core %d not in topology\n", i);
      return 1;
    }
    
    hwloc_bitmap_or(pu_set, pu_set, core->cpuset);
    i = hwloc_bitmap_next(core_set, i);
  }

  /* Write result as a string */
  hwloc_bitmap_list_snprintf(pus, pus_str_size, pu_set);

  hwloc_bitmap_free(pu_set);
  hwloc_bitmap_free(core_set);
  
  return 0;
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
char* mpibind_calc_restrict_cpus(hwloc_topology_t topo, char *cores,
				 const char *restr, const char *restr_type)
{
  /* Current model uses logical Cores to specify where this
     job should run. Need to get the OS cpus (including all
     the CPUs of an SMT core) to tell mpibind what it can use. */
  char *pus = malloc(LONG_STR_SIZE);

  /* Get the PUs associated with the given cores */ 
  if (mpibind_cores_to_pus(topo, cores, pus, LONG_STR_SIZE) != 0) {
    PRINT("cores_to_pus failed\n");
    return NULL;
  }

  PRINT("RM given cores: %s\n", cores);
  PRINT("Derived pus: %s\n", pus);
  PRINT("Total #cores: %d",
	hwloc_get_nbobjs_by_depth(topo,
				  mpibind_get_core_depth(topo)));
  PRINT("Total #pus: %d",
	hwloc_get_nbobjs_by_type(topo,
				 HWLOC_OBJ_PU));
  
  if (restr != NULL) {
    /* Need non-const pointer */
    char restr_str[LONG_STR_SIZE];
    strncpy(restr_str, restr, LONG_STR_SIZE-1);

    /* Get the restrict CPU or NUMA IDs directly or from a file */
    if (mpibind_parse_restrict_ids(restr_str, LONG_STR_SIZE) == 0) {
      PRINT("Restrict IDs read: <%s>", restr_str);
	    
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
      PRINT("Restrict IDs to PUs: <%s>", str3);

      /* Get the intersection of input pus and restrict var */  
      hwloc_bitmap_list_sscanf(genset, pus);
      hwloc_bitmap_and(genset, genset, cpuset);

      if ( !hwloc_bitmap_iszero(genset) )
	hwloc_bitmap_list_snprintf(pus, LONG_STR_SIZE, genset);
      else
	PRINT("Restrict yields empty set thus ignoring");

      hwloc_bitmap_free(genset);
      hwloc_bitmap_free(cpuset);
    }
  }

  return pus;
}


#if 0
int main(int argc, char *argv[])
{
  char str[40]; 
  strcpy(str, argv[1]);

  int rc, turn_on=-1, gpu=-1, smt=-1, greedy=-1, verb=-1, debug=-1;
  rc = mpibind_parse_option(str, &turn_on, &gpu, &smt, &greedy, &verb, &debug);

  printf("rc=%d\n turn_on=%d gpu=%d smt=%d greedy=%d verb=%d debug=%d\n",
	 rc, turn_on, gpu, smt, greedy, verb, debug); 
  
#if 0
  int rc = mpibind_get_restrict_ids(str, 40);
  printf("Out <%s> rc %d\n", str, rc);
#endif

  
  return 0;
}
#endif 




