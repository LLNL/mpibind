#include "test_utils.h"

int main(int argc, char **argv) {
  plan(NO_PLAN);

  char* topology_file = "../topo-xml/cts1-quartz-smt1.xml";
  char* answer_file = "./expected/expected.cts1-quartz";
  unit_test_topology(topology_file, answer_file);

  done_testing();
  return (0);
}
