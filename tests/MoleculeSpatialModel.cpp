#define BOOST_TEST_MODULE MoleculeSpatialModelTests
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "DistanceGeometry/generateConformation.h"
#include "BoundsFromSymmetry.h"

BOOST_AUTO_TEST_CASE(dumpDebugInfo) {
  using namespace MoleculeManip::DistanceGeometry;

  for(const auto& symmetryName: Symmetry::allNames) {
    auto molecule = DGDBM::asymmetricMolecule(symmetryName);

    MoleculeSpatialModel spatialModel {
      molecule.getGraph(),
      molecule.getStereocenterList(),
      MoleculeSpatialModel::DistanceMethod::UFFLike
    };

    std::cout << Symmetry::name(symmetryName) << std::endl;
    spatialModel.dumpDebugInfo();

    std::cout << "Resulting bounds matrix:" << std::endl;
    std::cout << spatialModel.makeDistanceBounds().access() << std::endl;
  }
}