#ifndef MPIBIND_TEST_UTILS_H
#define MPIBIND_TEST_UTILS_H
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mpibind.h"
#include "tap.h"

/**
 * The number of tests present in the test_suite.
 * This will be referenced when parsing answers and
 * generating tests. This is also used to ensure the number of
 * tests and number of answers are consistent.
 * **/
#define NUM_TESTS 12

/**
 * Representation of a test answer
 * **/
typedef struct {
  char* description;
  char* thread_mapping;
  char* cpu_mapping;
  char* gpu_mapping;
} mpibind_test_out_t;
/**
 * Input parameters for a test. This mimics
 * the structure of mpibind_t, but is defined
 * separately to make the tests independent of
 * mpibind_t's definition.
 * **/
typedef struct {
  /* Input parameters */
  hwloc_topology_t topo;
  int ntasks;
  int in_nthreads;
  int greedy;
  int gpu_optim;
  int smt;
  char* restr_set;
  int restr_type;
} mpibind_test_in_t;
/**
 * Initialize a test struct to default values.
 * This mimics the behavior of mpibind_init
 * **/
int mpibind_test_in_t_init(mpibind_test_in_t* hdl);
/**
 * Frees an answer
 * **/
void mpibind_test_out_t_free(mpibind_test_out_t* t);
/**
 * Frees a test object
 * **/
void mpibind_test_in_t_free(mpibind_test_in_t* t);
/**
 * Prints the current state of an mpibind_test_in_t object
 * **/
void mpibind_test_in_t_print(mpibind_test_in_t* params);
/** Helper function to check the cpu, gpu, and thread mappings**/
void check_mapping(mpibind_t* handle, mpibind_test_out_t* expected);
/**
 * Runs a set of tests and compares them to their answers.
 * **/
void run_test(hwloc_topology_t topo, mpibind_test_in_t *params,  mpibind_test_out_t *expected);
/**
 * Generate unit test information from a topology.
 * This will take high level information and create an array of
 * of objects containing parameters for each of the tests. The number
 * of tests created is passed back via num_test_ptr
 * **/
mpibind_test_in_t** generate_test_information(hwloc_topology_t);
/** load an xml file into a topology **/
void load_topology(hwloc_topology_t* topo, char* xml_file);
/**
 * Loads a set of test answers from a file.
 * num_test_ptr will be used to store the number of answers parsed
 * **/
mpibind_test_out_t** load_answers(char* filename);
/** Performs a unit test using a given topology xml and an answer file.
 * Test drivers should call this method
 * **/
void unit_test_topology(char* topology_filename, char* answer_filename);
#endif
