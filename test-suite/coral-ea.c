#include "test_utils.h"

int main(int argc, char **argv) {
  plan(NO_PLAN);

  char* topology_file = "../topo-xml/coral-ea-hwloc1.xml";
  char* answer_file = "./expected/expected.coral-ea";
  unit_test_topology(topology_file, answer_file);

  done_testing();
  return (0);
}
