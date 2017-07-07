#ifndef INCLUDE_CYCLIC_POLYGONS_LIB_H
#define INCLUDE_CYCLIC_POLYGONS_LIB_H

#include <boost/math/tools/roots.hpp>
#include "template_magic/Containers.h"
#include "template_magic/Numeric.h"

#include <vector>
#include <cassert>

/*! @file 
 *
 * This is a header-only collection of the best-functioning subset of
 * functionality from CyclicPolygons.h. Provides shortcut calculations of 
 * the internal angles for triangles and quadrilaterals, which are easier to
 * treat. Internal angle calculation is generalized for any number of edges.
 */

namespace CyclicPolygons {

namespace detail {

template<typename FloatType>
inline FloatType inverseLawOfCosines(
  const FloatType& opposingSideLength,
  const FloatType& adjacentSideLengthA,
  const FloatType& adjacentSideLengthB
) {
  return std::acos(
    (
      adjacentSideLengthA * adjacentSideLengthA
      + adjacentSideLengthB * adjacentSideLengthB
      - opposingSideLength * opposingSideLength
    ) / (
      2 * adjacentSideLengthA * adjacentSideLengthB
    )
  );
}

template<typename FloatType>
std::vector<FloatType> triangleShortcut(const std::vector<FloatType>& edgeLengths) {
  return {
    inverseLawOfCosines(edgeLengths[2], edgeLengths[0], edgeLengths[1]),
    inverseLawOfCosines(edgeLengths[0], edgeLengths[1], edgeLengths[2]),
    inverseLawOfCosines(edgeLengths[1], edgeLengths[0], edgeLengths[2])
  };
}

template<typename T>
inline T square(const T& value) {
  return value * value;
}

/* For a cyclic quadrilateral, the internal angle between adjacent edges a and b
 * is given as
 *
 *               a² + b² - c² - d²
 *    cos(phi) = -----------------
 *                 2 (ab + cd)
 *
 * This general structure from adjacent and non-adjacent edge lengths is
 * calculated below.
 */
template<typename FloatType>
FloatType quadrilateralInternalAngle(
  const std::vector<FloatType>& edgeLengths,
  const std::array<unsigned, 2>& adjacentIndices,
  const std::array<unsigned, 2>& nonAdjacentIndices
) {
  return std::acos(
    (
      square(edgeLengths.at(adjacentIndices[0]))
      + square(edgeLengths.at(adjacentIndices[1]))
      - square(edgeLengths.at(nonAdjacentIndices[0]))
      - square(edgeLengths.at(nonAdjacentIndices[1]))
    ) / (
      2 * (
        edgeLengths.at(adjacentIndices[0]) * edgeLengths.at(adjacentIndices[1])
        + edgeLengths.at(nonAdjacentIndices[0]) * edgeLengths.at(nonAdjacentIndices[1])
      )
    )
  );
}

template<typename FloatType>
std::vector<FloatType> quadrilateralShortcut(const std::vector<FloatType>& edgeLengths) {
  return {
    quadrilateralInternalAngle(edgeLengths, {0, 1}, {2, 3}),
    quadrilateralInternalAngle(edgeLengths, {1, 2}, {3, 0}),
    quadrilateralInternalAngle(edgeLengths, {2, 3}, {0, 1}),
    quadrilateralInternalAngle(edgeLengths, {3, 0}, {1, 2}),
  };
}

template<typename FloatType>
std::vector<FloatType> centralAngles(
  const FloatType& circumradius,
  const std::vector<FloatType>& edgeLengths
) {
  return TemplateMagic::map(
    edgeLengths,
    [&](const FloatType& edgeLength) -> FloatType {
      return std::acos(
        1 - (edgeLength * edgeLength) / (
          2 * circumradius * circumradius
        )
      );
    }
  );
}

template<typename FloatType>
FloatType centralAnglesDeviation(
  const FloatType& circumradius,
  const std::vector<FloatType>& edgeLengths
) {
  assert(edgeLengths.size() == 5);
  assert(circumradius > TemplateMagic::max(edgeLengths) / 2);

  return TemplateMagic::sum(
    centralAngles(circumradius, edgeLengths)
  ) - 2 * M_PI;
}

template<typename FloatType>
FloatType centralAnglesDeviationDerivative(
  const FloatType& circumradius,
  const std::vector<FloatType>& edgeLengths
) {
  return TemplateMagic::sum(
    TemplateMagic::map(
      edgeLengths,
      [&](const FloatType& a) -> FloatType {
        return -2 * a / (
          circumradius * std::sqrt(
            4 * circumradius * circumradius - a * a
          )
        );
      }
    )
  );
}

template<typename FloatType>
FloatType centralAnglesDeviationSecondDerivative(
  const FloatType& circumradius,
  const std::vector<FloatType>& edgeLengths
) {
  const FloatType squareCircumradius = circumradius * circumradius;

  return TemplateMagic::sum(
    TemplateMagic::map(
      edgeLengths,
      [&](const FloatType& a) -> FloatType {
        const auto temp = 4 * squareCircumradius - a * a;
        return -2 * a * (
          - 4 * std::pow(temp, -1.5)
          - std::pow(temp, -0.5) / (
            squareCircumradius
          )
        );
      }
    )
  );
}

template<typename FloatType>
FloatType regularCircumradius(
  const unsigned& nSides,
  const FloatType& a
) {
  return 0.5 * a / std::sin(M_PI / nSides);
}

template<typename FloatType>
FloatType convexCircumradius(const std::vector<FloatType>& edgeLengths) {
  const FloatType minR = TemplateMagic::max(edgeLengths) / 2 + 1e-10;
  const FloatType lowerBound = std::max(
    regularCircumradius(
      edgeLengths.size(),
      TemplateMagic::min(edgeLengths)
    ),
    minR
  );
  const FloatType upperBound = std::max(
    regularCircumradius(
      edgeLengths.size(),
      TemplateMagic::max(edgeLengths)
    ), 
    minR
  );
  const FloatType rootGuess = regularCircumradius(
    edgeLengths.size(),
    std::max(
      TemplateMagic::average(edgeLengths),
      minR
    )
  );

  auto rootSearchLambda = [&](const FloatType& circumradius) -> std::tuple<FloatType, FloatType, FloatType> {
    return std::make_tuple<FloatType, FloatType, FloatType>(
      centralAnglesDeviation(circumradius, edgeLengths),
      centralAnglesDeviationDerivative(circumradius, edgeLengths),
      centralAnglesDeviationSecondDerivative(circumradius, edgeLengths)
    );
  };

  const unsigned maxIter = 1000;
  boost::uintmax_t iterCount = maxIter;

  auto root = boost::math::tools::schroder_iterate(
    rootSearchLambda,
    rootGuess,
    lowerBound,
    upperBound,
    32, // bits precision
    iterCount
  );

  if(iterCount == static_cast<boost::uintmax_t>(maxIter)) {
    throw std::logic_error("Could not find pentagon circumradius!");
  }

  return root;
}

template<typename FloatType>
std::vector<FloatType> generalizedInternalAngles(
  const std::vector<FloatType>& edgeLengths,
  const FloatType& circumradius
) {
  // Immediately multiply with 2 to avoid doing so in every calculation
  const FloatType doubleR = 2 * circumradius;

  /* Add the first edge length onto the list so we can use pairwiseMap to get
   * all subsequent pairs
   */
  auto lengthsCopy = edgeLengths;
  lengthsCopy.emplace_back(lengthsCopy.front());

  return TemplateMagic::mapSequentialPairs(
    lengthsCopy,
    [&doubleR](const FloatType& a, const FloatType& b) -> FloatType {
      return (
        std::acos(
          a / doubleR
        ) + std::acos(
          b / doubleR
        )
      );
    }
  );
}

} // namespace detail


/*! 
 * Returns whether a cyclic polygon exists for the specified sequence of edge
 * lengths.
 */
template<typename FloatType>
bool exists(const std::vector<FloatType>& edgeLengths) {
  /* If a1, a2, ..., aN satisfy: Each edge length smaller than sum of others
   * -> There exists a convex cyclic polygon (Iosif Pinelis, 2005)
   *
   * Equivalent to saying largest value in set of edge lengths smaller than 
   * remainder, no need to check all of them.
   */

  const FloatType maxValue = TemplateMagic::max(edgeLengths);

  return (
    maxValue <
    TemplateMagic::sum(edgeLengths) - maxValue 
  );
}

/*! 
 * Returns internal angles of the convex cyclic polygon specified by the passed
 * edge lengths. Angles are returned in the following sequence:
 *
 *   edge lengths: a1, a2, ..., aN
 *   angles: a1 ∡ a2, a2 ∡ a3, ..., a(N-1) ∡ aN, aN ∡ a1
 *
 * Requires that the passed vector of edge lengths contains at minimum 3 edges.
 * The cyclic polygon must exist (use exists to check beforehand)
 * and no edge length may be zero (assumes logical error in calling code).
 */
template<typename FloatType>
std::enable_if_t<
  std::is_floating_point<FloatType>::value,
  std::vector<FloatType>
> internalAngles(const std::vector<FloatType>& edgeLengths) {
  assert(
    exists(edgeLengths)
    && "The passed sequence of lengths cannot be used to construct a polygon. "
      "An edge length surpassed the sum of the lengths of all others. This is "
      "the necessary condition for the existence of a cyclic polygon."
  );
  assert(
    edgeLengths.size() >= 3
    && "It is unreasonable to call this for less than three edges."
  );
  assert( 
    TemplateMagic::all_of(
      TemplateMagic::map(
        edgeLengths,
        [&](const FloatType& length) -> bool {
          return length != 0;
        }
      )
    ) && "At least one edge length in the sequence is zero "
      "Perhaps consider removing it from the set and approximating it as the "
      "next smaller polygon."
  );

  if(edgeLengths.size() == 3) {
    return detail::triangleShortcut<FloatType>(edgeLengths);
  } 
  
  if(edgeLengths.size() == 4) {
    return detail::quadrilateralShortcut<FloatType>(edgeLengths);
  } 

  // General solving scheme
  return detail::generalizedInternalAngles(
    edgeLengths, 
    detail::convexCircumradius(edgeLengths)
  );
}

} // namespace CyclicPolygons

#endif
