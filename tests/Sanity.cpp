#define BOOST_TEST_MODULE SanityTests
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <iostream>

#include "DistanceGeometry/generateConformation.h"
#include "BoundsFromSymmetry.h"
#include "IterateStereocenterPermutations.h"

using namespace std::string_literals;
using namespace MoleculeManip;
using namespace MoleculeManip::DistanceGeometry;

void explainDifference(
  const StereocenterList& a,
  const StereocenterList& b
) {
  std::cout << "A:" << std::endl;
  for(const auto& stereocenterPtr : a) {
    std::cout << stereocenterPtr << std::endl;
  }

  std::cout << "B:" << std::endl;
  for(const auto& stereocenterPtr : b) {
    std::cout << stereocenterPtr << std::endl;
  }
  std::cout << std::endl;
}

/* Test whether generating coordinates from a simple molecule and then
 * recovering all the stereocenter data from the positions alone yields the
 * same StereocenterList as you started out with
 *
 * TODO This test will fail worse now that SymmetryFit takes into account the 
 * geometry we expect the center to take, simply because the molecules created
 * by DGDBM::asymmetricMolecule are (chemically speaking) abject nonsense
 */
BOOST_AUTO_TEST_CASE( createPositionsAndFitNewMoleculeEqual ) {
  for(const auto& symmetryName: Symmetry::allNames) {
    // Get an asymmetric molecule (all ligands different) for the current molecule
    auto molecule = DGDBM::asymmetricMolecule(symmetryName);

    // For each possible arrangement of these ligands
    for(const auto& stereocenterPermutation : iterateStereocenterPermutations(molecule)) {
      /* Create an ensemble of 3D positions using threeDimensional refinement,
       * no metrization and uniform distance setting
       */
      auto ensemble = detail::runDistanceGeometry(
        molecule,
        100,
        MetrizationOption::off,
        false, // no y-inversion trick
        BFSConstraintCollector::DistanceMethod::Uniform
      );

      /* Check that for every PositionCollection, inferring the StereocenterList
       * from the generated coordinates yields the same StereocenterList you 
       * started out with.
       */
      auto mapped = TemplateMagic::map(
        ensemble,
        [&](const auto& positions) -> bool {
          auto inferredStereocenterList = molecule.getAdjacencyList().inferStereocentersFromPositions(
            positions
          );

          bool pass = molecule.stereocenters == inferredStereocenterList;

          if(!pass) {
            explainDifference(molecule.stereocenters, inferredStereocenterList);
          }

          return pass;
        }
      );

      /* The test passes only if this is true for all positioncollections
       * yielded by DG
       */
      bool testPass = TemplateMagic::all_of(mapped);

      if(!testPass) {
        auto pass = TemplateMagic::count(mapped, true);

        std::cout << "Test fails!" << std::endl
          << std::setw(8) << " " << " " << Symmetry::name(symmetryName) 
          << std::endl
          << std::setw(8) << std::to_string(pass)+ "/100"
          << " comparisons with inferred StereocenterList pass" << std::endl;

        std::cout << "StereocenterList has entries:" << std::endl;
        for(const auto& stereocenter: stereocenterPermutation.stereocenters) {
          std::cout << stereocenter << std::endl;
        }
      }

      BOOST_CHECK(testPass);
    }
  }
}
