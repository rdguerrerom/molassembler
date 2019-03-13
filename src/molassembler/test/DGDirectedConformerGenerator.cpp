/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 */

#define BOOST_FILESYSTEM_NO_DEPRECATED

#include "boost/filesystem.hpp"
#include "boost/test/unit_test.hpp"

#include "molassembler/DirectedConformerGenerator.h"
#include "molassembler/Molecule.h"
#include "molassembler/IO.h"

#include "Utils/Typenames.h"

#include "temple/Invoke.h"
#include "temple/Stringify.h"

#include <fstream>
#include <iomanip>
#include <iostream>

using namespace std::string_literals;
using namespace Scine;
using namespace molassembler;

BOOST_AUTO_TEST_CASE(directedConformerGenerator) {
  std::vector<
    std::tuple<std::string, unsigned, unsigned>
  > testCases {
    {"directed_conformer_generation/butane.mol", 1, 3},
    {"directed_conformer_generation/pentane.mol", 2, 9},
    {"directed_conformer_generation/caffeine.mol", 0, 0},
    {"isomorphisms/testosterone.mol", 1, 3},
  };

  auto executeTest = [](
    const std::string& filename,
    const unsigned numConsideredBonds,
    const unsigned idealEnsembleSize
  ) {
    auto mol = IO::read(filename);
    DirectedConformerGenerator generator(mol);

    BOOST_CHECK_MESSAGE(
      generator.bondList().size() == numConsideredBonds,
      "Bond list yielded by generator does not have expected size. Expected "
      << numConsideredBonds << " for " << filename << ", got "
      << generator.bondList().size() << " instead."
    );

    BOOST_CHECK_MESSAGE(
      generator.idealEnsembleSize() == idealEnsembleSize,
      "Generator ideal ensemble size does not yield expected number of "
      "conformers. Expected " << idealEnsembleSize << " for " << filename
        << ", got " << generator.idealEnsembleSize() << " instead."
    );

    // If there are
    if(idealEnsembleSize == 0) {
      return;
    }

    // Make a strict configuration. 500 Steps really needs to be enough for these
    DistanceGeometry::Configuration configuration {};
    configuration.refinementStepLimit = 500;

    /* Ensure we can make generate all conformers we have hypothesized exist */
    const unsigned maxTries = 3;
    while(generator.decisionListSetSize() != generator.idealEnsembleSize()) {
      auto newDecisionList = generator.generateNewDecisionList();

      bool pass = false;
      for(unsigned attempt = 0; attempt < maxTries; ++attempt) {
        auto positionResult = generator.generateConformation(newDecisionList, configuration);

        if(positionResult) {
          pass = true;
          break;
        }

        std::cout << "Conformer generation failure: " << positionResult.error().message() << "\n";
      }

      BOOST_CHECK_MESSAGE(
        pass,
        "Could not generate " << filename << " conformer w/ decision list: "
          << temple::stringify(newDecisionList) << " in " << maxTries << " attempts"
      );
    }
  };

  for(const auto& tup : testCases) {
    temple::invoke(executeTest, tup);
  }
}