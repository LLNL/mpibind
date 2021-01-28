/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory 
 ******************************************************/

#include <stdio.h>
#include <hwloc.h>

#define SHORT_STR_SIZE 32
#define LONG_STR_SIZE 1024


#if 0
struct topo_level {
  char *name;
  int nobjs;
  int depth;
  int pus_per_obj; 
  hwloc_bitmap_t cpuset; 
};

static
void get_level_info(hwloc_topology_t topo,
		    hwloc_cpuset_t cpuset, int depth, 
		    int pus_per_obj, struct topo_level *info)
{
  int i, nobjs, pu, count, nc; 
  hwloc_obj_t obj;
  char *str; 

  info->cpuset = hwloc_bitmap_alloc();
  info->name = malloc(SHORT_STR_SIZE);
  info->nobjs = 0;
  info->depth = depth;
  info->pus_per_obj = pus_per_obj; 
  str = info->name; 
  
  nobjs = hwloc_get_nbobjs_inside_cpuset_by_depth(topo, cpuset, depth);
  
  /* Get the pus for the given depth and cpuset */ 
  for (i=0; i<nobjs; i++) {
    obj = hwloc_get_obj_inside_cpuset_by_depth(topo, cpuset, depth, i);
    
    count = 0; 
    hwloc_bitmap_foreach_begin(pu, obj->cpuset) {
      hwloc_bitmap_set(info->cpuset, pu);
      info->nobjs++; 
      if (++count == pus_per_obj)
	break;
    } hwloc_bitmap_foreach_end(); 
  }
  
  /* Get the name of the matching level */ 
  nc = hwloc_obj_type_snprintf(str, SHORT_STR_SIZE, obj, 1);
  if (depth == hwloc_get_type_depth(topo, HWLOC_OBJ_CORE))
    snprintf(str+nc, SHORT_STR_SIZE-nc, "SMT%d", pus_per_obj); 
}

static
void free_topo_levels(struct topo_level *levels, int size)
{
  int i;

  for (i=0; i<size; i++) {
    free(levels[i].name);
    hwloc_bitmap_free(levels[i].cpuset); 
  }

  free(levels); 
}

/* 
 * While I can walk down the tree to find out what level 
 * is to be used for given ntasks/nthreads, I would have
 * special cases since the topology does not have SMT-2, 
 * SMT-3, etc. levels. 
 * Thus, this function creates my own level structure that 
 * I can use to manage any request uniformly. 
 */
static
struct topo_level* build_mapping_levels(hwloc_topology_t topo,
					hwloc_obj_t root,
					int *size)
{
  int idx, topo_depth, depth, hw_smt, smt, core_depth, nlevels;
  struct topo_level *topo_levels; 

  hw_smt = get_smt_level(topo); 
  topo_depth = hwloc_topology_get_depth(topo);
  core_depth = hwloc_get_type_depth(topo, HWLOC_OBJ_CORE);
  
  nlevels = topo_depth - root->depth;
  if (hw_smt > 2)
    nlevels += hw_smt - 2;
  topo_levels = calloc(nlevels, sizeof(struct topo_level));
  
  /* Walk the tree starting from the root */
  idx = 0; 
  for (depth=root->depth; depth<topo_depth; depth++) {
    get_level_info(topo, root->cpuset, depth, 1, topo_levels+idx); 
    idx++; 

    /* Build SMT levels starting at the Core+1 level */ 
    if (depth == core_depth) 
      for (smt=2; smt<hw_smt; smt++) {
	get_level_info(topo, root->cpuset, depth, smt, topo_levels+idx);
	idx++; 
      }
  }
  
  *size = idx; 
  return topo_levels; 
}
#endif

/* 
 * Get the PCI Bus ID of an I/O device object, e.g., 
 * 0000:04:00.0
 * Make sure PCI devices are not filtered out
 * when loading an hwloc topology. 
 */
int pci_busid_snprintf(char *buf, size_t size, hwloc_obj_t io)
{
  hwloc_obj_t obj; 

  if (io->type == HWLOC_OBJ_PCI_DEVICE)
    obj = io; 
  else if (io->parent->type == HWLOC_OBJ_PCI_DEVICE)
    obj = io->parent; 
  else
    return 0; 

  return snprintf(buf, size, "%04x:%02x:%02x.%01x", 
      obj->attr->pcidev.domain, obj->attr->pcidev.bus, 
      obj->attr->pcidev.dev, obj->attr->pcidev.func);
}

/* 
 * Get the Universally Unique ID (UUID) of a GPU device.
 * 
 * The GPU UUID is located in the info attributes of the 
 * associated OSDEV_GPU device (rather than OSDEV_COPROC). 
 * The backend for NVIDIA is NVML and for AMD is RSMI.  
 * NVML GPU OS devices
 *     NVIDIAUUID, NVIDIASerial 
 *     NVML is the NVIDIA Management Library and is included in CUDA.
 * RSMI GPU OS devices
 *     AMDUUID, AMDSerial 
 *     RSMI is the AMD ROCm SMI library
 */
int gpu_uuid_snprintf(char *buf, size_t size, hwloc_obj_t dev)
{
  int nc=0; 
  hwloc_obj_t obj=NULL; 

  if (dev->type == HWLOC_OBJ_OS_DEVICE && 
      dev->attr->osdev.type == HWLOC_OBJ_OSDEV_GPU)
        obj = dev;
  else if (dev->parent->io_arity > 0) {
    hwloc_obj_t io = dev->parent->io_first_child;
    do {
      if (io->type == HWLOC_OBJ_OS_DEVICE && 
          io->attr->osdev.type == HWLOC_OBJ_OSDEV_GPU)
        obj = io; 
    } while ((io = io->next_sibling) != NULL);
  }

  if (obj) {
    nc = snprintf(buf, size, "%s", 
      hwloc_obj_get_info_by_name(obj, "AMDUUID")); 
    if (nc <= 0)
      nc = snprintf(buf, size, "%s", 
        hwloc_obj_get_info_by_name(obj, "NVIDIAUUID")); 
  }

  return nc; 
}

/* 
 * Get the 'name=value' pairs of the infos structure  
 * of an object. 
 * struct hwloc_info_s ∗ infos
 *   char*name
 *   char*value
 * unsigned infos_count
 */
static 
int obj_infos_snprintf(char *str, size_t size, hwloc_obj_t obj)
{
  int i, nc=0;
  
  for (i=0; i<obj->infos_count; i++) 
    nc += snprintf(str+nc, size-nc, "%s=%s ",
		   obj->infos[i].name, obj->infos[i].value);

  return nc; 
} 

/*
 * Get the attributes of a normal or I/O object. 
 * Verbosity can be zero or greater than zero. 
 * Todo: Add memory objects (HWLOC_OBJ_NUMA_NODE)
 */
static
int obj_attr_snprintf(char *str, size_t size, hwloc_obj_t obj, 
                      int verbose)
{
  int nc=0; 

  /* Other object attributes:
     obj->depth, obj->gp_index, obj->logical_index,
     obj->attr->pcidev.vendor_id, obj->attr->pcidev.device_id
     hwloc_pci_class_string(obj->attr->pcidev.class_id) */ 
  
  if (hwloc_obj_type_is_normal(obj->type)) {
    nc += hwloc_obj_type_snprintf(str+nc, size-nc, obj, 1);
    nc += snprintf(str+nc, size-nc, "[%d]: os=%d ",
      obj->logical_index, obj->os_index);
    nc += snprintf(str+nc, size-nc, "cpuset=");
    nc += hwloc_bitmap_list_snprintf(str+nc, size-nc, obj->cpuset);
    nc += snprintf(str+nc, size-nc, " nodeset=");
    nc += hwloc_bitmap_list_snprintf(str+nc, size-nc, obj->nodeset);
    nc += snprintf(str+nc, size-nc, " arity=%d mema=%d ioa=%d ", 
      obj->arity, obj->memory_arity, obj->io_arity);
  }

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
      
    case HWLOC_OBJ_OSDEV_GPU : 
      nc += hwloc_obj_type_snprintf(str+nc, size-nc, obj, 1);
      nc += snprintf(str+nc, size-nc, ": name=%s ", obj->name);
      nc += snprintf(str+nc, size-nc, "UUID=%s ", 
        (hwloc_obj_get_info_by_name(obj, "AMDUUID")) ? 
          hwloc_obj_get_info_by_name(obj, "AMDUUID") : 
          hwloc_obj_get_info_by_name(obj, "NVIDIAUUID")); 
      nc += snprintf(str+nc, size-nc, "   ");
      nc += hwloc_obj_attr_snprintf(str+nc, size-nc, obj, " ", verbose);
      break; 

    case HWLOC_OBJ_OSDEV_OPENFABRICS :
      nc += hwloc_obj_type_snprintf(str+nc, size-nc, obj, 1);
      nc += snprintf(str+nc, size-nc, ": name=%s ", obj->name);
      nc += snprintf(str+nc, size-nc, "NodeGUID=%s ",
        hwloc_obj_get_info_by_name(obj, "NodeGUID"));     
      nc += snprintf(str+nc, size-nc, "   ");
      nc += hwloc_obj_attr_snprintf(str+nc, size-nc, obj, " ", verbose);
      break; 
    default: 
      break;
  }

  if (obj->type == HWLOC_OBJ_PCI_DEVICE) {
    /* Filter out less interesting devices:
       0x106: SATA
       0x200: Ethernet 
       0x300: VGA
       More interesting devices: 
       0x108: NVMe memory
       0x207: InfiniBand NICs
       0x302: NVIDIA GPUs
       0x380: AMD GPUs */ 
    unsigned cid = obj->attr->pcidev.class_id; 
    if (cid != 0x300 && cid != 0x200 && cid != 0x106) {
      nc += hwloc_obj_type_snprintf(str+nc, size-nc, obj, 1);
      nc += snprintf(str+nc, size-nc, ": busid=");
      nc += pci_busid_snprintf(str+nc, size-nc, obj);
      nc += snprintf(str+nc, size-nc, " link=%.2fGB/s ",
        obj->attr->pcidev.linkspeed); 
      nc += snprintf(str+nc, size-nc, "class=0x%x ",
        obj->attr->pcidev.class_id);
      nc += snprintf(str+nc, size-nc, "PCIDevice=%s ",
        hwloc_obj_get_info_by_name(obj, "PCIDevice")); 
      nc += snprintf(str+nc, size-nc, "   ");
      nc += hwloc_obj_attr_snprintf(str+nc, size-nc, obj, " ", verbose);
    }
  }

  return nc;
}

/* 
 * Walk the I/O objects stemming from 'root'
 * and apply function 'apply' to each object. 
 * 
 * Applying this function to the topology root 
 * object (Machine) may not show all the I/O devices, 
 * because most of them are hanging off of a Package,
 * not Machine. To see all the I/O objects use 
 * 'print_topo_io'
 */
static
void tree_walk_io_simple(void (*apply)(hwloc_obj_t, int), 
                         hwloc_obj_t root, int depth)
{
  hwloc_obj_t obj; 

  (*apply)(root, depth); 
  
  if (root->io_arity > 0) {
    obj = root->io_first_child; 
    do {
      tree_walk_io_simple(apply, obj, depth+1); 
    } while ((obj = obj->next_sibling) != NULL);
  }
}

/*
 * A generalization of tree_walk_io() that allows 
 * a function with arguments to be applied to 
 * each object 
 */
void tree_walk_io(void (*apply)(hwloc_obj_t, void*, int), 
                  hwloc_obj_t root, void *args, int depth)
{
  hwloc_obj_t obj; 

  (*apply)(root, args, depth); 
  
  if (root->io_arity > 0) {
    obj = root->io_first_child; 
    do {
      tree_walk_io(apply, obj, args, depth+1); 
    } while ((obj = obj->next_sibling) != NULL);
  }
}


/*
 * Print the 'name=value' pairs of the infos structure  
 * of an object. 
 */ 
void print_obj_info(hwloc_obj_t obj) {
  char str[LONG_STR_SIZE];
  if ( obj_infos_snprintf(str, sizeof(str), obj) )
    printf("%s\n", str);
}

/* 
 * Print the object and its attributes. 
 * Use 'indent' (non-negative) to indent the output. 
 */ 
void print_obj(hwloc_obj_t obj, int indent)
{
  int verb = 0; 
  char str[LONG_STR_SIZE];
  if (obj_attr_snprintf(str, sizeof(str), obj, verb) > 0)
    printf("%*s%s\n", 2*indent, "", str);
}

/*
 * Print devices of a specific type as a flat list.  
 * Types:
 * HWLOC_OBJ_PCI_DEVICE
 * HWLOC_OBJ_OS_DEVICE
 * HWLOC_OBJ_DIE
 * HWLOC_OBJ_NUMA_NODE
 * HWLOC_OBJ_GROUP
 * HWLOC_OBJ_CORE, ... 
 */
void print_devices(hwloc_topology_t topo, hwloc_obj_type_t type)
{
  hwloc_obj_t obj = NULL; 
  while ( (obj=hwloc_get_next_obj_by_type(topo, type, obj)) != NULL )
    print_obj(obj, 0);  
}

/*
 * A quick view of the topology of the normal 
 * components, i.e., no I/O devices. 
 */
void print_topo_brief(hwloc_topology_t topo)
{
  int depth;
  int topo_depth = hwloc_topology_get_depth(topo); 
  
  for (depth = 0; depth < topo_depth; depth++)
    printf("Depth %d: %s[%d]\n", depth,
	   hwloc_obj_type_string(hwloc_get_depth_type(topo, depth)), 
	   hwloc_get_nbobjs_by_depth(topo, depth)); 
}  

/*
 * Print the hierarchy of I/O objects of interest, 
 * including non-I/O ancestors, of a given topology. 
 * Objects of interest include Coprocessors, 
 * PCI devices, GPUs, and OpenFabrics NICs. 
 */ 
void print_topo_io(hwloc_topology_t topo)
{
  hwloc_uint64_t nonio_gp[128];
  int i, nonio_cnt=0; 
  hwloc_obj_t obj=NULL, parent; 
  
  while ( (obj = hwloc_get_next_osdev(topo, obj)) != NULL )
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC ||
        obj->attr->osdev.type == HWLOC_OBJ_OSDEV_GPU ||
        obj->attr->osdev.type == HWLOC_OBJ_OSDEV_OPENFABRICS) {
      /* Find the non-io ancestor first */ 
      parent = hwloc_get_non_io_ancestor_obj(topo, obj);

      /* Check if we've seen this normal object before */ 
      for (i=0; i<nonio_cnt; i++)
        if (nonio_gp[i] == parent->gp_index)
          break;
      /* If not, print it out with its I/O children */ 
      if (i == nonio_cnt) { 
        nonio_gp[nonio_cnt++] = parent->gp_index; 
        tree_walk_io_simple(print_obj, parent, 0);
      }
    }
}  
