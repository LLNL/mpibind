#include <CL/cl.h>
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
    cl_uint num_of_platforms = 0;
    cl_device_id* device_id = new cl_device_id[20];
    cl_uint num_of_devices = 0;
    cl_uint device_vendor_id;
    char* buf = new char[1024];
    size_t ret_size;

    if (clGetPlatformIDs (0, NULL, &num_of_platforms) != CL_SUCCESS) {
        printf ("Unable to get platform_id\n");
        return 1;
    }

    std::cout << "Number of platforms: " << num_of_platforms << std::endl;
    cl_platform_id* platform_ids = new cl_platform_id[num_of_platforms];

    if (clGetPlatformIDs (num_of_platforms, platform_ids, NULL) != CL_SUCCESS) {
        printf ("Unable to get platform_id\n");
        return 1;
    }

    bool found = false;
    for (size_t i = 0; i < num_of_platforms; i++)
        if (clGetDeviceIDs (platform_ids[i], CL_DEVICE_TYPE_GPU,
                            5, device_id, &num_of_devices) == CL_SUCCESS) {
            std::cout << getpid () << ": num devices := " << num_of_devices << std::endl;
            found = true;
            for (size_t j = 0; j < num_of_devices; j++) {
                clGetDeviceInfo (device_id[j], CL_DEVICE_NAME, 1024, buf, &ret_size);
                std::cout << getpid () << ": I found an id " << buf << "|" << device_id[j] << std::endl;
            }
        }
    if (!found) {
        printf ("Unable to get device_id\n");
        return 1;
    }

    return 0;
}