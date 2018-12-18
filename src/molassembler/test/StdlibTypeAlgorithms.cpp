/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 */

#include <boost/test/unit_test.hpp>

#include "molassembler/Detail/StdlibTypeAlgorithms.h"
#include "temple/Adaptors/Zip.h"
#include "temple/Functional.h"
#include "temple/Stringify.h"

#include <iostream>

using namespace Scine;
using namespace molassembler;
using namespace StdlibTypeAlgorithms;

BOOST_AUTO_TEST_CASE( stdlibTypeAlgorithms ) {
  /* 1, 2 (2 just calls 1) */
  std::vector<
    std::set<unsigned>
  > testSetList {
    {5, 2, 3, 9, 11, 4},
    {2, 1, 0, 12},
    {13, 6}
  };

  BOOST_CHECK(
    vectorOfSetsEqual(
      mergeOverlappingSets(
        testSetList
      ),
      {
        {0, 1, 2, 3, 4, 5, 9, 11, 12},
        {6, 13}
      }
    )
  );

  /* 3 */
  std::set<
    std::pair<unsigned, unsigned>
  > pairsSet {
    {1, 2},
    {2, 3},
    {4, 5},
    {5, 7},
    {7, 6} // can't remember if order is important or not, try it out
  };

  BOOST_CHECK(
    vectorOfSetsEqual(
      makeIndividualSets(
        pairsSet
      ),
      {
        {1, 2, 3},
        {4, 5, 6, 7}
      }
    )
  );

  /* 4 */
  std::vector<unsigned>
    a {1, 4, 7},
    b {2, 9, 3},
    expectedMerge {1, 4, 7, 2, 9, 3};
  auto merged = copyMerge(a, b);
  BOOST_CHECK(
    std::equal(
      merged.begin(),
      merged.end(),
      expectedMerge.begin(),
      expectedMerge.end()
    )
  );
}


BOOST_AUTO_TEST_CASE( combinationPermutation ) {
  const std::vector<unsigned> testLimits {4, 1, 3, 6, 9, 2};
  std::vector<unsigned> combination (testLimits.size(), 0);

  bool alwaysSmallerOrEqual = true;
  while(nextCombinationPermutation(combination, testLimits)) {
    if(
      !temple::all_of(
        temple::adaptors::zip(
          combination,
          testLimits
        ),
        [](const unsigned index, const unsigned limit) -> bool {
          return index <= limit;
        }
      )
    ) {
      alwaysSmallerOrEqual = false;
      std::cout << "Falsified for combination {"
        << temple::condense(combination)
        << "}" << std::endl;
      break;
    }
  }

  BOOST_CHECK(alwaysSmallerOrEqual);
}
