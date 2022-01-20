//-----------------------------------------------------------------------------
// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//-----------------------------------------------------------------------------
//
// This file is a simple driver to run tests expressed as a datalog script that
// contains facts describing the dataflow graph (such as edges and tag claims)
// and our expectations (such as which tags will be present or absent). It
// returns 1 if the test had contents in the testFails relation, 0 otherwise.
// This allows the datalog tests to be run from a bazel cc_test rule and avoids
// managing external facts files or diffing against an expected output file.
//
#include <iostream>
#include <memory>

#include "souffle/SouffleInterface.h"

int run_test(std::string const &test_name) {
  // We want one command line arg, the name of the current test module.
  std::unique_ptr<souffle::SouffleProgram> prog(
      souffle::ProgramFactory::newInstance(test_name));
  std::cout << "Attempting to run souffle program " << test_name << std::endl;
  assert(prog != nullptr);

  prog->run();

  souffle::Relation *checks_and_results = prog->getRelation("checkAndResult");

  assert(checks_and_results != nullptr);

  bool const test_is_trivial = checks_and_results->size() == 0;

  bool test_has_failures = false;
  for (auto &row : *checks_and_results) {
    std::string check_name;
    std::string pass_or_fail;
    row >> check_name >> pass_or_fail;
    if (pass_or_fail == "PASS") {
      continue;
    } else if (pass_or_fail == "FAIL") {
      test_has_failures = true;
      break;
    } else {
      assert(false);
    }
  }

  if (test_is_trivial) {
    std::cout
      << "Test "
      << test_name
      << " does not have any test conditions."
      << std::endl;
  } else if (test_has_failures) {
    std::cout
      << "Test " << test_name << " failed." << std::endl;
  } else {
    std::cout << "Test " << test_name << " succeeded." << std::endl;
    return 0;
  }
  prog->printAll();
  return 1;
}

int main(int argc, char **argv) {
  // The first command line arg should be the name of the current test. The
  // second optional argument should indicate whether or not the exit code
  // of the test should be inverted for expected-fail tests.
  assert((argc == 2) || (argc == 3));
  std::string const test_name = std::string(argv[1]);
  bool const invert_test = (argc == 3) && (std::string(argv[2]) == "invert");

  int const test_exit_code = run_test(test_name);
  if (invert_test) {
    return (test_exit_code == 0) ? 1 : 0;
  } else {
    return test_exit_code;
  }

}
