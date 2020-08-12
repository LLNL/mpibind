#include <hip/hip_runtime.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

/*
    Detects visible cpus
*/
int main (int argc, char** argv)
{
    int num_devices;
    hipDeviceProp_t devProp;
    hipError_t err;
    char *rocr_env, *hip_env;

    rocr_env = getenv ("ROCR_VISIBLE_DEVICES");
    hip_env = getenv ("HIP_VISIBLE_DEVICES");

    err = hipGetDeviceCount (&num_devices);

    if (rocr_env)
        std::cout << getpid () << ": ROCR_VISIBLE_DEVICES := " << rocr_env << std::endl;
    if (hip_env)
        std::cout << getpid () << ": HIP_VISIBLE_DEVICES := " << hip_env << std::endl;
    std::cout << "Number of computer capable devices: " << num_devices << std::endl;

    if (err == hipSuccess) {
        int i;
        for (i = 0; i < num_devices; i++) {
            hipGetDeviceProperties (&devProp, i);
            std::cout << "-------" << std::endl;
            std::cout << " System minor " << devProp.minor << std::endl;
            std::cout << " System major " << devProp.major << std::endl;
            std::cout << " agent prop name " << devProp.name << std::endl;
            std::cout << " device id " << devProp.pciDeviceID << "|" << devProp.pciBusID
                      << std::endl;
        }
    } else if (err == hipErrorNoDevice) {
        std::cout << "No devices found..." << std::endl;
    }

    else {
        std::cout << "Something went wrong!" << std::endl;
    }

    return 0;
}