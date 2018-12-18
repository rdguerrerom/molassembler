/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 * @brief A series of helpers to interface with the Delib library
 */

#ifndef INCLUDE_MOLASSEMBLER_DELIB_HELPERS_H
#define INCLUDE_MOLASSEMBLER_DELIB_HELPERS_H

#include "Utils/Typenames.h"
#include "molassembler/Types.h"

namespace Scine {

namespace molassembler {

namespace DelibHelpers {

//! Fetches the cartesian distance between two indices in a PositionCollection
double getDistance(
  const Scine::Utils::PositionCollection& positions,
  AtomIndex i,
  AtomIndex j
);

//! Fetches the angle in radians between three indices in a PostionCollection
double getAngle(
  const Scine::Utils::PositionCollection& positions,
  AtomIndex i,
  AtomIndex j,
  AtomIndex k
);

/*!
 * Fetches the dihedral angle in radians defined by four indices in a
 * PositionCollection.
 *
 * \note Resulting dihedrals are distributed on (-M_PI, M_PI].
 */
double getDihedral(
  const Scine::Utils::PositionCollection& positions,
  AtomIndex i,
  AtomIndex j,
  AtomIndex k,
  AtomIndex l
);

/*!
 * Fetches the dihedral angle in radians defined by four indices in a
 * PositionCollection, but passed as an array
 *
 * \note Resulting dihedrals are distributed on (-M_PI, M_PI].
 */
double getDihedral(
  const Scine::Utils::PositionCollection& positions,
  const std::array<AtomIndex, 4>& indices
);

/*!
 * Returns the signed tetrahedron volume spanned by four indices in a
 * PositionCollection
 */
double getSignedVolume(
  const Scine::Utils::PositionCollection& positions,
  AtomIndex i,
  AtomIndex j,
  AtomIndex k,
  AtomIndex l
);

/*!
 * Returns the signed tetrahedron volume spanned by four indices in a
 * PositionCollection, but passed as an array
 */
double getSignedVolume(
  const Scine::Utils::PositionCollection& positions,
  const std::array<AtomIndex, 4>& indices
);

/* Reimplementation on vector basis alone */

Eigen::Vector3d averagePosition(
  const Scine::Utils::PositionCollection& positions,
  const std::vector<AtomIndex>& indices
);

double distance(
  const Eigen::Vector3d& i,
  const Eigen::Vector3d& j
);

double angle(
  const Eigen::Vector3d& i,
  const Eigen::Vector3d& j,
  const Eigen::Vector3d& k
);

/*! Calculates the dihedral between four positions
 *
 * \note Resulting dihedrals are distributed on (-M_PI, M_PI].
 */
double dihedral(
  const Eigen::Vector3d& i,
  const Eigen::Vector3d& j,
  const Eigen::Vector3d& k,
  const Eigen::Vector3d& l
);

/*!
 * Returns the signed tetrahedron volume spanned by four spatial positions
 * adjusted by V' = 6 * V
 */
double adjustedSignedVolume(
  const Eigen::Vector3d& i,
  const Eigen::Vector3d& j,
  const Eigen::Vector3d& k,
  const Eigen::Vector3d& l
);

} // namespace DelibHelpers

} // namespace molassembler

} // namespace Scine
#endif
