/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 * @brief Handle arrangements of substituents at corners of an atom-centered
 *   shape
 *
 * Coordinative stereopermutator class header file. Permits the storage of
 * particular arrangements of bonded atoms around a central atom and their
 * manipulation.
 *
 * Handles the stereopermutation issue, allowing users to cycle through
 * non-mutually-superimposable arrangements of substituents, here called
 * 'assignments'.
 */

#ifndef INCLUDE_MOLASSEMBLER_ATOM_STEREOPERMUTATOR_H
#define INCLUDE_MOLASSEMBLER_ATOM_STEREOPERMUTATOR_H

#include "molassembler/Options.h"
#include "molassembler/OuterGraph.h"

#if __cpp_lib_experimental_propagate_const >= 201505
#define MOLASSEMBLER_ENABLE_PROPAGATE_CONST
#include <experimental/propagate_const>
#endif

using namespace std::string_literals;

namespace Scine {

namespace molassembler {
/* Forward declarations */
struct RankingInformation;
struct AbstractStereopermutations;
struct FeasibleStereopermutations;

namespace DistanceGeometry {

class SpatialModel;
struct ChiralConstraint;

} // namespace DistanceGeometry

/**
 * @brief Handles the steric permutation of substituents of a non-terminal
 *   central atom
 *
 * This class handles the permutation of ranked ligands around a central atom.
 * It models its haptic ligands' binding sites and bridges in multidentate
 * ligands in order to decide which stereopermutations are feasible. A
 * stereopermutation may be infeasible, i.e. not realizable in
 * three-dimensional space, if either haptic ligands would intersect due to
 * too close ligand angles for their spatial heft, or if a multidentate
 * ligand's bridge length between binding sites were too short to match the
 * angle. The list of stereopermutations reduced by infeasible
 * stereopermutations is then re-indexed and those indices into the list are
 * called assignments.
 *
 * A Stereopermutator can be unassigned, i.e. the distinct stereopermutation
 * that the substituents are can be indeterminate. If you choose to generate
 * conformers for a molecule that includes unassigned stereopermutators, every
 * conformer will choose an assignment from the pool of feasible assignments
 * randomly, but consistent with relative statistical occurrence weights.
 *
 * E.g. a square planar AABC ligand set will have an A-A cis stereopermutation
 * that occurs twice as often as the A-A trans stereopermutation.
 *
 * @note An instance of this class on a given central atom does not indicate
 *   that that atom is a stereocenter. That is only the case if there are
 *   multiple stereopermutations of the ranked substituents / ligands.
 */
class AtomStereopermutator {
public:
  //! Old state dumped upon propagation
  using PropagatedState = std::tuple<
    RankingInformation,
    AbstractStereopermutations,
    FeasibleStereopermutations,
    boost::optional<unsigned>
  >;

  /*!
   * @brief Site index sequence defining a chiral constraint. If a site index
   *   is None, then it denotes the position of the central index
   */
  using MinimalChiralConstraint = std::array<boost::optional<unsigned>, 4>;

//!@name Special member functions
//!@{
  AtomStereopermutator(AtomStereopermutator&& other) noexcept;
  AtomStereopermutator& operator = (AtomStereopermutator&& other) noexcept;
  AtomStereopermutator(const AtomStereopermutator& other);
  AtomStereopermutator& operator = (const AtomStereopermutator& other);
  ~AtomStereopermutator();

  /*! @brief Construct an AtomStereopermutator
   *
   * @param graph The molecule's graph. This information is needed to model
   *   haptic ligands.
   * @param shape The local idealized shape to model. Typically the
   *   result of Molecule's inferShape.
   * @param centerAtom The atom index within the molecule that is the center of
   *   the local idealized shape
   * @param ranking The ranking of the central atom's substituents and ligand
   *   sites. Typically the result of Molecule's rankPriority.
   *
   * @complexity{@math{L\cdot S!} where @math{L} is the number of links and
   * @math{S} is the size of @p shape}
   */
  AtomStereopermutator(
    const OuterGraph& graph,
    Shapes::Shape shape,
    AtomIndex centerAtom,
    RankingInformation ranking
  );
//!@}

//!@name Static functions
//!@{
  /*! @brief Picks a shape retaining as much chiral state as possible on a
   *   shape size increase
   *
   * @complexity{@math{O(S!)} if uncached, @math{\Theta(1)} otherwise}
   * @throws std::logic_error If there are no larger shapes
   */
  static Shapes::Shape up(Shapes::Shape shape);

  /*!
   * @brief Picks a shape retaining as much chiral state as possible on a
   *   shape size decrease
   *
   * @complexity{@math{O(S!)} if uncached, @math{\Theta(1)} otherwise}
   * @throws std::logic_error If there are no smaller shapes
   */
  static Shapes::Shape down(Shapes::Shape shape, unsigned removedShapePosition);
//!@}

//!@name Modifiers
  /*! @brief Changes the assignment of the stereopermutator
   *
   * @param assignment The new assignment of the stereopermutator. May be
   *   @p boost::none, which sets the chiral state as indeterminate. Must be
   *   less than the number of assignments if not None.
   *
   * @complexity{@math{\Theta(1)} if @p assignment is @c boost::none.
   * @math{\Theta(S)} otherwise}
   */
  void assign(boost::optional<unsigned> assignment);

  /*! @brief Assign the Stereopermutator randomly using relative statistical weights
   *
   * Stereopermutations are generated with relative statistical occurrence
   * weights. The assignment is then chosen from the possible stereopermutations
   * with a discrete distribution whose weights are the corresponding relative
   * statistical occurrences.
   *
   * @complexity{@math{\Theta(1)} if @p assignment is @c boost::none.
   * @math{\Theta(S)} otherwise}
   *
   * @parblock @note If the stereocenter is already assigned, it is reassigned.
   * @endparblock
   *
   * @parblock @note The state of the passed PRNG is advanced.
   * @endparblock
   */
  void assignRandom(random::Engine& engine);

  /** @brief Applies an atom index permutation
   *
   * @complexity{@math{\Theta(1)}}
   *
   * @param permutation The permutation to apply
   */
  void applyPermutation(const std::vector<AtomIndex>& permutation);

  /*! @brief Determine the shape and assignment realized in positions
   *
   * The shape and assignment are determined based on three-dimensional
   * positions using angle and chiral distortions from the respective idealized
   * shapes.
   *
   * @param graph The molecule's graph which this permutator helps model
   * @param angstromWrapper The wrapped positions
   *
   * @complexity{@math{\Theta(S!)}}
   *
   * @note If Options::tauCriterion is set to @p Enable, this function may
   *   exclude some shapes from the fitting procedure based on geometric
   *   criteria.
   */
  void fit(
    const OuterGraph& graph,
    const AngstromWrapper& angstromWrapper
  );

  /*! @brief Propagate the stereocenter state through a possible ranking change
   *
   * In case a graph modification changes the ranking of this stereopermutator's
   * substituents, it must be redetermined whether the new configuration is a
   * stereopermutator and if so, which assignment corresponds to the previous one.
   *
   * @complexity{@math{L\cdot S!} where @math{L} is the number of links and
   * @math{S} is the size of @p shape}
   */
  boost::optional<PropagatedState> propagate(
    const OuterGraph& graph,
    RankingInformation newRanking,
    boost::optional<Shapes::Shape> shapeOption
  );

  /*! @brief Adapts atom indices in the internal state to the removal of an atom
   *
   * Atom indices are adapted to a graph-level removal of an atom. The removed
   * index is changed to a placeholder value.
   *
   * @complexity{@math{\Theta(1)}}
   */
  void propagateVertexRemoval(AtomIndex removedIndex);

  /*! @brief Change the underlying shape of the permutator
   *
   * @complexity{@math{L\cdot S!} where @math{L} is the number of links and
   * @math{S} is the size of @p shape}
   *
   * @todo Consider trying to propagate within same shape size
   * @post The permutator is unassigned (chiral state is discarded)
   */
  void setShape(
    Shapes::Shape shape,
    const OuterGraph& graph
  );
//!@}

//!@name Information
//!@{
  /*! @brief Fetches angle between binding sites in the idealized shape
   *
   * @param i Site index one
   * @param j Site index two
   *
   * @complexity{@math{\Theta(1)}}
   *
   * @pre @p i and @p j are valid ligand indices into the underlying
   * RankingInformation's RankingInformation#ligands member.
   *
   * @sa getRanking()
   */
  double angle(unsigned i, unsigned j) const;

  /*! @brief Returns the permutation index within the set of feasible permutations, if set
   *
   * Returns the information of whether the stereopermutator is assigned
   * or not, and if so, which assignment it is.
   *
   * @complexity{@math{\Theta(1)}}
   */
  boost::optional<unsigned> assigned() const;

  /*! @brief Returns the central atom this permutator is placed on
   *
   * @complexity{@math{\Theta(1)}}
   */
  AtomIndex centralIndex() const;

  /*! @brief Returns IOP within the set of symbolic ligand permutations
   *
   * This is different to the assignment. The assignment denotes the index
   * within the set of possible (more specifically, not obviously infeasible)
   * stereopermutations.
   *
   * @complexity{@math{\Theta(1)}}
   */
  boost::optional<unsigned> indexOfPermutation() const;

  /*! @brief Returns a minimal representation of chiral constraints
   *
   * Every minimal representation consists only of site indices. If no site
   * index is present, this position is the location of the central atom.
   *
   * The minimal representation assumes that all shape tetrahedra are
   * defined as Positive targets, which is checked in the
   * shapes tests.
   *
   * @complexity{@math{\Theta(T)} where @math{T} is the number of tetrahedra
   * defined for the modeled shape}
   *
   * @param enforce Emit minimal representations of chiral constraints even if
   * the stereopermutator does not have any chiral state, i.e.
   * numStereopermutators() <= 1, as long as it is assigned.
   */
  std::vector<MinimalChiralConstraint> minimalChiralConstraints(bool enforce = false) const;

  /*! @brief Returns an information string for diagnostic purposes
   *
   * @complexity{@math{\Theta(1)}}
   */
  std::string info() const;

  /*! @brief Returns an information string for ranking equality checking purposes
   *
   * @complexity{@math{\Theta(1)}}
   */
  std::string rankInfo() const;

  /*! @brief Returns the underlying feasible stereopermutations object
   *
   * @complexity{@math{\Theta(1)}}
   * @note This is library-internal and not part of the public API
   */
  const AbstractStereopermutations& getAbstract() const;

  /*!  @brief Returns the underlying feasible stereopermutations object
   *
   * @complexity{@math{\Theta(1)}}
   * @note This is library-internal and not part of the public API
   */
  const FeasibleStereopermutations& getFeasible() const;

  /*! @brief Returns the underlying ranking
   *
   * @complexity{@math{\Theta(1)}}
   */
  const RankingInformation& getRanking() const;

  /*! @brief Returns the underlying shape
   *
   * @complexity{@math{\Theta(1)}}
   */
  Shapes::Shape getShape() const;

  /*! @brief Yields the mapping from site indices to shape positions
   *
   * @complexity{@math{\Theta(1)}}
   * @throws std::logic_error if the stereopermutator is unassigned.
   */
  const std::vector<unsigned>& getShapePositionMap() const;

  /*! @brief Returns the number of possible assignments
   *
   * The number of possible assignments is the number of non-superposable
   * arrangements of the abstract ligand case reduced by trans-arranged
   * multidentate pairs where the bridge length is too short or overlapping
   * haptic cones.
   *
   * For instance, if octahedral M[(A-A)3], there are four abstract arrangements
   * - trans-trans-trans
   * - trans-cis-cis
   * - 2x cis-cis-cis (Δ and Λ isomers, ship propeller-like chirality)
   *
   * However, the number of stereopermutations for a concrete case in which the
   * bridges are too short to allow trans bonding is reduced by all
   * arrangements containing a trans-bonded bidentate ligand, i.e. only Δ and Λ
   * remain. The number of assignments is then only two.
   *
   * This is the upper exclusive bound on Some-type arguments to assign().
   *
   * @complexity{@math{\Theta(1)}}
   */
  unsigned numAssignments() const;

  /*!
   * @brief Returns the number of possible stereopermutations
   *
   * The number of possible stereopermutations is the number of
   * non-superposable arrangements of the abstract ligand case without removing
   * trans-arranged multidentate pairs or overlapping haptic cones.
   *
   * For instance, if octahedral M[(A-A)3], there are four abstract arrangements
   * - trans-trans-trans
   * - trans-cis-cis
   * - 2x cis-cis-cis (Δ and Λ isomers, ship propeller-like chirality)
   *
   * However, the number of assignments for a concrete case in which the bridges
   * are too short to allow trans binding is reduced by all arrangements
   * containing a trans-bonded bidentate ligand, i.e. only Δ and Λ remain.
   *
   * Fetches the number of permutations determined by symbolic ligand
   * calculation, not considering linking or haptic ligand cones.
   *
   * @complexity{@math{\Theta(1)}}
   */
  unsigned numStereopermutations() const;
//!@}


//!@name Operators
//!@{
  /** @brief Checks whether the underlying shape, central atom index, number
   *   of stereopermutations and current assignment match
   *
   * @param other The other atom stereopermutator to compare against
   *
   * @return Whether the underlying shape, central atom index, number of
   *   stereopermutations and current assignment match
   */
  bool operator == (const AtomStereopermutator& other) const;
  //! Inverts operator ==
  bool operator != (const AtomStereopermutator& other) const;

  /**
   * @brief Lexicographically compares the central atom index, the shape,
   *   the number of stereopermutations, and the current assignment
   *
   * @param other The other atom stereopermutator to compare against
   *
   * @return Which atom stereopermutator is lexicographically smaller
   */
  bool operator < (const AtomStereopermutator& other) const;
//!@}

private:
  class Impl;

#ifdef MOLASSEMBLER_ENABLE_PROPAGATE_CONST
  std::experimental::propagate_const<
    std::unique_ptr<Impl>
  > _pImpl;
#else
  std::unique_ptr<Impl> _pImpl;
#endif
};

} // namespace molassembler

} // namespace Scine

#endif
