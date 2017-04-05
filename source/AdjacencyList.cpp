#include "AdjacencyList.h"
#include "CNStereocenter.h"
#include "EZStereocenter.h"
#include "CommonTrig.h"

#include "SymmetryFit.h"
#include "Log.h"

using namespace MoleculeManip;

/* Private members */
// Helper class to write the Graph as Graphviz output
struct AdjacencyList::MolGraphWriter {
  /* Settings to determine appearance */
  // Color maps
  const std::map<
    std::string,
    std::string
  > elementBGColorMap {
    {"H", "white"},
    {"C", "gray"},
    {"N", "blue"},
    {"O", "red"}
  };

  const std::map<
    std::string,
    std::string
  > elementTextColorMap {
    {"H", "black"},
    {"C", "white"},
    {"N", "white"},
    {"O", "white"}
  };

  const std::map<
    BondType,
    std::string
  > bondTypeDisplayString {
   {BondType::Single, "color = \"black\""},
    {BondType::Double, "color = \"black:invis:black\""},
    {BondType::Triple, "color = \"black:invis:black:invis:black\""},
    {BondType::Quadruple, "label = \"4\""},
    {BondType::Quintuple, "label = \"5\""},
    {BondType::Sextuple, "label = \"6\""},
    {BondType::Aromatic, "style = \"dashed\""},
    {BondType::Eta, "style = \"dotted\""}
  };

  /* State */
  // We promise to be good and not change anything
  const GraphType* const graphPtr;

  /* Constructor */
  MolGraphWriter(const GraphType* passGraphPtr) : graphPtr(passGraphPtr) {}

  /* Information */
  Delib::ElementType getElementType(const AtomIndexType& vertexIndex) const {
    return (*graphPtr)[vertexIndex].elementType;
  }

  // Global options
  void operator() (std::ostream& os) const {
    os << "graph [fontname = \"Arial\", layout = neato];\n"
      << "node [fontname = \"Arial\", shape = circle, style = filled];\n"
      << "edge [fontname = \"Arial\"];\n";
  }

  // Vertex options
  void operator() (std::ostream& os, const AtomIndexType& vertexIndex) const {
    const std::string symbolString = Delib::ElementInfo::symbol(
      getElementType(vertexIndex)
    );

    os << "[";
    
    // Add element name and index label
    os << "label = \"" << symbolString << vertexIndex << "\"";

    // Coloring
    if(elementBGColorMap.count(symbolString)) {
      os << ", fillcolor=\"" << elementBGColorMap.at(symbolString) << "\"";
    } else { // default
      os << ", fillcolor=\"white\"";
    }
    if(elementTextColorMap.count(symbolString)) {
      os << ", fontcolor=\"" << elementTextColorMap.at(symbolString) << "\"";
    } else { // default
      os << ", fontcolor=\"orange\"";
    }

    // Font sizing
    if(symbolString == "H") os << ", fontsize=10, width=.3, fixedsize=true";
    
    os << "]";
  }

  // Edge options
  void operator() (std::ostream& os, const EdgeIndexType& edgeIndex) const {
    os << "[";

    // Bond Type display options
    auto bondType = (*graphPtr)[edgeIndex].bondType;
    if(bondTypeDisplayString.count(bondType)) {
      os << bondTypeDisplayString.at(bondType);
    }

    // If one of the bonded atoms is a hydrogen, shorten the bond
    if(
      getElementType(
        boost::target(edgeIndex, *graphPtr)
      ) == Delib::ElementType::H
      || getElementType(
        boost::source(edgeIndex, *graphPtr)
      ) == Delib::ElementType::H
    ) {
      os << ", len=0.5";
    }

    os << "]";
  }
};

bool AdjacencyList::_isValidIndex(const AtomIndexType& index) const {
  return index < numAtoms();
}

std::vector<AtomIndexType> AdjacencyList::_getCNStereocenterCandidates() const {
  std::vector<AtomIndexType> candidates;

  for(AtomIndexType i = 0; i < numAtoms(); i++) {
    if(
      /* TODO this is no longer a valid way of checking how many ligands there are
       * -> eta bonds exist!
       */
      getNumAdjacencies(i) >= 3 
    ) candidates.push_back(i);
  }

  return candidates;
}

std::vector<EdgeIndexType> AdjacencyList::_getEZStereocenterCandidates() const {
  std::vector<EdgeIndexType> candidates;

  for(
    const auto& edgeIndex : 
    RangeForTemporary<GraphType::edge_iterator>(
      boost::edges(_adjacencies)
    )
  ) {
    auto source = boost::source(edgeIndex, _adjacencies),
         target = boost::target(edgeIndex, _adjacencies);

    if(
      _adjacencies[edgeIndex].bondType == BondType::Double
      && getNumNonEtaAdjacencies(source) == 3
      && getNumNonEtaAdjacencies(target) == 3
    ) {
      candidates.push_back(edgeIndex);
    }
  }

  return candidates;
}

std::vector<LocalGeometry::LigandType> AdjacencyList::_reduceToLigandTypes(
  const AtomIndexType& index
) const {
  /* TODO 
   * - No L, X determination. Although, will L, X even be needed for metals?
   *   Maybe only for OZ and NVE determination...
   */
  /* VSEPR formulation is that geometry is a function of 
   * - localized charge of central atom
   * - atom type of central atom, neighbors
   * - bond types to neighbors
   */

  // call this only on non-terminal atoms
  assert(getNumAdjacencies(index) > 1);

  // first basic stuff for VSEPR, later L and X for transition metals
  // geometry inference does not care if the substituents are somehow 
  // connected (unless in later models the entire structure is considered)
  std::vector<LocalGeometry::LigandType> ligands;

  for(const auto& adjacentIndex: iterateAdjacencies(index)) {
    ligands.push_back(
      LocalGeometry::LigandType {
        0, 0, {
          {  // L and X are 0 since only VSEPR is considered for now
            getElementType(adjacentIndex),
            getBondType(index, adjacentIndex).value()
          }
        }
      }
    );
  }

  return ligands;
}





/* Constructors */
AdjacencyList::AdjacencyList(
  const Delib::ElementTypeCollection& elements,
  const Edges& edges
) {
  for(const auto& element: elements) {
    addAtom(element);
  }

  for(const auto& edge: edges) {
    addBond(edge.first.first, edge.first.second, edge.second);
  }
}

AtomIndexType AdjacencyList::addAtom(
  const Delib::ElementType& elementType
) {
  auto vertex = boost::add_vertex(_adjacencies);
  _adjacencies[vertex].elementType = elementType;
  return vertex;
}

void AdjacencyList::addBond(
  const AtomIndexType& a,
  const AtomIndexType& b,
  const BondType& bondType
) {
  assert(_isValidIndex(a) && _isValidIndex(b) && a != b);
  auto edgeAddPair = boost::add_edge(a, b, _adjacencies);
  _adjacencies[edgeAddPair.first].bondType = bondType;
}

void AdjacencyList::changeElementType(
  const AtomIndexType& a,
  const Delib::ElementType& elementType
) {
  assert(_isValidIndex(a));
  _adjacencies[a].elementType = elementType;
}

void AdjacencyList::clear() {
  // Delete EVERYTHING
  _adjacencies.clear();
}

void AdjacencyList::removeAtom(const AtomIndexType& a) {
  // Remove all edges to and from this vertex
  boost::clear_vertex(a, _adjacencies);

  // Remove the vertex itself
  boost::remove_vertex(a, _adjacencies);
}

void AdjacencyList::removeBond(
  const AtomIndexType& a,
  const AtomIndexType& b
) {
  // Find edges
  auto edgePair = boost::edge(a, b, _adjacencies);
  if(edgePair.second) {
    boost::remove_edge(edgePair.first, _adjacencies);
  }
}

/* Information */
const GraphType& AdjacencyList::access() const {
  return _adjacencies;
}

bool AdjacencyList::isAdjacent(
  const AtomIndexType& a,
  const AtomIndexType& b
) const {
  GraphType::adjacency_iterator begin, end;
  std::tie(begin, end) = boost::adjacent_vertices(a, _adjacencies);
  return std::find(
    begin,
    end,
    b
  ) != end;
}

StereocenterList AdjacencyList::detectStereocenters() const {
  StereocenterList stereocenterList;

  // Find CNStereocenters
  for(const auto& candidateIndex : _getCNStereocenterCandidates()) {
    // Determine the local geometry
    auto localGeometryName = determineLocalGeometry(candidateIndex);
    auto rankResultPair = rankPriority(candidateIndex);

    // Construct a Stereocenter here
    std::shared_ptr<
      Stereocenters::CNStereocenter
    > newStereocenter = std::make_shared<
      Stereocenters::CNStereocenter
    >(
      localGeometryName,
      candidateIndex,
      rankResultPair.first,
      rankResultPair.second
    );

    /*std::cout << "Trial stereocenter: " << newStereocenter -> info() << std::endl;
    std::cout << "Ranked adjacent indices (low to high): vec{";
    for(const auto& adjacency: rankResultPair.first) {
      std::cout << adjacency;
      if(adjacency != rankResultPair.first.back()) std::cout << ", ";
    }
    std::cout << "}" << std::endl;

    std::cout << "Equal pairs: vec{";
    for(const auto& indexPair : rankResultPair.second) {
      std::cout << "(" << indexPair.first << ", " << indexPair.second << ")";
    }
    std::cout << "}" << std::endl;*/

    if(newStereocenter -> assignments() > 1) {
      stereocenterList.add(
        std::move(newStereocenter)
      );
    }
  }

  /* TODO
   * - Will need refinement to not instantiate EZStereocenters in small cycles
   *   (up to a preset size, maybe around 8 or so?)
   */
  // Find EZStereocenters
  for(
    const auto& edgeIndex : 
    _getEZStereocenterCandidates()
  ) {
    auto source = boost::source(edgeIndex, _adjacencies),
         target = boost::target(edgeIndex, _adjacencies);

    // Calculate Priorities for each's substituents
    auto sourceSubstituentsRanking = rankPriority(
      source,
      {target} // exclude edge sharing neighbor
    );

    // If the source's substituents are unequal (no equal pair sets)
    if(sourceSubstituentsRanking.second.size() == 0) {
      auto targetSubstituentsRanking = rankPriority(
        target,
        {source} // exclude edge sharing neighbor
      );

      // target must also have no equal pairs
      if(targetSubstituentsRanking.second.size() == 0) {
        // Instantiate an EZStereocenter there!
        stereocenterList.add(
          std::make_shared<
            Stereocenters::EZStereocenter
          >(
            source,
            sourceSubstituentsRanking.first,
            target,
            targetSubstituentsRanking.first
          )
        );
      }
    }
  }

  return stereocenterList;
}

Symmetry::Name AdjacencyList::determineLocalGeometry(
  const AtomIndexType& index
) const {
  assert(getNumAdjacencies(index) > 1); 

  auto ligandsVector = _reduceToLigandTypes(index);

  // TODO this below is invalid for metals!
  unsigned nSites = getNumAdjacencies(index);
  int formalCharge = 0;

  if(AtomInfo::isMainGroupElement(getElementType(index))) {
    return LocalGeometry::VSEPR::determineGeometry(
      getElementType(index),
      nSites,
      ligandsVector,
      formalCharge
    );
  } else {
    auto findIter = std::find_if(
      Symmetry::allNames.begin(),
      Symmetry::allNames.end(),
      [&nSites](const auto& symmetryName) -> bool {
        return Symmetry::size(symmetryName) == nSites;
      }
    );

    if(findIter == Symmetry::allNames.end()) {
      throw std::logic_error(
        "Could not find a suitable local geometry!"
      );
    }

    return *findIter;
  }
}

unsigned AdjacencyList::getNumAdjacencies(
  const AtomIndexType& a
) const {
  return boost::out_degree(a, _adjacencies);
}

unsigned AdjacencyList::getNumNonEtaAdjacencies(
  const AtomIndexType& a
) const {
  unsigned count = 0;

  for(
    const auto& edgeIndex:
    RangeForTemporary<GraphType::out_edge_iterator>(
      boost::out_edges(a, _adjacencies)
    )
  ) {
    if(_adjacencies[edgeIndex].bondType != BondType::Eta) {
      count += 1;
    }
  }

  return count;
}

/*! Returns a range-for temporary object allowing c++11 style for loop 
 * iteration through an atom's adjacencies
 */
RangeForTemporary<GraphType::adjacency_iterator> AdjacencyList::iterateAdjacencies(
  const AtomIndexType& a
) const {
  return RangeForTemporary<
    GraphType::adjacency_iterator
  >(
    boost::adjacent_vertices(a, _adjacencies)
  );
}

/* TODO
 * - does not treat correctly:
 *   - cycles
 *   - stereocenters (Z over E, R over S (?))
 *   - double and triple bond ghost atom splitting
 * - unsure about sub-lists. is this approach even remotely correct?
 * - FUCK CIP rules -> maybe just use the unsigned values of assignments in
 *   GraphFeatures and rank branches with that.
 * - test
 */
std::pair<
  std::vector<AtomIndexType>, // the sorted list of substituent priorities
  std::set< // a set of pairs of AtomIndexTypes that are EQUAL
    std::pair<
      AtomIndexType,
      AtomIndexType
    >
  >
> AdjacencyList::rankPriority(
  const AtomIndexType& a,
  const std::vector<AtomIndexType>& excludeAdjacent 
) const {
  auto toRank = getAdjacencies(a);
  std::set<
    std::pair<
      AtomIndexType,
      AtomIndexType
    >
  > equalPairs;

  // remove excludes from toRank
  toRank.erase(
    std::remove_if(
      toRank.begin(),
      toRank.end(),
      [&excludeAdjacent](const AtomIndexType& atomIndex) {
        return std::find(
          excludeAdjacent.begin(),
          excludeAdjacent.end(),
          atomIndex
        ) != excludeAdjacent.end();
      }
    ),
    toRank.end()
  );

  auto getZ = [this](const AtomIndexType& a) -> int {
    return static_cast<int>(getElementType(a));
  };

  auto BFSIterate = [this, getZ](
    std::set<AtomIndexType>& visitedSet,
    std::vector<AtomIndexType>& seeds,
    std::multiset<int, std::greater<int> >& Zs
  ) {
    std::vector<AtomIndexType> newSeeds;
    for(const auto& index : seeds) {
      for(const auto& potentialSeed : iterateAdjacencies(index)) {
        // if not in visitedSet
        if(visitedSet.count(potentialSeed) == 0) {
          // add it to the set and new seeds
          visitedSet.insert(potentialSeed);
          newSeeds.push_back(potentialSeed);

          // add it's Z to Zs
          Zs.insert(getZ(potentialSeed));
        } // else skip
      }
    }

    // overwrite seeds
    seeds = newSeeds;
  };
  
  // sort toRank according to CIP-like rules
  std::sort(
    toRank.begin(),
    toRank.end(),
    [&a, this, &getZ, &BFSIterate, &equalPairs](
      const AtomIndexType& lhs,
      const AtomIndexType& rhs
    ) {
      std::set<AtomIndexType> lhsVisited = {a}, rhsVisited = {a};
      std::vector<AtomIndexType> lhsSeeds = {lhs}, rhsSeeds = {rhs};
      std::multiset<
        int,
        std::greater<int> // in CIP, list of Z is ordered DESC
      > lhsZ = { getZ(lhs) }, rhsZ = { getZ(rhs) };
      while(
          lhsSeeds.size() > 0
          || rhsSeeds.size() > 0
      ) {
        // compare lists
        if(lhsZ < rhsZ) return true;
        else if(lhsZ > rhsZ) return false;

        // iterate along the bonds
        BFSIterate(lhsVisited, lhsSeeds, lhsZ);
        BFSIterate(rhsVisited, rhsSeeds, rhsZ);
      }

      // all equal -> add to equalPairs
      equalPairs.emplace(
        std::min(lhs, rhs),
        std::max(lhs, rhs)
      );
      return false;
    }
  );

  return {
    toRank,
    equalPairs
  };
}

// Creates a copy of the contained data suitable for the Edges class
std::vector<AdjacencyList::ExplicitEdge> AdjacencyList::getEdges() const {
  std::vector<ExplicitEdge> edges;

  for(
    const auto& edgeIndex : 
    RangeForTemporary<
      GraphType::edge_iterator
    >(boost::edges(_adjacencies))
  ) {
    edges.push_back(ExplicitEdge({
      {boost::source(edgeIndex, _adjacencies), boost::target(edgeIndex, _adjacencies)},
      _adjacencies[edgeIndex].bondType
    }));
  }

  return edges;
}

std::vector<AtomIndexType> AdjacencyList::getAdjacencies(
  const AtomIndexType& a
) const {
  std::vector<AtomIndexType> copy;

  // C++17 auto [begin, end] = ...
  GraphType::adjacency_iterator begin, end;
  std::tie(begin, end) = boost::adjacent_vertices(a, _adjacencies);
  std::copy(
    begin,
    end,
    std::back_inserter(copy)
  );

  return copy;
}

Delib::ElementType AdjacencyList::getElementType(const AtomIndexType& index) const {
  assert(_isValidIndex(index));
  return _adjacencies[index].elementType;
}

boost::optional<BondType> AdjacencyList::getBondType(
  const AtomIndexType& a,
  const AtomIndexType& b
) const {
  auto edgePair = boost::edge(a, b, _adjacencies);

  if(edgePair.second) {
    return _adjacencies[edgePair.first].bondType;
  } else return boost::none;

}

StereocenterList AdjacencyList::inferStereocentersFromPositions(
  const Delib::PositionCollection& positions
) const {
  StereocenterList stereocenters;

  /* Add a CNStereocenter everywhere where the symmetry yielding the best fit is 
   * not the one that AdjacencyList's determineLocalGeometry gets and where we
   * can fully determine a Stereocenter's assignment from the positions
   */
  for(unsigned candidateIndex = 0; candidateIndex < numAtoms(); candidateIndex++) {
    // Skip terminal atoms
    if(getNumAdjacencies(candidateIndex) <= 1) continue;

    // Determine the local geometry
    const auto localGeometryName = determineLocalGeometry(candidateIndex);
    auto rankResultPair = rankPriority(candidateIndex);

    // Construct it
    std::shared_ptr<
      Stereocenters::CNStereocenter
    > stereocenterPtr = std::make_shared<
      Stereocenters::CNStereocenter
    >(
      localGeometryName,
      candidateIndex,
      rankResultPair.first,
      rankResultPair.second
    );

    // Downcast the Stereocenter to a CNStereocenter
    auto CNStereocenterPtr = std::dynamic_pointer_cast<
      Stereocenters::CNStereocenter
    >(stereocenterPtr);

    // Perform the fit
    SymmetryFit fit {
      CNStereocenterPtr,
      getAdjacencies(
        CNStereocenterPtr -> centerAtom
      ),
      positions
    };

#ifndef NDEBUG
    using ::operator<<; // use global namespace ostream operators

    // Log in Debug builds, provided particular is set
    Log::log(Log::Particulars::StereocenterFitAnalysisInfo) << fit;
#endif

    /* Cases
     * Best symmetry is equal to localGeometry and no assignment can be made
     *  -> no stereocenter
     * Best symmetry is equal to localGeometry and an assignment can be made
     *  -> add stereocenter, set assignment
     * Best symmetry is unequal to localGeometry and no assignment can be made
     *  -> add stereocenter
     * Best symmetry is unequal to localGeometry and an assignment can be made
     *  -> add stereocenter, set assignment
     * Best symmetry is unequal and only one assignment exists
     *  -> add stereocenter, set assignment
     */

    // Set it to the best symmetry
    if(
      !( // Only case when stereocenter is not added
        fit.bestSymmetry == localGeometryName
        && fit.assignmentsWithLowestDeviation.size() > 1
      )
    ) {
      CNStereocenterPtr -> changeSymmetry(fit.bestSymmetry);

      /* If that pair with lowest deviation has only a single assignment,
       * assign it, else leave it unassigned (it has only a single assignment 
       * also if only one unique assignment exists)
       */
      if(fit.assignmentsWithLowestDeviation.size() == 1) {
        CNStereocenterPtr -> assign(
          fit.assignmentsWithLowestDeviation.front()
        );
      }

      // add it to the list
      stereocenters.add(stereocenterPtr);
    }

  }

  // TODO EZStereocenters
  /* NOTES
   * - CNStereocenter detection may have generated trigonal planar
   *   stereocenters on the endpoints of the double bond edge -> remove if an
   *   EZStereocenter is instantiated there instead
   *
   * STEPS
   * - Calculate dihedral angle of high-priority pair from 3D
   *   -> Select E/Z within tolerance of 0° / 180° endpoints
   *   -> Throw outside of those tolerances
   */

  return stereocenters;
}

unsigned AdjacencyList::numAtoms() const {
  return boost::num_vertices(_adjacencies);
}

unsigned AdjacencyList::numBonds() const {
  return boost::num_edges(_adjacencies);
}

void AdjacencyList::dumpGraphviz(const std::string& filename) const {
  MolGraphWriter propertyWriter(&_adjacencies);

  std::ofstream outStream(filename);

  boost::write_graphviz(
    outStream,
    _adjacencies,
    propertyWriter,
    propertyWriter,
    propertyWriter
  );

  outStream.close();
}

/* Operators */
RangeForTemporary<GraphType::adjacency_iterator> AdjacencyList::operator[](
  const AtomIndexType& a
) const {
  return RangeForTemporary<
    GraphType::adjacency_iterator
  >(
    boost::adjacent_vertices(a, _adjacencies)
  );
}
