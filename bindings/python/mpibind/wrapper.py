import os
import re

from _mpibind._mpibind import ffi, lib

class MpibindResourceAssignment():
    def __init__(self, resource_type, resource_string):
        self.type = resource_type
        self.assignment = resource_string

    @property
    def assignment(self):
        return self.__assignment
    
    @assignment.setter
    def assignment(self, assignment):
        self.__assignment = assignment
        self.count = self._parse_count(assignment)

    def _parse_count(self, assignment):
        if not assignment:
            return 0

        count = 0
        for item in assignment.split(","):
            count += 1
            if "-" in item:
                start, end = item.split("-")
                count += (int(end) - int(start))

        return count

    def __repr__(self):
        return f"{self.type}: {self.assignment}"

class MpibindTaskAssignment():
    def __init__(self, cpus, gpus, thread_count):
        self.cpus = MpibindResourceAssignment("cpus", cpus)
        self.gpus = MpibindResourceAssignment("gpus", gpus)
        self.smt = thread_count

    def __repr__(self):
        return f"smt: {self.smt} {repr(self.gpus)} {repr(self.cpus)}"

class MpibindMapping():
    def __init__(self, mapping_string):
        self.assignments = []
        self._parse_mapping(mapping_string)

    def _parse_mapping(self, mapping_string):
        mapping_pattern = r'task([\d\s]+)thds([\d\s]+)gpus([\d\s]+)cpus([\d\s,-]+)'
        mapping_string = mapping_string.replace('\n', '').split('mpibind:')[1:]
        for line in mapping_string:
            result = re.search(mapping_pattern, line)
            thds = result.group(2).strip()
            gpus = result.group(3).strip()
            cpus = result.group(4).strip()

            self.assignments.append(MpibindTaskAssignment(cpus, gpus, thds))

    def __repr__(self):
        rep = ""
        for idx, member in enumerate(self.assignments):
            rep += f"task {idx}: {repr(member)}\n"
        return rep

    def __getitem__(self, idx):
        return self.assignments[idx]

class MpibindWrapper():
    def __init__(self):
        self.ffi = ffi
        self.lib = lib

    def get_mapping(
        self, 
        handle=None,
        topology=None,
        **kwargs
    ):
        if topology is not None:
            self.use_topology(topology)
        
        if handle is None:
            p_handle = self.ffi.new("struct mpibind_t **")
            self.lib.mpibind_init(p_handle)
            handle = p_handle[0]
        
        self._configure_handle(handle, **kwargs)
        self.mpibind(handle)
        return MpibindMapping(self._get_mapping_string(handle))

    def use_topology(self, topology_file_path):
        os.environ['HWLOC_XMLFILE'] = topology_file_path

    def set_ntasks(self, handle, ntasks):
        self.lib.mpibind_set_ntasks(handle, ntasks)

    def set_nthreads(self, handle, nthreads):
        self.lib.mpibind_set_nthreads(handle, nthreads)

    def set_smt(self, handle, smt):
        self.lib.mpibind_set_smt(handle, smt)

    def set_gpu_optim(self, handle, gpu_optim):
        self.lib.mpibind_set_gpu_optim(handle, gpu_optim)

    def set_greedy(self, handle, greedy):
        self.lib.mpibind_set_greedy(handle, greedy)
    
    def mpibind(self, handle):
        self.lib.mpibind(handle)

    def _get_mapping_string(self, handle, buffer_size = 2048, encoding = "utf-8"):
        mapping_buffer = self.ffi.new("char[]", buffer_size)
        self.lib.mpibind_get_mapping_string(handle, mapping_buffer, buffer_size)
        return self.ffi.string(mapping_buffer).decode(encoding)

    def _configure_handle(self, handle, **kwargs):
        if 'ntasks' in kwargs:
            self.set_ntasks(handle, kwargs['ntasks'])
        if 'nthreads' in kwargs:
            self.set_nthreads(handle, kwargs['nthreads'])
        if 'smt' in kwargs:
            self.set_smt(handle, kwargs['smt'])
        if 'gpu_optim' in kwargs:
            self.set_gpu_optim(handle, kwargs['gpu_optim'])
        if 'greedy' in kwargs:
            self.set_greedy(handle, kwargs['greedy'])


if __name__ == "__main__":
    """Demontration of working with mpibind python wrapper"""
    wrapper = MpibindWrapper()
    
    #Work directly with ffi through wrapper
    p_handle = wrapper.ffi.new("struct mpibind_t **")
    wrapper.lib.mpibind_init(p_handle)
    handle = p_handle[0]
    wrapper.lib.mpibind_set_ntasks(handle, 4)
    wrapper.lib.mpibind(handle)
    buffer = wrapper.ffi.new("char[]", 2048)
    wrapper.lib.mpibind_get_mapping_string(handle, buffer, 2048)
    low_mapping = MpibindMapping(wrapper.ffi.string(buffer).decode('utf-8'))

    print("Working with cffi:")
    print(low_mapping)
    print()
    
    #Let wrapper parse mapping with handle configured using direct ffi
    mid_mapping = wrapper.get_mapping(handle=handle)
    print("Passing pre-configured handle:")
    print(low_mapping)
    print()

    #Letting the wrapper use a one-time handle
    high_mapping = wrapper.get_mapping(ntasks=4)
    print("Letting wrapper use one-time handle:")
    print(low_mapping)
    print()