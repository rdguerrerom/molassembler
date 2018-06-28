#include "AtomStereocenter.h"

#include <Eigen/Dense>

#include "chemical_symmetries/Properties.h"
#include "CyclicPolygons.h"
#include "temple/constexpr/ConsecutiveCompare.h"
#include "temple/Containers.h"
#include "temple/constexpr/Numeric.h"
#include "temple/Optionals.h"
#include "temple/Random.h"

#include "DistanceGeometry/SpatialModel.h"
#include "DistanceGeometry/DistanceGeometry.h"
#include "detail/BuildTypeSwitch.h"
#include "detail/CommonTrig.h"
#include "detail/DelibHelpers.h"
#include "detail/StdlibTypeAlgorithms.h"
#include "Log.h"

#include <iomanip>

namespace molassembler {

namespace Stereocenters {

AtomStereocenter::PermutationState::PermutationState(
  const RankingInformation& ranking,
  const AtomIndexType centerAtom,
  const Symmetry::Name symmetry,
  const GraphType& graph
) {
  canonicalLigands = canonicalize(ranking.ligandsRanking);
  symbolicCharacters = transferToSymbolicCharacters(canonicalLigands);
  selfReferentialLinks = selfReferentialTransform(
    ranking.links,
    canonicalLigands
  );

  using ModelType = DistanceGeometry::SpatialModel;

  ligandDistances = temple::mapToVector(
    ranking.ligands,
    [&](const auto& ligandIndices) -> DistanceGeometry::ValueBounds {
      return ModelType::ligandDistanceFromCenter(
        ligandIndices,
        centerAtom,
        ModelType::bondRelativeVariance,
        graph
      );
    }
  );

  Cycles etaLessCycles {graph, true};
  coneAngles.reserve(ranking.ligands.size());

  for(unsigned i = 0; i < ranking.ligands.size(); ++i) {
    coneAngles.push_back(
      ModelType::coneAngle(
        ranking.ligands.at(i),
        ligandDistances.at(i),
        ModelType::bondRelativeVariance,
        graph,
        etaLessCycles
      )
    );
  }

  permutations = stereopermutation::uniquesWithWeights(
    StereopermutationType {
      symmetry,
      symbolicCharacters,
      selfReferentialLinks
    },
    symmetry,
    false // Do NOT remove trans-spanning linked groups
  );

  // Determine which permutations are feasible and which aren't
  if(
    // Links are present
    !ranking.links.empty()
    // OR there are haptic ligands
    || temple::sum(
      temple::map(
        ranking.ligands,
        [](const auto& ligandIndices) -> unsigned {
          if(ligandIndices.size() == 1) {
            return 0;
          }

          return 1;
        }
      )
    ) > 0
  ) {
    Cycles cycleData {graph};
    feasiblePermutations.reserve(permutations.assignments.size());
    const unsigned P = permutations.assignments.size();
    for(unsigned i = 0; i < P; ++i) {
      if(
        isFeasibleStereopermutation(
          permutations.assignments.at(i),
          canonicalLigands,
          coneAngles,
          ranking,
          symmetry,
          graph
        )
      ) {
        feasiblePermutations.push_back(i);
      }
    }
    feasiblePermutations.shrink_to_fit();
  } else {
    feasiblePermutations = temple::iota<unsigned>(permutations.assignments.size());
  }
}

RankingInformation::RankedLigandsType AtomStereocenter::PermutationState::canonicalize(
  RankingInformation::RankedLigandsType rankedLigands
) {
  std::stable_sort(
    rankedLigands.begin(),
    rankedLigands.end(),
    [](const auto& setA, const auto& setB) -> bool {
      // Inverted comparison so that larger sets come first
      return setA.size() > setB.size();
    }
  );

  return rankedLigands;
}

// Transform canonical ranked ligands to canonical characters
std::vector<char> AtomStereocenter::PermutationState::transferToSymbolicCharacters(
  const RankingInformation::RankedLigandsType& canonicalLigands
) {
  std::vector<char> characters;

  char currentChar = 'A';
  for(const auto& equalPrioritySet : canonicalLigands) {
    for(unsigned i = 0; i < equalPrioritySet.size(); ++i) {
      characters.push_back(currentChar);
    }

    ++currentChar;
  }

  return characters;
}

stereopermutation::Stereopermutation::LinksSetType
AtomStereocenter::PermutationState::selfReferentialTransform(
  const RankingInformation::LinksType& rankingLinks,
  const RankingInformation::RankedLigandsType& canonicalLigands
) {
  stereopermutation::Stereopermutation::LinksSetType links;

  for(const auto& link : rankingLinks) {
    auto getRankedPosition = [&canonicalLigands](const AtomIndexType ligandIndex) -> unsigned {
      unsigned position = 0;
      for(const auto& equalLigandsSet : canonicalLigands) {
        for(const auto& rankedLigandIndex : equalLigandsSet) {
          if(rankedLigandIndex == ligandIndex) {
            return position;
          }

          ++position;
        }
      }

      throw std::logic_error("Ligand index not found in ranked ligands");
    };

    auto a = getRankedPosition(link.indexPair.first),
         b = getRankedPosition(link.indexPair.second);

    links.emplace(
      std::min(a, b),
      std::max(a, b)
    );
  }

  return links;
}

std::vector<unsigned>
AtomStereocenter::PermutationState::generateLigandToSymmetryPositionMap(
  const stereopermutation::Stereopermutation& assignment,
  const RankingInformation::RankedLigandsType& canonicalLigands
) {
  /* TODO NO Stereopermutation link information is used here!
   *
   * Need to ensure that
   * AAAAAA {0, 1}, {2, 3}, {4, 5}
   * and AAAAAA {0, 1}, {2, 4}, {3, 5}
   * are different!
   *
   * AABCAD {0,1}, {0, 4}, {1, 4} has to work too
   * - cannot use index of symmetry position to fetch atom index within equal priority groups
   * - must be two-stage algorithm that fixes linked symmetry positions first, then distributes
   *   the remaining
   */

  constexpr unsigned placeholder = std::numeric_limits<unsigned>::max();

  std::vector<unsigned> positionMap (
    assignment.characters.size(),
    placeholder
  );

  /* First, process the links.
   *
   * For every atom index within the group of indices of equal priority, we
   * have to keep information on which have been used and which haven't.
   */
  auto usedLists = temple::map(
    canonicalLigands,
    [](const auto& equalPrioritySet) -> std::vector<bool> {
      return std::vector<bool>(equalPrioritySet.size(), false);
    }
  );

  std::vector<
    std::vector<unsigned>
  > availableSymmetryPositions;

  for(char i = 'A'; i <= temple::max(assignment.characters); ++i) {
    std::vector<unsigned> positions;

    for(unsigned s = 0; s < assignment.characters.size(); ++s) {
      if(assignment.characters.at(s) == i) {
        positions.push_back(s);
      }
    }

    availableSymmetryPositions.emplace_back(
      std::move(positions)
    );
  }

  auto placeAndMark = [&](const unsigned& symmetryPositionIndex) {
    char priority = assignment.characters.at(symmetryPositionIndex);

    unsigned countUpToPosition = std::count(
      assignment.characters.begin(),
      assignment.characters.begin() + symmetryPositionIndex,
      priority
    );

    unsigned correspondingLigand = canonicalLigands.at(priority - 'A').at(countUpToPosition);

    if(positionMap.at(correspondingLigand) == placeholder) {
      unsigned newSymmetryPosition = availableSymmetryPositions.at(priority - 'A').front();

      positionMap.at(correspondingLigand) = newSymmetryPosition;

      availableSymmetryPositions.at(priority - 'A').erase(
        availableSymmetryPositions.at(priority - 'A').begin()
      );

      usedLists.at(priority - 'A').at(countUpToPosition) = true;
    }
  };

  for(const auto& link: assignment.links) {
    placeAndMark(link.first);
    placeAndMark(link.second);
  }

  // Next, process the characters
  for(const auto& priorityChar : assignment.characters) {
    // Get an unused atom index for that priority
    auto unusedIndexIter = std::find(
      usedLists.at(priorityChar - 'A').begin(),
      usedLists.at(priorityChar - 'A').end(),
      false
    );

    if(unusedIndexIter != usedLists.at(priorityChar - 'A').end()) {
      unsigned correspondingLigand = canonicalLigands.at(priorityChar - 'A').at(
        unusedIndexIter - usedLists.at(priorityChar - 'A').begin()
      );

      assert(positionMap.at(correspondingLigand) == placeholder);

      unsigned symmetryPosition = availableSymmetryPositions.at(priorityChar - 'A').front();

      availableSymmetryPositions.at(priorityChar - 'A').erase(
        availableSymmetryPositions.at(priorityChar - 'A').begin()
      );

      positionMap.at(correspondingLigand) = symmetryPosition;

      *unusedIndexIter = true;
    }
  }

  assert(
    temple::all_of(
      positionMap,
      [](const unsigned& symmetryPosition) -> bool {
        return symmetryPosition != placeholder;
      }
    ) && "A symmetry position is still marked with the placeholder!"
  );

  return positionMap;
}

std::vector<unsigned> AtomStereocenter::PermutationState::generateSymmetryPositionToLigandMap(
  const stereopermutation::Stereopermutation& assignment,
  const RankingInformation::RankedLigandsType& canonicalLigands
) {
  auto base = generateLigandToSymmetryPositionMap(assignment, canonicalLigands);

  std::vector<unsigned> inverseMap (base.size());

  for(unsigned i = 0; i < base.size(); ++i) {
    inverseMap.at(base.at(i)) = i;
  }

  return inverseMap;
}

std::vector<char> AtomStereocenter::PermutationState::makeStereopermutationCharacters(
  const RankingInformation::RankedLigandsType& canonicalLigands,
  const std::vector<char>& canonicalStereopermutationCharacters,
  const std::vector<unsigned>& ligandsAtSymmetryPositions
) {
  // Replace the ligand indices by their new ranking characters
  std::vector<unsigned> flattenedIndices;
  for(const auto& equalPrioritySet : canonicalLigands) {
    for(const auto& index : equalPrioritySet) {
      flattenedIndices.push_back(index);
    }
  }

  std::vector<char> newStereopermutationCharacters;

  for(const auto& index : ligandsAtSymmetryPositions) {
    const auto findIter = std::find(
      flattenedIndices.begin(),
      flattenedIndices.end(),
      index
    );

    newStereopermutationCharacters.push_back(
      canonicalStereopermutationCharacters.at(
        findIter - flattenedIndices.begin()
      )
    );
  }

  return newStereopermutationCharacters;
}

/* WARNING: This has to be a copy-initialized optional. Don't change it, unless
 * you want to sift through -fsanitize=address output to find the bug in the
 * optional propagation. It's not safe to return a reference to within a
 * temporary object where this is used.
 */
boost::optional<std::vector<unsigned>> AtomStereocenter::PermutationState::getIndexMapping(
  const Symmetry::properties::SymmetryTransitionGroup& mappingsGroup,
  const ChiralStatePreservation& preservationOption
) {
  if(mappingsGroup.indexMappings.empty()) {
    return boost::none;
  }

  if(preservationOption == ChiralStatePreservation::EffortlessAndUnique) {
    if(
      mappingsGroup.indexMappings.size() == 1
      && mappingsGroup.angularDistortion <= 0.2
    ) {
      return mappingsGroup.indexMappings.front();
    }

    return boost::none;
  }

  if(preservationOption == ChiralStatePreservation::Unique) {
    if(mappingsGroup.indexMappings.size() == 1) {
      return mappingsGroup.indexMappings.front();
    }

    return boost::none;
  }

  if(preservationOption == ChiralStatePreservation::RandomFromMultipleBest) {
    return mappingsGroup.indexMappings.at(
      rng.getSingle<unsigned>(
        0,
        mappingsGroup.indexMappings.size() - 1
      )
    );
  }

  return boost::none;
}

bool AtomStereocenter::PermutationState::isFeasibleStereopermutation(
  const StereopermutationType& assignment,
  const RankingInformation::RankedLigandsType& canonicalLigands,
  const ConeAngleType& coneAngles,
  const RankingInformation& ranking,
  const Symmetry::Name symmetry,
  const GraphType& graph
) {
  auto symmetryPositionMap = generateLigandToSymmetryPositionMap(
    assignment,
    canonicalLigands
  );

  // Check if any haptic ligand cones intersect
  const unsigned L = ranking.ligands.size();
  for(unsigned ligandI = 0; ligandI < L - 1; ++ligandI) {
    for(unsigned ligandJ = 0; ligandJ < L; ++ligandJ) {
      // Do not test cone angles if no angle could be calculated
      if(!coneAngles.at(ligandI) || !coneAngles.at(ligandJ)) {
        continue;
      }

      double symmetryAngle = Symmetry::angleFunction(symmetry)(
        symmetryPositionMap.at(ligandI),
        symmetryPositionMap.at(ligandJ)
      );

      /* A haptic steropermutation of ligands is only obviously impossible if
       * the haptic ligands have no spatial freedom to arrange in a fashion
       * that does not overlap.
       */
      if(
        (
          symmetryAngle
          - coneAngles.at(ligandI).value().lower
          - coneAngles.at(ligandJ).value().lower
        ) < 0
      ) {
        return false;
      }
    }
  }

  /* Idea: An assignment is unfeasible if any link's cycle cannot be realized
   * as a flat cyclic polygon, in which the edges from the central atom are
   * merged using the joint angle calculable from the assignment and symmetry.
   *
   * The algorithm below is explained in detail in
   * documents/denticity_feasibility/.
   */
  for(const auto& link : ranking.links) {
    // Ignore cycles of size 3
    if(link.cycleSequence.size() == 4) {
      continue;
    }

    // Perform no checks if, for either of the ligands, no cone angle could be calculated
    if(!coneAngles.at(link.indexPair.first) || !coneAngles.at(link.indexPair.second)) {
      continue;
    }

    const DistanceGeometry::ValueBounds ligandIConeAngle = coneAngles.at(link.indexPair.first).value();
    const DistanceGeometry::ValueBounds ligandJConeAngle = coneAngles.at(link.indexPair.second).value();

    const double symmetryAngle = Symmetry::angleFunction(symmetry)(
      symmetryPositionMap.at(link.indexPair.first),
      symmetryPositionMap.at(link.indexPair.second)
    );

    /* A link across haptic ligands is only obviously impossible if it is
     * impossible in the best case scenario. In this case, especially for alpha,
     * ligand bridge links must be possible only in the best case spatial
     * arrangement for the haptic ligand link to be possible. That means
     * subtracting the upper bound of the respective cone angles.
     */
    const double alpha = std::max(
      0.0,
      symmetryAngle - ligandIConeAngle.upper - ligandJConeAngle.upper
    );

    auto edgeLengths = temple::mapSequentialPairs(
      link.cycleSequence,
      [&](const auto& i, const auto& j) -> double {
        auto findEdgePair = boost::edge(i, j, graph);
        assert(findEdgePair.second);

        return Bond::calculateBondDistance(
          graph[i].elementType,
          graph[j].elementType,
          graph[findEdgePair.first].bondType
        );
      }
    );

    const double a = edgeLengths.front();
    const double b = edgeLengths.back();
    const double c = CommonTrig::lawOfCosines(a, b, alpha);

    edgeLengths.front() = c;
    edgeLengths.erase(edgeLengths.end() - 1);

    // Quick escape: If the cyclic polygon isn't even constructible, fail
    if(!CyclicPolygons::exists(edgeLengths)) {
      return false;
    }

    /* Test that no atom in cyclic polygon except binding sites in binding
     * distance to central atom.
     */

    const auto phis = CyclicPolygons::internalAngles(edgeLengths);

    const double d1 = CommonTrig::lawOfCosines(
      a,
      edgeLengths.at(1), // i
      phis.at(0) + alpha
    );

    if(
      d1 <= Bond::calculateBondDistance(
        graph[link.cycleSequence.at(0)].elementType,
        // 0 is the central index, 1 is the first ligand
        graph[link.cycleSequence.at(2)].elementType,
        BondType::Single
      )
    ) {
      return false;
    }

    std::vector<double> distances {a, d1};
    std::vector<double> deltas {};

    for(unsigned i = 1; i < phis.size() - 2; ++i) {
      deltas.push_back(
        CommonTrig::lawOfCosinesAngle(
          edgeLengths.at(i),
          *(distances.end() - 1),
          *(distances.end() - 2)
        )
      );

      distances.push_back(
        CommonTrig::lawOfCosines(
          distances.back(),
          edgeLengths.at(i + 1),
          phis.at(i) - deltas.back()
        )
      );

      if(
        distances.back() <= Bond::calculateBondDistance(
          graph[link.cycleSequence.at(0)].elementType,
          // 0 is the central index, 1 is the first ligand
          graph[link.cycleSequence.at(i + 2)].elementType,
          BondType::Single
        )
      ) {
        return false;
      }
    }
  }

  return true;
}


/* Static functions */
/* Constructors */
AtomStereocenter::AtomStereocenter(
  const GraphType& graph,
  // The symmetry of this Stereocenter
  const Symmetry::Name& symmetry,
  // The atom this Stereocenter is centered on
  const AtomIndexType centerAtom,
  // Ranking information of substituents
  RankingInformation ranking
) : _ranking {std::move(ranking)},
    _centerAtom {centerAtom},
    _symmetry {symmetry},
    _assignmentOption {boost::none}
{
  _cache = PermutationState {
    _ranking,
    _centerAtom,
    _symmetry,
    graph
  };
}

/* Modification */
void AtomStereocenter::addSubstituent(
  const GraphType& graph,
  const AtomIndexType newSubstituentIndex,
  RankingInformation newRanking,
  const Symmetry::Name& newSymmetry,
  const ChiralStatePreservation& preservationOption
) {
  // Calculate set of new permutations from changed parameters
  PermutationState newPermutationState {
    newRanking,
    _centerAtom,
    newSymmetry,
    graph
  };

  /* Try to find a continuation of chiral state (index of permutation in the new
   * set of permutations)
   */
  boost::optional<unsigned> newStereopermutation = boost::none;

  /* Two possible situations: Either a full ligand is added, or an atom is
   * added to a ligand
   */
  boost::optional<bool> soleConstitutingIndex;
  unsigned ligandIndexAddedTo;
  for(
    unsigned ligandI = 0;
    ligandI < newRanking.ligands.size() && !soleConstitutingIndex;
    ++ligandI
  ) {
    for(const AtomIndexType constitutingIndex : newRanking.ligands.at(ligandI)) {
      if(constitutingIndex == newSubstituentIndex) {
        ligandIndexAddedTo = ligandI;
        soleConstitutingIndex = (newRanking.ligands.at(ligandI).size() == 1);
        break;
      }
    }
  }

  // No need to find a new assignment if no chiral state is present
  if(_assignmentOption && numStereopermutations() > 1) {
    // Transfer indices from smaller symmetry to larger
    std::vector<unsigned> ligandsAtNewSymmetryPositions;

    if(Symmetry::size(newSymmetry) == Symmetry::size(_symmetry)) {
      /* If no symmetry transition happens, then all we have to figure out is a
       * ligand to ligand mapping (since ligands may have reordered completely)
       */
      assert(!soleConstitutingIndex.value());

      // Sort ligands in both rankings so we can use lexicographical comparison
      _ranking.ligands.at(ligandIndexAddedTo).push_back(newSubstituentIndex);
      for(auto& ligand : _ranking.ligands) {
        temple::sort(ligand);
      }

      for(auto& ligand : newRanking.ligands) {
        temple::sort(ligand);
      }

      auto ligandMapping = temple::map(
        _ranking.ligands,
        [&newRanking](const auto& ligand) -> unsigned {
          auto findIter = std::find(
            newRanking.ligands.begin(),
            newRanking.ligands.end(),
            ligand
          );

          assert(findIter != newRanking.ligands.end());

          return findIter - newRanking.ligands.begin();
        }
      );

      ligandsAtNewSymmetryPositions.resize(Symmetry::size(newSymmetry));
      for(unsigned i = 0; i < ligandMapping.size(); ++i) {
        ligandsAtNewSymmetryPositions.at(i) = ligandMapping.at(
          _cache.symmetryPositionMap.at(i)
        );
      }
    } else if(Symmetry::size(newSymmetry) == Symmetry::size(_symmetry) + 1) {
      assert(soleConstitutingIndex.value());

      /* Try to get a mapping to the new symmetry
       * If that returns a Some, try to get a mapping by preservationOption policy
       *
       * If any of these steps returns boost::none, the whole expression is
       * boost::none.
       */
      auto suitableMappingOption = Symmetry::getMapping(
        _symmetry,
        newSymmetry,
        boost::none
      ) | temple::callIfSome(
        PermutationState::getIndexMapping,
        temple::ANS,
        preservationOption
      );

      if(suitableMappingOption) {
        /* So now we must transfer the current assignment into the new symmetry
         * and search for it in the set of uniques.
         */
        const auto& symmetryMapping = suitableMappingOption.value();


        // Copy over the current symmetry position map
        std::vector<unsigned> ligandsAtOldSymmetryPositions = _cache.symmetryPositionMap;
        ligandsAtOldSymmetryPositions.push_back(ligandIndexAddedTo);
        ligandsAtNewSymmetryPositions.resize(Symmetry::size(newSymmetry));

        for(unsigned i = 0; i < Symmetry::size(newSymmetry); ++i) {
          ligandsAtNewSymmetryPositions.at(
            symmetryMapping.at(i)
          ) = ligandsAtOldSymmetryPositions.at(i);
        }
      }
      /* If no mapping can be found that fits to the preservationOption,
       * newStereopermutation remains boost::none, and this stereocenter loses
       * any chiral information it may have had.
       */
    }

    if(!ligandsAtNewSymmetryPositions.empty()) {
      // Get character representation in new symmetry
      std::vector<char> charactersInNewSymmetry = PermutationState::makeStereopermutationCharacters(
        newPermutationState.canonicalLigands,
        newPermutationState.symbolicCharacters,
        ligandsAtNewSymmetryPositions
      );

      // Construct an assignment from it
      auto trialStereopermutation = stereopermutation::Stereopermutation(
        newSymmetry,
        charactersInNewSymmetry,
        newPermutationState.selfReferentialLinks
      );

      // Generate the rotational equivalents
      auto allTrialRotations = trialStereopermutation.generateAllRotations(newSymmetry);

      // Search for a match from the vector of uniques
      for(unsigned i = 0; i < newPermutationState.permutations.assignments.size(); ++i) {
        if(allTrialRotations.count(newPermutationState.permutations.assignments.at(i)) > 0) {
          newStereopermutation = i;
          break;
        }
      }
    }
  }

  // Overwrite class state
  _ranking = std::move(newRanking);
  _symmetry = newSymmetry;
  _cache = std::move(newPermutationState);

  assign(newStereopermutation);
}

void AtomStereocenter::assign(boost::optional<unsigned> assignment) {
  if(assignment) {
    assert(assignment.value() < _cache.feasiblePermutations.size());
  }

  // Store current assignment
  _assignmentOption = std::move(assignment);

  /* save a mapping of next neighbor indices to symmetry positions after
   * assigning (AtomIndexType -> unsigned).
   */
  if(_assignmentOption) {
    _cache.symmetryPositionMap = PermutationState::generateLigandToSymmetryPositionMap(
      _cache.permutations.assignments.at(
        _cache.feasiblePermutations.at(
          _assignmentOption.value()
        )
      ),
      _cache.canonicalLigands
    );
  } else { // Wipe the map
    _cache.symmetryPositionMap.clear();
  }
}

void AtomStereocenter::assignRandom() {
  assign(
    rng.pickDiscrete(
      // Map the feasible permutations onto their weights
      temple::map(
        _cache.feasiblePermutations,
        [&](const unsigned permutationIndex) -> unsigned {
          return _cache.permutations.weights.at(permutationIndex);
        }
      )
    )
  );
}

void AtomStereocenter::propagateGraphChange(
  const GraphType& graph,
  RankingInformation newRanking
) {
  if(
    newRanking.ligandsRanking == _ranking.ligandsRanking
    && newRanking.links == _ranking.links
  ) {
    return;
  }

  PermutationState newPermutationState {
    newRanking,
    _centerAtom,
    _symmetry,
    graph
  };

  boost::optional<unsigned> newStereopermutation = boost::none;

  /* Before we overwrite class state, we need to figure out which assignment
   * in the new set of assignments corresponds to the one we have now.
   * This is only necessary in the case that the stereocenter is currently
   * assigned and only possible if the new number of assignments is smaller or
   * equal to the amount we have currently.
   *
   * Additionally, in some circumstances, propagateGraphChange can be called
   * with either fewer or more ligands than the current ranking indicates. This
   * happens if e.g. a bond is added between ligands, forming a single haptic
   * ligand, or breaking a haptic ligand into two. These cases are excluded
   * with the condition of an equal number of ligands, and thus universally
   * lead to a loss of stereoinformation.
   */
  if(
    _assignmentOption
    && numStereopermutations() > 1
    && (
      newPermutationState.permutations.assignments.size()
      <= _cache.permutations.assignments.size()
    ) && newRanking.ligands.size() == _ranking.ligands.size()
  ) {
    const auto& currentStereopermutation = _cache.permutations.assignments.at(
      _cache.feasiblePermutations.at(
        _assignmentOption.value()
      )
    );

    // Replace the characters by their corresponding indices from the old ranking
    std::vector<unsigned> ligandsAtSymmetryPositions = PermutationState::generateSymmetryPositionToLigandMap(
      currentStereopermutation,
      _cache.canonicalLigands
    );

    // Replace the atom indices by their new ranking characters
    std::vector<char> newStereopermutationCharacters = PermutationState::makeStereopermutationCharacters(
      newPermutationState.canonicalLigands,
      newPermutationState.symbolicCharacters,
      ligandsAtSymmetryPositions
    );

    // Create a new assignment with those characters
    auto trialStereopermutation = stereopermutation::Stereopermutation(
      _symmetry,
      newStereopermutationCharacters,
      newPermutationState.selfReferentialLinks
    );

    // Generate all rotations of this trial assignment
    auto allTrialRotations = trialStereopermutation.generateAllRotations(_symmetry);

    // Find out which of the new assignments has a rotational equivalent
    for(unsigned i = 0; i < newPermutationState.permutations.assignments.size(); ++i) {
      if(allTrialRotations.count(newPermutationState.permutations.assignments.at(i)) > 0) {
        newStereopermutation = i;
        break;
      }
    }
  }

  // Overwrite the class state
  _ranking = std::move(newRanking);
  _cache = std::move(newPermutationState);
  assign(newStereopermutation);
}

void AtomStereocenter::propagateVertexRemoval(const AtomIndexType removedIndex) {
  /* This function replaces any occurrences of the atom index that is being
   * removed in the global state with a placeholder of the same type and updates
   * any invalidated atom indices.
   */

  /* If the central atom is being removed, just drop this stereocenter
   * beforehand in caller. This would just be unnecessary work.
   */
  assert(_centerAtom != removedIndex);

  // Define some helper functions
  auto updateIndexInplace = [&removedIndex](AtomIndexType& index) -> void {
    if(index > removedIndex) {
      --index;
    } else if(index == removedIndex) {
      index = Stereocenter::removalPlaceholder;
    }
  };

  auto updateIndex = [&removedIndex](const AtomIndexType index) -> AtomIndexType {
    if(index > removedIndex) {
      return index - 1;
    }

    if(index == removedIndex) {
      return Stereocenter::removalPlaceholder;
    }

    return index;
  };

  /* Update indices in RankingInformation */
  for(auto& equalPrioritySet : _ranking.sortedSubstituents) {
    for(auto& index : equalPrioritySet) {
      updateIndexInplace(index);
    }
  }

  for(auto& ligandIndicesList : _ranking.ligands) {
    for(auto& atomIndex : ligandIndicesList) {
      updateIndexInplace(atomIndex);
    }
  }

  for(auto& link : _ranking.links) {
    link.cycleSequence = temple::map(
      link.cycleSequence,
      updateIndex
    );
  }
}

void AtomStereocenter::removeSubstituent(
  const GraphType& graph,
  const AtomIndexType which,
  RankingInformation newRanking,
  const Symmetry::Name& newSymmetry,
  const ChiralStatePreservation& preservationOption
) {
  /* This function tries to find a new assignment for the situation in which
   * the previously replaced atom index is actually removed.
   *
   * Since the introduction of haptic ligands, the prior graph change can
   * encompass two things:
   * - A ligand that is comprised of a singular atom has been removed. The
   *   symmetry size is reduced and a state continuation must be found.
   * - A constituting atom of a haptic ligand has been removed. No symmetry
   *   change happens.
   */
  PermutationState newPermutationState {
    newRanking,
    _centerAtom,
    newSymmetry,
    graph
  };

  boost::optional<unsigned> newStereopermutation;

  /* Find out in which ligand the atom is removed, and whether it is the sole
   * constituting index
   */
  bool soleConstitutingIndex [[gnu::unused]];
  unsigned ligandIndexRemovedFrom;

  { // Temporary local scope to avoid pollution
    bool found = false;
    for(
      unsigned ligandI = 0;
      ligandI < _ranking.ligands.size() && !found;
      ++ligandI
    ) {
      for(const AtomIndexType constitutingIndex : _ranking.ligands.at(ligandI)) {
        if(constitutingIndex == which) {
          found = true;
          soleConstitutingIndex = (_ranking.ligands.at(ligandI).size() == 1);
          ligandIndexRemovedFrom = ligandI;
        }
      }
    }

    if(!found) {
      throw std::logic_error("Ligand index being removed from not found!");
    }
  }

  // No need to find a new assignment if we currently do not carry chiral state
  if(_assignmentOption && numStereopermutations() > 1) {
    std::vector<unsigned> ligandsAtNewSymmetryPositions;

    if(Symmetry::size(newSymmetry) == Symmetry::size(_symmetry)) {
      /* If no symmetry transition happens, then all we have to figure out is a
       * ligand to ligand mapping.
       */
      assert(!soleConstitutingIndex);

      /* Sort ligands in the old ranking and new so we can use lexicographical
       * comparison to figure out a mapping
       */
      for(auto& ligand : _ranking.ligands) {
        temple::inplaceRemove(ligand, which);
        temple::sort(ligand);
      }

      for(auto& ligand : newRanking.ligands) {
        temple::sort(ligand);
      }

      // Calculate the mapping from old ligands to new ones
      auto ligandMapping = temple::map(
        _ranking.ligands,
        [&newRanking](const auto& ligand) -> unsigned {
          auto findIter = std::find(
            newRanking.ligands.begin(),
            newRanking.ligands.end(),
            ligand
          );

          assert(findIter != newRanking.ligands.end());

          return findIter - newRanking.ligands.begin();
        }
      );

      ligandsAtNewSymmetryPositions.resize(Symmetry::size(newSymmetry));
      // Transfer ligands to new mapping
      for(unsigned i = 0; i < ligandMapping.size(); ++i) {
        ligandsAtNewSymmetryPositions.at(i) = ligandMapping.at(
          _cache.symmetryPositionMap.at(i)
        );
      }
    } else if(Symmetry::size(newSymmetry) == Symmetry::size(_symmetry) - 1) {
      assert(soleConstitutingIndex);
      /* Try to get a symmetry mapping to the new symmetry position
       * If there are mappings, try to select one according to preservationOption policy
       *
       * If any of those steps returns boost::none, the whole expression is
       * boost::none.
       */
      auto suitableMappingOptional = Symmetry::getMapping(
        _symmetry,
        newSymmetry,
        /* Last parameter is the deleted symmetry position, which is the
         * symmetry position at which the ligand being removed is currently at
         */
        _cache.symmetryPositionMap.at(ligandIndexRemovedFrom)
      ) | temple::callIfSome(
        PermutationState::getIndexMapping,
        temple::ANS,
        preservationOption
      );

      if(suitableMappingOptional) {
        const auto& symmetryMapping = suitableMappingOptional.value();

        // Transfer indices from current symmetry to new symmetry
        ligandsAtNewSymmetryPositions.resize(Symmetry::size(newSymmetry));
        for(unsigned i = 0; i < Symmetry::size(newSymmetry); ++i) {
          ligandsAtNewSymmetryPositions.at(i) = _cache.symmetryPositionMap.at(
            symmetryMapping.at(i)
          );
        }

        /* Now we have the old ligand indices in the new symmetry positions.
         * Since we know which ligand is deleted, we can decrement any indices
         * larger than it and obtain the new ligand indices.
         */
        for(auto& ligandIndex : ligandsAtNewSymmetryPositions) {
          if(ligandIndex > ligandIndexRemovedFrom) {
            --ligandIndex;
          }
        }
      }

      if(!ligandsAtNewSymmetryPositions.empty()) {
        // Get character representation in new symmetry
        std::vector<char> charactersInNewSymmetry = PermutationState::makeStereopermutationCharacters(
          newPermutationState.canonicalLigands,
          newPermutationState.symbolicCharacters,
          ligandsAtNewSymmetryPositions
        );

        // TODO Shouldn't the links in the new symmetry be generated too for use in comparison??

        // Construct an assignment
        auto trialStereopermutation = stereopermutation::Stereopermutation(
          newSymmetry,
          charactersInNewSymmetry,
          newPermutationState.selfReferentialLinks
        );

        // Generate the rotational equivalents
        auto allTrialRotations = trialStereopermutation.generateAllRotations(newSymmetry);

        // Search for a match from the vector of uniques
        for(unsigned i = 0; i < newPermutationState.permutations.assignments.size(); ++i) {
          if(allTrialRotations.count(newPermutationState.permutations.assignments.at(i)) > 0) {
            newStereopermutation = i;
            break;
          }
        }
      }
    }
  }

  // Overwrite class state
  _ranking = std::move(newRanking);
  _symmetry = newSymmetry;
  _cache = std::move(newPermutationState);
  assign(newStereopermutation);
}

const RankingInformation& AtomStereocenter::getRanking() const {
  return _ranking;
}

Symmetry::Name AtomStereocenter::getSymmetry() const {
  return _symmetry;
}

void AtomStereocenter::fit(
  const GraphType& graph,
  const AngstromWrapper& angstromWrapper,
  const std::vector<Symmetry::Name>& excludeSymmetries
) {
  // For all atoms making up a ligand, decide on the spatial average position
  const std::vector<Eigen::Vector3d> ligandPositions = temple::mapToVector(
    _ranking.ligands,
    [&angstromWrapper](const std::vector<AtomIndexType>& ligandAtoms) -> Eigen::Vector3d {
      return DelibHelpers::averagePosition(angstromWrapper.positions, ligandAtoms);
    }
  );

  // Save stereocenter state to return to if no fit is viable
  const Symmetry::Name priorSymmetry = _symmetry;
  const boost::optional<unsigned> priorStereopermutation  = _assignmentOption;

  const Symmetry::Name initialSymmetry {Symmetry::Name::Linear};
  const unsigned initialStereopermutation = 0;
  const double initialPenalty = 100;

  Symmetry::Name bestSymmetry = initialSymmetry;
  unsigned bestStereopermutation = initialStereopermutation;
  double bestPenalty = initialPenalty;
  unsigned bestStereopermutationMultiplicity = 1;

  auto excludesContains = temple::makeContainsPredicate(excludeSymmetries);

  // Cycle through all symmetries
  for(const auto& symmetryName : Symmetry::allNames) {
    // Skip any Symmetries of different size
    if(
      Symmetry::size(symmetryName) != Symmetry::size(_symmetry)
      || excludesContains(symmetryName)
    ) {
      continue;
    }

    // Change the symmetry of the AtomStereocenter
    setSymmetry(symmetryName, graph);

    for(
      unsigned assignment = 0;
      assignment < numAssignments();
      ++assignment
    ) {
      // Assign the stereocenter
      assign(assignment);

      const double angleDeviations = temple::sum(
        temple::mapAllPairs(
          temple::iota<unsigned>(Symmetry::size(_symmetry)),
          [&](const unsigned ligandI, const unsigned ligandJ) -> double {
            return std::fabs(
              DelibHelpers::angle(
                ligandPositions.at(ligandI),
                angstromWrapper.positions.at(_centerAtom).toEigenVector(),
                ligandPositions.at(ligandJ)
              ) - angle(ligandI, ligandJ)
            );
          }
        )
      );

      // We can stop immediately if this is worse
      if(angleDeviations > bestPenalty) {
        continue;
      }

      /* TODO should this be kept at all? Just a follow-up error from the angle
       * What value does it bring?
       */
      const double oneThreeDistanceDeviations = temple::sum(
        temple::mapAllPairs(
          temple::iota<unsigned>(Symmetry::size(_symmetry)),
          [&](const unsigned ligandI, const unsigned ligandJ) -> double {
            return std::fabs(
              // ligandI - ligandJ 1-3 distance from positions
              DelibHelpers::distance(
                ligandPositions.at(ligandI),
                ligandPositions.at(ligandJ)
              )
              // idealized 1-3 distance from
              - CommonTrig::lawOfCosines(
                // i-j 1-2 distance from positions
                DelibHelpers::distance(
                  ligandPositions.at(ligandI),
                  angstromWrapper.positions.at(_centerAtom).toEigenVector()
                ),
                // j-k 1-2 distance from positions
                DelibHelpers::distance(
                  angstromWrapper.positions.at(_centerAtom).toEigenVector(),
                  ligandPositions.at(ligandJ)
                ),
                // idealized Stereocenter angle
                angle(ligandI, ligandJ)
              )
            );
          }
        )
      );

      // Another early continue
      if(angleDeviations + oneThreeDistanceDeviations > bestPenalty) {
        continue;
      }

      const double chiralityDeviations = temple::sum(
        temple::map(
          minimalChiralityConstraints(),
          [&](const auto& minimalPrototype) -> double {
            double volume = temple::unpackArrayToFunction(
              temple::map(
                minimalPrototype,
                [&](const boost::optional<unsigned>& ligandIndexOptional) -> Eigen::Vector3d {
                  if(ligandIndexOptional) {
                    return ligandPositions.at(ligandIndexOptional.value());
                  }

                  return angstromWrapper.positions.at(_centerAtom).asEigenVector();
                }
              ),
              DelibHelpers::adjustedSignedVolume
            );

            // minimalChiralityConstraints() supplies only Positive targets
            if(volume < 0) {
              return 1;
            }

            return 0;
          }
        )
      );

      double fitPenalty = angleDeviations
        + oneThreeDistanceDeviations
        + chiralityDeviations;


#ifndef NDEBUG
      Log::log(Log::Particulars::AtomStereocenterFit)
        << Symmetry::nameIndex(symmetryName)
        << ", " << assignment
        << ", " << std::setprecision(4) << std::fixed
        << angleDeviations << ", "
        << oneThreeDistanceDeviations << ", "
        << chiralityDeviations
        << std::endl;
#endif

      if(fitPenalty < bestPenalty) {
        bestSymmetry = symmetryName;
        bestStereopermutation = assignment;
        bestPenalty = fitPenalty;
        bestStereopermutationMultiplicity = 1;
      } else if(fitPenalty == bestPenalty) {
        // Assume that IF we have multiplicity, it's from the same symmetry
        assert(bestSymmetry == symmetryName);
        bestStereopermutationMultiplicity += 1;
      }
    }
  }

  /* In case NO assignments could be tested, return to the prior state.
   * This guards against situations in which predicates in
   * uniques could lead no assignments to be returned, such as
   * in e.g. square-planar AAAB with {0, 3}, {1, 3}, {2, 3} with removal of
   * trans-spanning groups. In that situation, all possible assignments are
   * trans-spanning and uniques is an empty vector.
   *
   * At the moment, this predicate is disabled, so no such issues should arise.
   * Just being safe.
   */
  if(
    bestSymmetry == initialSymmetry
    && bestStereopermutation == initialStereopermutation
    && bestPenalty == initialPenalty
  ) {
    // Return to prior
    setSymmetry(priorSymmetry, graph);
    assign(priorStereopermutation);
  } else {
    // Set to best fit
    setSymmetry(bestSymmetry, graph);

    /* How to handle multiplicity?
     * Current policy: If there is multiplicity, do not assign
     */
    if(bestStereopermutationMultiplicity > 1) {
      assign(boost::none);
    } else {
      assign(bestStereopermutation);
    }
  }
}

/* Information */
double AtomStereocenter::angle(
  const unsigned i,
  const unsigned j
) const {
  assert(i != j);
  assert(!_cache.symmetryPositionMap.empty());

  return Symmetry::angleFunction(_symmetry)(
    _cache.symmetryPositionMap.at(i),
    _cache.symmetryPositionMap.at(j)
  );
}

void AtomStereocenter::setModelInformation(
  DistanceGeometry::SpatialModel& model,
  const std::function<double(const AtomIndexType)> cycleMultiplierForIndex,
  const double looseningMultiplier
) const {
  /* Intra-site modelling */
  for(unsigned ligandI = 0; ligandI < _cache.ligandDistances.size(); ++ligandI) {
    /* If no cone information is present, do not correct the distance to the
     * ligand using the cone angle
     */
    if(!_cache.coneAngles.at(ligandI)) {
      for(const AtomIndexType i : _ranking.ligands.at(ligandI)) {
        model.setBondBoundsIfEmpty(
          {{i, _centerAtom}},
          _cache.ligandDistances.at(ligandI)
        );
      }
    }

    /* Distance of every ligand site atom index to the central atom assumptions
     * - Every haptic index is on the cone base circle
     * - Cone height is defined by _cache.ligandDistance
     * - Cone angle is defined by _cache.coneAngle
     */
    DistanceGeometry::ValueBounds coneAngleBounds = _cache.coneAngles.at(ligandI).value();

    double upperHypotenuse = (
      _cache.ligandDistances.at(ligandI).upper
      / std::cos(coneAngleBounds.lower)
    );

    double lowerHypotenuse = (
      _cache.ligandDistances.at(ligandI).lower
      / std::cos(coneAngleBounds.upper)
    );

    for(const AtomIndexType i : _ranking.ligands.at(ligandI)) {
      model.setBondBoundsIfEmpty(
        {{i, _centerAtom}},
        DistanceGeometry::ValueBounds {
          lowerHypotenuse,
          upperHypotenuse
        }
      );
    }

    /* Angles between ligand-constituting atoms within a single index
     * - Minimally 0° (if there were a zero-length bond)
     *   You could compute shortest possible bond constexpr and insert a trig
     *   calc here, but the bond level distance is supplied elsewhere by
     *   SpatialModel anyway, no need to duplicate that information
     * - Maximally 2 * the upper cone angle (but not more than M_PI)
     */
    temple::forAllPairs(
      _ranking.ligands.at(ligandI),
      [&](const AtomIndexType i, const AtomIndexType j) {
        model.setAngleBoundsIfEmpty(
          {{i, _centerAtom, j}},
          DistanceGeometry::ValueBounds {
            0,
            std::min(M_PI, 2 * coneAngleBounds.upper)
          }
        );
      }
    );
  }

  /* Inter-site modelling */
  for(unsigned i = 0; i < _ranking.ligands.size() - 1; ++i) {
    if(!_cache.coneAngles.at(i)) {
      continue;
    }

    for(unsigned j = i + 1; j < _ranking.ligands.size(); ++j) {
      if(!_cache.coneAngles.at(j)) {
        continue;
      }

      DistanceGeometry::ValueBounds angleBounds {
        (
          angle(i, j)
          - _cache.coneAngles.at(i).value().upper
          - _cache.coneAngles.at(j).value().upper
        ),
        (
          angle(i, j)
          + _cache.coneAngles.at(i).value().upper
          + _cache.coneAngles.at(j).value().upper
        )
      };

      temple::forAllPairs(
        _ranking.ligands.at(i),
        _ranking.ligands.at(j),
        [&](const AtomIndexType x, const AtomIndexType y) -> void {
          double variation = (
            DistanceGeometry::SpatialModel::angleAbsoluteVariance
            * looseningMultiplier
            * cycleMultiplierForIndex(x)
            * cycleMultiplierForIndex(y)
          );

          model.setAngleBoundsIfEmpty(
            {{x, _centerAtom, y}},
            DistanceGeometry::ValueBounds {
              std::max(0.0, angleBounds.lower - variation),
              std::min(M_PI, angleBounds.upper + variation)
            }
          );
        }
      );
    }
  }
}

boost::optional<unsigned> AtomStereocenter::assigned() const {
  return _assignmentOption;
}

boost::optional<unsigned> AtomStereocenter::indexOfPermutation() const {
  if(_assignmentOption) {
    return _cache.feasiblePermutations.at(_assignmentOption.value());
  }

  return boost::none;
}

std::vector<
  std::array<boost::optional<unsigned>, 4>
> AtomStereocenter::minimalChiralityConstraints() const {
  std::vector<
    std::array<boost::optional<unsigned>, 4>
  > precursors;

  // Only collect constraints if it's actually assigned
  if(_assignmentOption) {

    /* Invert _neighborSymmetryPositionMap, we need a mapping of
     *  (position in symmetry) -> atom index
     */
    auto symmetryPositionToLigandIndexMap = PermutationState::generateSymmetryPositionToLigandMap(
      _cache.permutations.assignments.at(
        _cache.feasiblePermutations.at(
          _assignmentOption.value()
        )
      ),
      _cache.canonicalLigands
    );

    // Get list of tetrahedra from symmetry
    auto tetrahedraList = Symmetry::tetrahedra(_symmetry);

    precursors.reserve(tetrahedraList.size());
    for(const auto& tetrahedron : tetrahedraList) {
      /* Replace boost::none with centerAtom, indices (represent positions within
       * the symmetry) with the atom index at that position from the inverted map
       */

      // Make a minimal sequence from it
      precursors.push_back(
        temple::map(
          tetrahedron,
          [&](const boost::optional<unsigned>& indexOptional) -> boost::optional<unsigned> {
            if(indexOptional) {
              return symmetryPositionToLigandIndexMap.at(
                indexOptional.value()
              );
            }

            return boost::none;
          }
        )
      );
    }
  }

  return precursors;
}

std::vector<DistanceGeometry::ChiralityConstraint> AtomStereocenter::chiralityConstraints() const {
  return temple::map(
    minimalChiralityConstraints(),
    [&](const auto& minimalConstraint) -> DistanceGeometry::ChiralityConstraint {
      /* We need to calculate target upper and lower volumes for the chirality
       * constraints. _cache.ligandDistances contains bounds for the distance to
       * each ligand site plane, and since the center of each cone should
       * constitute the average ligand position, we can calculate 1-3 distances
       * between the centerpoints of ligands using the idealized angles.
       *
       * The target volume of the chirality constraint created by the
       * tetrahedron is calculated using internal coordinates (the
       * Cayley-Menger determinant), always leading to V > 0, so depending on
       * the current assignment, the sign of the result is switched. The
       * formula used later in chirality constraint calculation for explicit
       * coordinates is adjusted by V' = 6 V to avoid an unnecessary factor, so
       * we do that here too:
       *
       *    288 V²  = |...|               | substitute V' = 6 V
       * -> 8 (V')² = |...|
       * ->      V' = sqrt(|...| / 8)
       *
       * where the Cayley-Menger determinant |...| is square symmetric:
       *
       *          |   0    1    1    1    1  |
       *          |        0  d12² d13² d14² |
       *  |...| = |             0  d23² d24² |
       *          |                  0  d34² |
       *          |  ...                  0  |
       *
       */

      Eigen::Matrix<double, 5, 5> lowerMatrix, upperMatrix;

      lowerMatrix.row(0).setOnes();
      upperMatrix.row(0).setOnes();

      lowerMatrix.diagonal().setZero();
      upperMatrix.diagonal().setZero();

      for(unsigned i = 0; i < 4; ++i) {
        boost::optional<DistanceGeometry::ValueBounds> iBounds;
        if(minimalConstraint.at(i)) {
          iBounds = _cache.ligandDistances.at(
            minimalConstraint.at(i).value()
          );
        }

        for(unsigned j = i + 1; j < 4; ++j) {
          boost::optional<DistanceGeometry::ValueBounds> jBounds;
          if(minimalConstraint.at(j)) {
            jBounds = _cache.ligandDistances.at(
              minimalConstraint.at(j).value()
            );
          }

          assert(iBounds || jBounds);

          DistanceGeometry::ValueBounds oneThreeDistanceBounds;
          if(iBounds && jBounds) {
            oneThreeDistanceBounds = {
              CommonTrig::lawOfCosines(
                iBounds.value().lower,
                jBounds.value().lower,
                angle(
                  minimalConstraint.at(i).value(),
                  minimalConstraint.at(j).value()
                )
              ),
              CommonTrig::lawOfCosines(
                iBounds.value().upper,
                jBounds.value().upper,
                angle(
                  minimalConstraint.at(i).value(),
                  minimalConstraint.at(j).value()
                )
              )
            };
          } else if(iBounds) {
            oneThreeDistanceBounds = iBounds.value();
          } else {
            oneThreeDistanceBounds = jBounds.value();
          }

          lowerMatrix(i + 1, j + 1) = std::pow(oneThreeDistanceBounds.lower, 2);
          upperMatrix(i + 1, j + 1) = std::pow(oneThreeDistanceBounds.upper, 2);
        }
      }

      const double boundFromLower = static_cast<
        Eigen::Matrix<double, 5, 5>
      >(
        lowerMatrix.selfadjointView<Eigen::Upper>()
      ).determinant();

      const double boundFromUpper = static_cast<
        Eigen::Matrix<double, 5, 5>
      >(
        upperMatrix.selfadjointView<Eigen::Upper>()
      ).determinant();

      assert(boundFromLower > 0 && boundFromUpper > 0);

      const double volumeFromLower = std::sqrt(boundFromLower / 8);
      const double volumeFromUpper = std::sqrt(boundFromUpper / 8);

      // Map the ligand indices to their constituent indices for use in the prototype
      auto tetrahedronLigands = temple::map(
        minimalConstraint,
        [&](const boost::optional<unsigned>& ligandIndexOptional) -> std::vector<AtomIndexType> {
          if(ligandIndexOptional) {
            return _ranking.ligands.at(ligandIndexOptional.value());
          }

          return {_centerAtom};
        }
      );

      /* Although it is tempting to assume that the Cayley-Menger determinant
       * using the lower bounds is smaller than the one using upper bounds,
       * this is NOT true. We cannot a priori know which of both yields the
       * lower or upper bounds on the 3D volume, and hence must ensure only
       * that the ordering is preserved in the generation of the
       * ChiralityConstraint, which checks that the lower bound on the volume
       * is certainly smaller than the upper one.
       *
       * You can check this assertion with a CAS. The relationship between both
       * determinants (where u_ij = l_ij + Δ) is wholly indeterminant, i.e. no
       * logical operator (<, >, <=, >=, ==) between both is true. It
       * completely depends on the individual values. Maybe in very specific
       * cases one can deduce some relationship, but not generally.
       *
       * Also, since chemical_symmetry only emits positive chiral target volume
       * index sequences (see test case name allTetrahedraPositive), no
       * inversion has to be considered.
       */

      return {
        std::move(tetrahedronLigands),
        std::min(volumeFromLower, volumeFromUpper),
        std::max(volumeFromLower, volumeFromUpper)
      };
    }
  );
}

std::string AtomStereocenter::info() const {
  std::string returnString = "CN "s
    + std::to_string(_centerAtom) + " ("s + Symmetry::name(_symmetry) +", "s;

  const auto& characters = _cache.symbolicCharacters;
  std::copy(
    characters.begin(),
    characters.end(),
    std::back_inserter(returnString)
  );

  for(const auto& link : _cache.selfReferentialLinks) {
    returnString += ", "s + characters.at(link.first) + "-"s + characters.at(link.second);
  }

  returnString += "): "s;

  if(_assignmentOption) {
    returnString += std::to_string(_assignmentOption.value());
  } else {
    returnString += "u";
  }

  const unsigned A = numAssignments();
  returnString += "/"s + std::to_string(A);

  const unsigned P = numStereopermutations();
  if(P != A) {
    returnString += " ("s + std::to_string(P) + ")"s;
  }

  return returnString;
}

std::string AtomStereocenter::rankInfo() const {
  /* rankInfo is specifically geared towards RankingTree's consumption,
   * and MUST use indices of permutation
   */
  return (
    "CN-"s + std::to_string(static_cast<unsigned>(_symmetry))
    + "-"s + std::to_string(numStereopermutations())
    + "-"s + (
      indexOfPermutation()
      ? std::to_string(indexOfPermutation().value())
      : "u"s
    )
  );
}

std::vector<AtomIndexType> AtomStereocenter::involvedAtoms() const {
  return {_centerAtom};
}

unsigned AtomStereocenter::numAssignments() const {
  return _cache.feasiblePermutations.size();
}

unsigned AtomStereocenter::numStereopermutations() const {
  return _cache.permutations.assignments.size();
}

void AtomStereocenter::setSymmetry(
  const Symmetry::Name symmetryName,
  const GraphType& graph
) {
  _symmetry = symmetryName;

  // TODO Chiral information in same symmetry size change can also be preserved
  // But careful, this could effect fit() negatively

  _cache = PermutationState {
    _ranking,
    _centerAtom,
    _symmetry,
    graph
  };

  // Dis-assign the stereocenter
  assign(boost::none);
}

Type AtomStereocenter::type() const {
  return Type::AtomStereocenter;
}

bool AtomStereocenter::operator == (const AtomStereocenter& other) const {
  return (
    _symmetry == other._symmetry
    && _centerAtom == other._centerAtom
    && numStereopermutations() == other.numStereopermutations()
    && _assignmentOption == other._assignmentOption
  );
}

bool AtomStereocenter::operator < (const AtomStereocenter& other) const {
  /* Sequentially compare individual components, comparing assignments last
   * if everything else matches
   */
  return temple::consecutiveCompareSmaller(
    _centerAtom,
    other._centerAtom,
    _symmetry,
    other._symmetry,
    numAssignments(),
    other.numAssignments(),
    _assignmentOption,
    other._assignmentOption
  );
}

} // namespace Stereocenters

} // namespace molassembler