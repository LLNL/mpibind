/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 ******************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hwloc.h>

#include <sys/wait.h>
#include <unistd.h>

#ifdef HAVE_AMD_GPUS
#include "hip/hip_runtime.h"
#endif 

#define MAX_PCI_LEN 20
#define MAX_STR_LEN 512


int obj_attr_snprintf(char *str, size_t size, hwloc_obj_t obj, 
                      int verbose)
{
  int nc=0; 

  if (obj->type == HWLOC_OBJ_OS_DEVICE)
  switch (obj->attr->osdev.type) {
    case HWLOC_OBJ_OSDEV_COPROC :
      nc += hwloc_obj_type_snprintf(str+nc, size-nc, obj, 1);
      nc += snprintf(str+nc, size-nc, ": name=%s ", obj->name);
      nc += snprintf(str+nc, size-nc, "subtype=%s ", obj->subtype);
      nc += snprintf(str+nc, size-nc, "GPUModel=%s ", 
        hwloc_obj_get_info_by_name(obj, "GPUModel"));
      nc += snprintf(str+nc, size-nc, "   ");
      /* Get obj->infos in one shot */ 
      nc += hwloc_obj_attr_snprintf(str+nc, size-nc, obj, " ", verbose);
      break; 

    default: 
      break;
  }

  
  return nc;
}
 


void set_vis_devs(char *str)
{
  // Don't invoke any GPU calls before resetting the environment!
  // Otherwise, there's no effect of setting VISIBLE_DEVICES. 
  //cudaGetDeviceCount(&ndevs);
  //printf("Initial num. devices %d\n", ndevs); 

  printf("Resetting environment to devices %s\n", str); 
  unsetenv("ROCR_VISIBLE_DEVICES");
  unsetenv("HIP_VISIBLE_DEVICES");
  unsetenv("CUDA_VISIBLE_DEVICES");
#ifdef HAVE_AMD_GPUS
    setenv("ROCR_VISIBLE_DEVICES", str, 1);
#else 
    setenv("CUDA_VISIBLE_DEVICES", str, 1);
#endif
}


void print_devices(hwloc_topology_t topo)
{
  char str[MAX_STR_LEN]; 
  hwloc_obj_t obj = NULL;
  while ( (obj=hwloc_get_next_obj_by_type(topo, HWLOC_OBJ_OS_DEVICE, obj)) != NULL ) 
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
      str[0] = '\0'; 
      obj_attr_snprintf(str, MAX_STR_LEN, obj, 0);
      printf("%s\n", str);
  }
}

int get_list_len(char *lst)
{
  // Copy VISDEVS string since strtok modifies the input string 
  char tmp[strlen(lst)]; 
  strcpy(tmp, lst);

  /* Get list size */ 
  int idevs = 0; 
  char *token = strtok(tmp, ",");
  while( token != NULL ) {
    idevs++; 
    token = strtok(NULL, ",");
  }

  return idevs; 
}


void test_wdup(char *visdevs, hwloc_topology_t topo)
{
  set_vis_devs(visdevs); 

  hwloc_topology_t topo2; 
  printf("Duplicating the topology\n"); 
  hwloc_topology_dup(&topo2, topo); 

  set_vis_devs(visdevs); 

  print_devices(topo2);
  hwloc_topology_destroy(topo2);    
}

void test_wfork(char *vds)
{
  set_vis_devs(vds); 
  pid_t cpid = fork();
  
  if (cpid == 0) {
    unsetenv("ROCR_VISIBLE_DEVICES");
    unsetenv("HIP_VISIBLE_DEVICES");
    printf("Child:\n");
    set_vis_devs(vds); 

    hwloc_topology_t topo;
    hwloc_topology_init(&topo);
    hwloc_topology_set_io_types_filter(topo,
        HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
    hwloc_topology_load(topo);
    print_devices(topo);
    hwloc_topology_destroy(topo);    

    exit(0);
  } else if (cpid > 0) {
    printf("Parent: Nothing to do but wait...\n");
    wait(NULL);
  } else {
    printf("fork() failed\n");
  }
}

void test_wnew_topo(char *vds)
{
  set_vis_devs(vds); 

  hwloc_topology_t topo;
  hwloc_topology_init(&topo);
  hwloc_topology_set_io_types_filter(topo,
      HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  hwloc_topology_load(topo);
  print_devices(topo);
  hwloc_topology_destroy(topo);    
}


void test_wdev_api(char *vds)
{
  int i, odevs=-1; 
  /* Cannot call the device driver before settting 
     VISIBLE DEVICES. Otherwise, the devices are set
     and cannot be changed */ 
  //cudaGetDeviceCount(&odevs);
  //printf("Modified num. devices %d\n", odevs); 

  set_vis_devs(vds); 
  cudaGetDeviceCount(&odevs);
  printf("Modified num. devices %d\n", odevs); 

  /* Get device PCI ID */ 
  char pci[MAX_PCI_LEN]; 
  for (i=0; i<odevs; i++) {
      pci[0] = '\0'; 
      cudaDeviceGetPCIBusId(pci, MAX_PCI_LEN, i);
      printf("PCI ID of device %d = %s\n", i, pci);
  }
}

void test_wfork_api(char *vds)
{
  int i, odevs=-1; 
  /* Don't call into device functions until 
     after setting visible devices */ 
  //cudaGetDeviceCount(&odevs);
  //printf("Num. devices %d\n", odevs); 

  set_vis_devs(vds); 
  cudaGetDeviceCount(&odevs);
  printf("Num. devices %d\n", odevs); 

  pid_t cpid = fork();
  
  if (cpid == 0) {
    unsetenv("ROCR_VISIBLE_DEVICES");
    unsetenv("HIP_VISIBLE_DEVICES");
    printf("Child:\n");
    set_vis_devs(vds); 

    cudaGetDeviceCount(&odevs);
    printf("Num. devices %d\n", odevs); 
    /* Get device PCI ID */ 
    char pci[MAX_PCI_LEN]; 
    for (i=0; i<odevs; i++) {
      pci[0] = '\0'; 
      cudaDeviceGetPCIBusId(pci, MAX_PCI_LEN, i);
      printf("PCI ID of device %d = %s\n", i, pci);
    }
    
    exit(0);
  } else if (cpid > 0) {
    printf("Parent: Nothing to do but wait...\n");
    wait(NULL);
  } else {
    printf("fork() failed\n");
  }
}



/* Lessons learned: 
   1. Setting VISIBLE DEVICES in the context of hwloc: 
      The environmnet variables must be set before the 
      first time the topology is loaded. 
   2. Setting VISIBLE DEVICES in the context of device API calls: 
      The environment variables must be called before the 
      first invocation of a device function. 
   3. Using fork does not really allows to overwrite the points
      above. 
   4. hwloc loading a topology has the same effect as calling 
      a device function, i.e., after this setting VISIBLE 
      DEVICES is too late. 
 */ 

int main(int argc, char *argv[])
{
  char vds[] = "1"; 
  //int idevs = get_list_len(vds); 

  hwloc_topology_t topo;
  hwloc_topology_init(&topo);
  /* OS devices are filtered by default, enable to see GPUs */
  hwloc_topology_set_type_filter(topo, HWLOC_OBJ_OS_DEVICE,
      HWLOC_TYPE_FILTER_KEEP_IMPORTANT);
  /* Include PCI devices to determine whether two GPUs                          
     are the same device, i.e., opencl1d1 and cuda1 */
  hwloc_topology_set_type_filter(topo, HWLOC_OBJ_PCI_DEVICE,
       HWLOC_TYPE_FILTER_KEEP_IMPORTANT);

  /* Setting visible devices must be done before 
     loading the topology the first time! */ 
  set_vis_devs(vds); 

  /* If testing whether VISIBLE DEVICES work with 
     the device API functions, don't load the topology
     because this set the devices and can't be changed later */ 
  hwloc_topology_load(topo);
  //print_devices(topo);


#if 1
  test_wnew_topo(vds); 
#endif 
#if 0
  test_wdup(vds, topo);
#endif
#if 0
  test_wfork(vds);
#endif
#if 0
  test_wdev_api(vds);
#endif
#if 0
  test_wfork_api(vds);
#endif 

  hwloc_topology_destroy(topo);

  return 0; 
}