#define BOOST_TEST_MODULE RankingTreeTestsModule
#define BOOST_FILESYSTEM_NO_DEPRECATED

#include "boost/filesystem.hpp"
#include "boost/graph/isomorphism.hpp"
#include "boost/test/unit_test.hpp"

#include "temple/constexpr/Bitmask.h"
#include "temple/Containers.h"
#include "temple/Stringify.h"

#include "molassembler/Cycles.h"
#include "molassembler/Detail/StdlibTypeAlgorithms.h"
#include "molassembler/Graph/GraphAlgorithms.h"
#include "molassembler/IO.h"
#include "molassembler/Molecule/RankingTree.h"
#include "molassembler/StereocenterList.h"

#include <random>
#include <fstream>

using namespace molassembler;

const std::string directoryPrefix = "ranking_tree_molecules/";

bool isBondStereocenter(
  const Molecule& molecule,
  const BondIndex& e,
  const unsigned numPermutations,
  const boost::optional<unsigned>& assignment
) {
  auto stereocenterOption = molecule.stereocenters().option(e);

  if(!stereocenterOption) {
    std::cout << "No stereocenter on vertices " << e.first << " - " << e.second << "\n";

    for(const auto& stereocenter : molecule.stereocenters().bondStereocenters()) {
      std::cout << "BondStereocenter on " << e.first << " - " << e.second << ": " << stereocenter.info() << "\n";
    }
    return false;
  }

  if(stereocenterOption->numStereopermutations() != numPermutations) {
    std::cout << "Bond stereocenter on " << stereocenterOption->edge().first
      << " - " << stereocenterOption->edge().second
      << " has " << stereocenterOption->numStereopermutations()
      << " stereopermutations, not " << numPermutations << "\n";
    return false;
  }

  if(assignment) {
    if(stereocenterOption->assigned() != assignment.value()) {
      std::cout << "Bond stereocenter on " << stereocenterOption->edge().first
        << " - " << stereocenterOption->edge().second
        << " is assigned "
        << (
          stereocenterOption->assigned()
          ? std::to_string(stereocenterOption->assigned().value())
          : "u"
        )
        << ", not " << assignment.value() << "\n";
      return false;
    }
  }

  return true;
}

bool isAtomStereocenter(
  const Molecule& molecule,
  AtomIndex i,
  const unsigned numPermutations,
  const boost::optional<unsigned>& assignment
) {
  auto stereocenterOption = molecule.stereocenters().option(i);

  if(!stereocenterOption) {
    std::cout << "No stereocenter on atom index " << i << "\n";
    return false;
  }

  if(stereocenterOption->numStereopermutations() != numPermutations) {
    std::cout << "Atom stereocenter on " << i << " has "
      << stereocenterOption->numStereopermutations() << " stereopermutations, not "
      << numPermutations << "\n";
    return false;
  }

  if(assignment) {
    if(stereocenterOption->assigned() != assignment.value()) {
      std::cout << "Atom stereocenter on " << i << " is assigned "
        << (
          stereocenterOption->assigned()
          ? std::to_string(stereocenterOption->assigned().value())
          : "u"
        ) << ", not " << assignment.value() << "\n";
      return false;
    }
  }

  return true;
}

bool isStereogenic(
  const Molecule& molecule,
  AtomIndex i
) {
  auto stereocenterOption = molecule.stereocenters().option(i);

  if(!stereocenterOption) {
    return false;
  }

  if(stereocenterOption->numStereopermutations() <= 1) {
    return false;
  }

  return true;
}

void writeExpandedTree(
  const std::string& fileName,
  const AtomIndex expandOnIndex
) {
  auto molecule = IO::read(
    directoryPrefix + fileName
  );

  auto expandedTree = RankingTree(
    molecule.graph(),
    molecule.graph().cycles(),
    molecule.stereocenters(),
    molecule.dumpGraphviz(),
    expandOnIndex,
    {},
    RankingTree::ExpansionOption::Full
  );

  std::ofstream dotFile(fileName + ".dot");
  dotFile << expandedTree.dumpGraphviz();
  dotFile.close();
}

BOOST_AUTO_TEST_CASE(sequenceRuleOneTests) {
  using namespace std::string_literals;
  // P. 92.2.2 Sequence subrule 1b: Priority due to duplicate atoms
  // Cycle and multiple-bond splitting
  auto exampleThree = IO::read(
    directoryPrefix + "1S5R-bicyclo-3-1-0-hex-2-ene.mol"
  );

  auto exampleThreeExpanded = RankingTree(
    exampleThree.graph(),
    exampleThree.graph().cycles(),
    exampleThree.stereocenters(),
    exampleThree.dumpGraphviz(),
    0,
    {},
    RankingTree::ExpansionOption::Full
  );

  auto exampleThreeRanked = exampleThreeExpanded.getRanked();

  BOOST_CHECK_MESSAGE(
    (exampleThreeRanked == std::vector<
      std::vector<unsigned long>
    > { {6}, {3}, {2}, {1} }),
    "Example three expanded is not {{6}, {3}, {2}, {1}}, but: "
    << temple::condenseIterable(
      temple::map(
        exampleThreeRanked,
        [](const auto& set) -> std::string {
          return temple::condenseIterable(set);
        }
      )
    )
  );

  auto exampleThreeExpandedAgain = RankingTree(
    exampleThree.graph(),
    exampleThree.graph().cycles(),
    exampleThree.stereocenters(),
    exampleThree.dumpGraphviz(),
    1,
    {},
    RankingTree::ExpansionOption::Full
  );

  BOOST_CHECK((
    exampleThreeExpandedAgain.getRanked() == std::vector<
      std::vector<unsigned long>
    > { {7}, {4}, {2}, {0} }
  ));
}

template<typename T>
std::string condenseSets(const std::vector<std::vector<T>>& sets) {
  return temple::condenseIterable(
    temple::map(
      sets,
      [](const auto& set) -> std::string {
        return "{"s + temple::condenseIterable(set) + "}"s;
      }
    )
  );
}

BOOST_AUTO_TEST_CASE(sequenceRuleThreeTests) {
  // P-92.4.2.1 Example 1 (Z before E)
  auto ZEDifference = IO::read(
    directoryPrefix + "2Z5S7E-nona-2,7-dien-5-ol.mol"s
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(ZEDifference, 0, 2, 0),
    "Stereocenter at C0 in 2Z5S7E-nona-2,7-dien-5-ol is not S"
  );

  // P-92.4.2.2 Example 1 (Z before E in aux. stereocenters, splitting)
  auto EECyclobutane = IO::read(
    directoryPrefix + "1E3E-1,3-difluoromethylidenecyclobutane.mol"
  );

  BOOST_CHECK_MESSAGE(
    isBondStereocenter(EECyclobutane, BondIndex {0, 3}, 2, 0)
    && isBondStereocenter(EECyclobutane, BondIndex {5, 6}, 2, 0),
    "1E3E-1,3-difluoromethylidenecyclobutane double bonds aren't E. "
  );

  // P-92.4.2.2 Example 2 (stereogenic before non-stereogenic)
  auto inTreeNstgDB = IO::read(
    directoryPrefix
    + "(2Z5Z7R8Z11Z)-9-(2Z-but-2-en-1-yl)-5-(2E-but-2-en-1-yl)trideca-2,5,8,11-tetraen-7-ol.mol"s
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(inTreeNstgDB, 0, 2, 1u),
    "(2Z5Z7R8Z11Z)-9-(2Z-but-2-en-1-yl)-5-(2E-but-2-en-1-yl)trideca-2,5,8,11-tetraen-7-ol "
    "difference between non-stereogenic auxiliary stereocenter and assigned "
    "stereocenter isn't recognized! "
  );
}

BOOST_AUTO_TEST_CASE(sequenceRuleFourTests) {
  // (4A) P-92.5.1 Example (stereogenic before non-stereogenic)
  auto pseudoOverNonstg = IO::read(
    directoryPrefix
    + "(2R,3s,4S,6R)-2,6-dichloro-5-(1R-1-chloroethyl)-3-(1S-1-chloroethyl)heptan-4-ol.mol"s
  );

  BOOST_CHECK_MESSAGE(
    !isStereogenic(pseudoOverNonstg, 10),
    "(2R,3s,4S,6R)-2,6-dichloro-5-(1R-1-chloroethyl)-3-(1S-1-chloroethyl)heptan-4-ol.mol "
    "branch with R-R aux. stereocenters not non-stereogenic"
  );

  BOOST_CHECK_MESSAGE(
    isStereogenic(pseudoOverNonstg, 1),
    "(2R,3s,4S,6R)-2,6-dichloro-5-(1R-1-chloroethyl)-3-(1S-1-chloroethyl)heptan-4-ol.mol "
    "branch with R-S aux. stereocenters not stereogenic"
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(pseudoOverNonstg, 0, 2, 0),
    "(2R,3s,4S,6R)-2,6-dichloro-5-(1R-1-chloroethyl)-3-(1S-1-chloroethyl)heptan-4-ol.mol "
    "sequence rule 4A does not recognize stereogenic over non-stereogenic, 3 as S"
  );

  // (4B) P-92.5.2.2 Example 1 (single chain pairing, ordering and reference selection)
  auto simpleLikeUnlike = IO::read(
    directoryPrefix + "(2R,3R,4R,5S,6R)-2,3,4,5,6-pentachloroheptanedioic-acid.mol"s
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(simpleLikeUnlike, 10, 2, 1u),
    "(2R,3R,4R,5S,6R)-2,3,4,5,6-pentachloroheptanedioic-acid central carbon does "
    " not register as a stereocenter and/or isn't assigned as R"
  );

  // (4B) P-92.5.2.2 Example 3 (single-chain pairing, cycle splitting)
  auto lAlphaLindane = IO::read(
    directoryPrefix + "l-alpha-lindane.mol"s
  );

  BOOST_CHECK_MESSAGE(
    (
      temple::all_of(
        std::vector<AtomIndex> {6, 7, 8, 9, 10, 11},
        [&](const auto carbonIndex) -> bool {
          return isStereogenic(lAlphaLindane, carbonIndex);
        }
      )
    ),
    "Not all L-alpha-lindane carbon atoms not recognized as stereocenters!"
  );

  // (4B) P-92.5.2.2 Example 4 (multiple-chain stereocenter ranking)
  auto oxyNitroDiffBranches = IO::read(
    directoryPrefix + "(2R,3S,6R,9R,10S)-6-chloro-5-(1R,2S)-1,2-dihydroxypropoxy-7-(1S,2S)-1,2-dihydroxypropoxy-4,8-dioxa-5,7-diazaundecande-2,3,9,10-tetrol.mol"s
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(oxyNitroDiffBranches, 0, 2, 1u),
    "(2R,3S,6R,9R,10S)-6-chloro-5-(1R,2S)-1,2-dihydroxypropoxy-7-(1S,2S)-1,2-dihydroxypropoxy-4,8-dioxa-5,7-diazaundecande-2,3,9,10-tetrol central carbon not recognized as R"
  );

  // (4B) P-92.5.2.2 Example 5 (multiple-chain stereocenter ranking)
  auto groupingDifferences = IO::read(
    directoryPrefix + "(2R,3R,5R,7R,8R)-4.4-bis(2S,3R-3-chlorobutan-2-yl)-6,6-bis(2S,4S-3-chlorobutan-2-yl)-2,8-dichloro-3,7-dimethylnonan-5-ol.mol"s
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(groupingDifferences, 0, 2, 1u),
    "The central carbon in (2R,3R,5R,7R,8R)-4.4-bis(2S,3R-3-chlorobutan-2-yl)-6,6-bis(2S,4S-3-chlorobutan-2-yl)-2,8-dichloro-3,7-dimethylnonan-5-ol is not recognized as R"
  );

  // (4B) P-92.5.2.2 Example 6 (number of reference descriptors)
  auto numReferenceDescriptors = IO::read(
    directoryPrefix + "2R-2-bis(1R)-1-hydroxyethylamino-2-(1R)-1-hydroxyethyl(1S)-1-hydroxyethylaminoacetic-acid.mol"
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(numReferenceDescriptors, 0, 2, 1u),
    "The central carbon in 2R-2-bis(1R)-1-hydroxyethylamino-2-(1R)-1-hydroxyethyl(1S)-1-hydroxyethylaminoacetic-acid is not recognized as R"
  );
}

BOOST_AUTO_TEST_CASE(sequenceRuleFiveTests) {
  // (4C) P-92.5.3 Example r/s leads to R difference
  auto rsDifference = IO::read(
    directoryPrefix + "(2R,3r,4R,5s,6R)-2,6-dichloro-3,5-bis(1S-1-chloroethyl)heptan-4-ol.mol"
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(rsDifference, 0, 2, 1u),
    "The central carbon in (2R,3r,4R,5s,6R)-2,6-dichloro-3,5-bis(1S-1-chloroethyl)heptan-4-ol is not recognized as R"
  );

  // (5) P-92.6 Example 1 simple R/S difference leads to r
  auto pseudo = IO::read(
    directoryPrefix + "(2R,3r,4S)-pentane-2,3,4-trithiol.mol"
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(pseudo, 0, 2, 1u),
    "The central carbon in (2R,3r,4S)-pentane-2,3,4-trithiol is not recognized as R"
  );

  // (5) P-92.6 Example 2 cyclobutane splitting
  auto cyclobutane = IO::read(
    directoryPrefix + "(1r,3r)-cyclobutane-1,3-diol.mol"
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(cyclobutane, 2, 2, 1u)
    && isAtomStereocenter(cyclobutane, 3, 2, 1u),
    "The chiral carbons in (1r,3r)-cyclobutane-1,3-diol aren't properly recognized"
  );

  // (5) P-92.6 Example 5 double bond ranking
  auto pseudoDB = IO::read(
    directoryPrefix + "(2E,4R)-4-chloro-3-(1S-1-chloroethyl)pent-2-ene.mol"
  );

  BOOST_CHECK_MESSAGE(
    isBondStereocenter(pseudoDB, BondIndex {0, 3}, 2, 0u),
    "Double bond in (2E,4R)-4-chloro-3-(1S-1-chloroethyl)pent-2-ene isn't E"
  );

  // (5) P-92.6 Example 6
  auto fourDoesNothing = IO::read(
    directoryPrefix + "1s-1-(1R,2R-1,2-dichloropropyl-1S,2R-1,2-dichloropropylamino)1-(1R,2S-1,2-dichloropropyl-1S,2S-1,2-dichloropropylamino)methan-1-ol.mol"
  );

  BOOST_CHECK_MESSAGE(
    isAtomStereocenter(fourDoesNothing, 0, 2, 0u),
    "The central stereocenter in 1s-1-(1R,2R-1,2-dichloropropyl-1S,2R-1,2-dichloropropylamino)1-(1R,2S-1,2-dichloropropyl-1S,2S-1,2-dichloropropylamino)methan-1-ol isn't recognized as S"
  );
}
