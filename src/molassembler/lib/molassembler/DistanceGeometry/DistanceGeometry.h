#ifndef DISTANCE_GEOMETRY_HPP
#define DISTANCE_GEOMETRY_HPP

#include <tuple>
#include "common_typedefs.h"
#include "DistanceGeometry/ValueBounds.h"

/*! @file
 *
 * Contains some central data class declarations and type definitions for the
 * entire Distance Geometry scheme.
 */

namespace molassembler {

//! Distance geometry-related classes and functions
namespace DistanceGeometry {

struct ChiralityConstraint {
  std::array<AtomIndexType, 4> indices;
  double lower, upper;

  ChiralityConstraint(
    const std::array<AtomIndexType, 4>& indices,
    const double& lower,
    const double& upper
  ) : indices(indices),
      lower(lower),
      upper(upper)
  {
    // Must be <= because flat targets have lower = upper = 0
    assert(lower <= upper);
  }
};

enum class Partiality {
  FourAtom,
  TenPercent,
  All
};

} // namespace DistanceGeometry

} // namespace molassembler

#endif