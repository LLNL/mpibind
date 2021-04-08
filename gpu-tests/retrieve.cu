/******************************************************
 * Edgar A. Leon
 * Lawrence Livermore National Laboratory
 ******************************************************/

#include <stdio.h>
#ifdef HAVE_AMD_GPUS
#include "hip/hip_runtime.h"
#endif 

#define MAX_PCI_LEN 20

void chooseDevPartial(int dev)
{
  int odev=-1;
  int busId=-1, deviceId=-1, domainId=-1;
  char pci[MAX_PCI_LEN];
  cudaDeviceProp prop; 

  // Get selected device properties 
  cudaDeviceGetPCIBusId(pci, MAX_PCI_LEN, dev);
  sscanf(pci, "%04x:%02x:%02x", &domainId, &busId, &deviceId);
  
   // Partially fill device properties and match 
   memset(&prop, 0, sizeof(cudaDeviceProp));
   prop.pciDomainID = domainId; 
   prop.pciBusID = busId;
   prop.pciDeviceID = deviceId; 
   
   cudaChooseDevice(&odev, &prop);
   printf("Partial match of device %d: device %d\n", dev, odev);
   printf("\tInput: DomainID=0x%x BusId=0x%x DeviceId=0x%x\n",
    domainId, busId, deviceId); 
   if (dev != odev)
     printf("\tError: ChooseDevice did not match the correct device\n");
}

void chooseDevFull(int dev)
{
  int odev=-1;
  cudaDeviceProp prop; 

  // Get all device properties 
  cudaGetDeviceProperties(&prop, dev); 

  cudaChooseDevice(&odev, &prop);
  printf("Full match of device %d: device %d\n", dev, odev);
  printf("\tInput: DomainID=0x%x BusId=0x%x DeviceId=0x%x\n",
	 prop.pciDomainID, prop.pciBusID, prop.pciDeviceID); 
#ifndef HAVE_AMD_GPUS
  // HIP does not have a uuid field! 
  printf("\t       UUID=0x%x\n", prop.uuid); 
#endif

  if (dev != odev)
    printf("\tError: ChooseDevice did not match the correct device\n");
}

void getDevByPCI(int dev, char *pci)
{
  int pciBusID=-1, pciDeviceID=-1, pciDomainID=-1;
  int odev=-1; 
    
  sscanf(pci, "%04x:%02x:%02x", &pciDomainID, &pciBusID, &pciDeviceID);

  // PCI ID: String in one of the following forms: 
  // [domain]:[bus]:[device].[function] 
  // [domain]:[bus]:[device] 
  // [bus]:[device].[function] 
  // where domain, bus, device, and function are all hex values
  cudaDeviceGetByPCIBusId(&odev, pci);

  printf("GetbyPCI match of device %d: device %d\n", dev, odev);
  printf("\tInput: DomainID=0x%x BusId=0x%x DeviceId=0x%x\n",
    pciDomainID, pciBusID, pciDeviceID); 
  if (odev != dev)
    printf("Error: GetByPCI did not match the correct device\n");
}


int main(int argc, char *argv[])
{
  int dev, ndevs; 
  char pci[MAX_PCI_LEN];

  
  cudaGetDeviceCount(&ndevs);
  if (ndevs <= 0) {
    printf("No devices found\n"); 
    return 0; 
  }

  // Select input device
  // Avoid choosing device 0, if possible, to enhance testing
  // dev = 1; 
  dev = ndevs-1; 

  cudaDeviceGetPCIBusId(pci, MAX_PCI_LEN, dev);
  printf("PCI ID of device %d = %s\n", dev, pci);

  getDevByPCI(dev, pci);

  chooseDevPartial(dev);

  chooseDevFull(dev);

  cudaSetDevice(dev);
    
  return 0; 
}

