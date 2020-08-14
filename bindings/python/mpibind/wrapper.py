import os
import re

from _mpibind._pympibind import ffi, lib


class MpibindWrapper:
    def __init__(self):
        """
        Wrapper for mpibind that attempts to facilitate common use cases like
        aquiring a mapping while still exposing the ffi and lib for advanced
        users
        """
        self.ffi = ffi
        self.lib = lib

    def get_mapping(self, handle=None, topology=None, **kwargs):
        """
        Use mpibind logic to get a mapping for a job. Users of this function
        can specifiy a handle they have configured externally. If no handle
        is passed, a one-time handle is initiated. Optionally, users can specify
        a topology xml file to use when determining the mapping. kwargs are used
        to catch attributes users would like to set on their handle before 
        determining the mapping (e.g. ntasks = 4)
        """
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
        """
        Setting the HWLOC_XMLFILE
        """
        os.environ["HWLOC_XMLFILE"] = topology_file_path

    def set_ntasks(self, handle, ntasks):
        """
        Small wrapper function for setting ntasks
        """
        self.lib.mpibind_set_ntasks(handle, ntasks)

    def set_nthreads(self, handle, nthreads):
        """
        Small wrapper function for setting nthreads
        """
        self.lib.mpibind_set_nthreads(handle, nthreads)

    def set_smt(self, handle, smt):
        """
        Small wrapper function for setting smt
        """
        self.lib.mpibind_set_smt(handle, smt)

    def set_gpu_optim(self, handle, gpu_optim):
        """
        Small wrapper function for setting gpu_optim
        """
        self.lib.mpibind_set_gpu_optim(handle, gpu_optim)

    def set_greedy(self, handle, greedy):
        """
        Small wrapper function for setting greedy
        """
        self.lib.mpibind_set_greedy(handle, greedy)

    def mpibind(self, handle):
        """
        Small wrapper function for call to mpibind
        """
        self.lib.mpibind(handle)

    def _get_mapping_string(self, handle, buffer_size=2048, encoding="utf-8"):
        """
        Internal helper that handles the passing of a buffer containing the
        mapping from C to Python
        """
        mapping_buffer = self.ffi.new("char[]", buffer_size)
        self.lib.mpibind_get_mapping(handle, mapping_buffer, buffer_size)
        return self.ffi.string(mapping_buffer).decode(encoding)

    def _configure_handle(self, handle, **kwargs):
        """
        Internal helper for processing the kwargs that get passed to get_mapping
        """
        if "ntasks" in kwargs:
            self.set_ntasks(handle, kwargs["ntasks"])
        if "nthreads" in kwargs:
            self.set_nthreads(handle, kwargs["nthreads"])
        if "smt" in kwargs:
            self.set_smt(handle, kwargs["smt"])
        if "gpu_optim" in kwargs:
            self.set_gpu_optim(handle, kwargs["gpu_optim"])
        if "greedy" in kwargs:
            self.set_greedy(handle, kwargs["greedy"])


class MpibindMapping:
    def __init__(self, mapping_string):
        """
        Class that parses a mapping string into an object for accessibility. 
        Ex:

        mapping = MpibindMapping(<mapping_string>)
        task0_assignment = mapping[0]
        """
        self.assignments = []
        self._parse_mapping(mapping_string)

    def _parse_mapping(self, mapping_string):
        """
        Internal helper that handles the initial parsing of the mapping string
        """
        mapping_pattern = r"task([\d\s]+)thds([\d\s]+)gpus([\d\s]+)cpus([\d\s,-]+)"
        mapping_string = mapping_string.replace("\n", "").split("mpibind:")[1:]
        for line in mapping_string:
            result = re.search(mapping_pattern, line)
            thds = result.group(2).strip()
            gpus = result.group(3).strip()
            cpus = result.group(4).strip()

            self.assignments.append(MpibindTaskAssignment(cpus, gpus, thds))

    def __repr__(self):
        """
        Set the format that users will see when print is used on a MpibindMapping
        object
        """
        rep = ""
        for idx, member in enumerate(self.assignments):
            rep += f"task {idx}: {repr(member)}\n"
        return rep

    def __getitem__(self, idx):
        """
        Enable the access of assignments using the bracket operator
        e.g. 
        task0_assignment = mapping[0] vs 
        """
        return self.assignments[idx]


class MpibindTaskAssignment:
    def __init__(self, cpus, gpus, thread_count):
        """
        Holder class for the resources given to a single task
        """
        self.cpus = MpibindResourceAssignment("cpus", cpus)
        self.gpus = MpibindResourceAssignment("gpus", gpus)
        self.smt = thread_count

    def __repr__(self):
        """
        Format for print
        """
        return f"smt: {self.smt} {repr(self.gpus)} {repr(self.cpus)}"


class MpibindResourceAssignment:
    def __init__(self, resource_type, resource_string):
        """
        Holder class that represents an assignment of resources
        """
        self.type = resource_type
        self.assignment = resource_string

    @property
    def assignment(self):
        return self.__assignment

    @assignment.setter
    def assignment(self, assignment):
        """
        Automatically update the count when the idset is changed
        """
        self.__assignment = assignment
        self.count = self._parse_count(assignment)

    def _parse_count(self, assignment):
        """
        Internal helper to calculate a resource count from an idset
        """
        if not assignment:
            return 0

        count = 0
        for item in assignment.split(","):
            count += 1
            if "-" in item:
                start, end = item.split("-")
                count += int(end) - int(start)

        return count

    def __repr__(self):
        """
        Format for print
        """
        return f"{self.type}: {self.assignment}"


if __name__ == "__main__":
    """Demontration of working with mpibind python wrapper"""
    wrapper = MpibindWrapper()

    # Work directly with ffi through wrapper
    p_handle = wrapper.ffi.new("struct mpibind_t **")
    wrapper.lib.mpibind_init(p_handle)
    handle = p_handle[0]
    wrapper.lib.mpibind_set_ntasks(handle, 4)
    wrapper.lib.mpibind(handle)
    buffer = wrapper.ffi.new("char[]", 2048)
    wrapper.lib.mpibind_get_mapping(handle, buffer, 2048)
    low_mapping = MpibindMapping(wrapper.ffi.string(buffer).decode("utf-8"))

    print("Working with cffi:")
    print(low_mapping)
    print()

    # Let wrapper parse mapping with handle configured using direct ffi
    mid_mapping = wrapper.get_mapping(handle=handle)
    print("Passing pre-configured handle:")
    print(low_mapping)
    print()

    # Letting the wrapper use a one-time handle
    high_mapping = wrapper.get_mapping(ntasks=4)
    print("Letting wrapper use one-time handle:")
    print(low_mapping)
    print()
