// Copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
// See LICENSE.txt for details.

#define BOOST_TEST_MODULE DelibHelperTests
#include <boost/test/unit_test.hpp>

#include "molassembler/Detail/DelibHelpers.h"
#include "temple/Random.h"
#include "molassembler/Options.h"

#include <Eigen/Geometry>

#include <cmath>

BOOST_AUTO_TEST_CASE(dihedralTests) {
  using namespace molassembler::DelibHelpers;

  Delib::PositionCollection positions;
  positions.push_back(
    Delib::Position {
      Eigen::Vector3d {
        1, 0, 0
      }
    }
  );
  positions.push_back(
    Delib::Position {
      Eigen::Vector3d {
        0, 0, 0
      }
    }
  );
  positions.push_back(
    Delib::Position {
      Eigen::Vector3d {
        0, 1, 0
      }
    }
  );

  const Eigen::Vector3d lastPosition {1, 1, 0};

  positions.push_back(
    Delib::Position {lastPosition}
  );

  for(
    const double randomAngle :
    temple::random::getN<double>(
      -M_PI + 0.01,
      M_PI - 0.01,
      100,
      molassembler::randomnessEngine()
    )
  ) {

    positions[3] = Delib::Position {
      Eigen::AngleAxisd(
        randomAngle,
        Eigen::Vector3d::UnitY()
      ) * lastPosition
    };

    const auto dihedral = getDihedral(positions, 0, 1, 2, 3);

    BOOST_CHECK_MESSAGE(
       std::fabs(dihedral - randomAngle) < 1e-10,
      "Twist angle: " << randomAngle << ", reported angle: " << dihedral
    );
  }
}
