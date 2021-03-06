/*!@file
 * @copyright This code is licensed under the 3-clause BSD license.
 *   Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.
 *   See LICENSE.txt for details.
 * @brief Calculate shape position maps
 */

#ifndef INCLUDE_MOLASSEMBLER_STEREOPERMUTATORS_SYMMETRY_POSITION_MAPS_H
#define INCLUDE_MOLASSEMBLER_STEREOPERMUTATORS_SYMMETRY_POSITION_MAPS_H

#include "Molassembler/RankingInformation.h"

#include "Molassembler/Temple/StrongIndexMap.h"
#include "Molassembler/Shapes/Data.h"

namespace Scine {
namespace Molassembler {
namespace Stereopermutations {
class Stereopermutation;
} // namespace Stereopermutations

using SiteToShapeVertexMap = Temple::StrongIndexFlatMap<SiteIndex, Shapes::Vertex>;

/*! @brief Generates a flat mapping from site indices to shape vertices
 *
 * Generates a mapping from site indices to shape vertices according to
 * the ranking character distribution to shape vertex of a stereopermutation
 * (its characters member) and any defined links between shape positions.
 *
 * @code{.cpp}
 * auto mapping = siteToShapeVertexMap(...);
 * unsigned symmetryPositionOfSiteFour = mapping.at(4u);
 * @endcode
 *
 * @complexity{@math{\Theta(N)}}
 */
SiteToShapeVertexMap siteToShapeVertexMap(
  const Stereopermutations::Stereopermutation& stereopermutation,
  const RankingInformation::RankedSitesType& canonicalSites,
  const std::vector<RankingInformation::Link>& siteLinks
);

/*! @brief Generates a flat mapping from shape vertices to site indices
 *
 * Generates exactly the inverse map to generateSiteToSymmetryPositionMap
 *
 * @code{cpp}
 * auto mapping = shapeVertexToSiteIndexMap(...);
 * unsigned siteIndexAtSymmetryPositionFive = mapping.at(5u);
 * @endcode
 *
 * @complexity{@math{\Theta(N)}}
 */
Temple::StrongIndexFlatMap<Shapes::Vertex, SiteIndex> shapeVertexToSiteIndexMap(
  const Stereopermutations::Stereopermutation& stereopermutation,
  const RankingInformation::RankedSitesType& canonicalSites,
  const std::vector<RankingInformation::Link>& siteLinks
);

Stereopermutations::Stereopermutation stereopermutationFromSiteToShapeVertexMap(
  const SiteToShapeVertexMap& siteToShapeVertexMap,
  const std::vector<RankingInformation::Link>& links,
  const RankingInformation::RankedSitesType& canonicalSites
);

} // namespace Molassembler
} // namespace Scine

#endif
