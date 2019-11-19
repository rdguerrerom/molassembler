/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 */
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/operators.h"
#include "OptionalPython.h"

#include "molassembler/Molecule.h"
#include "molassembler/OuterGraph.h"
#include "molassembler/StereopermutatorList.h"
#include "molassembler/Serialization.h"

#include "Utils/Geometry/ElementTypes.h"
#include "Utils/Geometry/AtomCollection.h"
#include "Utils/Geometry/FormulaGenerator.h"

#include "boost/process/child.hpp"
#include "boost/process/io.hpp"
#include "boost/process/search_path.hpp"

bool graphvizInPath() {
  return !boost::process::search_path("dot").empty();
}

std::string pipeSVG(const Scine::molassembler::Molecule& molecule) {
  std::string callString = "dot -Tsvg";
  std::stringstream os;

  // Construct pipe streams for redirection
  boost::process::opstream ips;
  boost::process::pstream ps;
  boost::process::pstream err;

  // Start the child process
  boost::process::child childProcess(callString, boost::process::std_in<ips, boost::process::std_out> ps,
                                     boost::process::std_err > err);

  // Feed our graphviz into the process
  ips << molecule.dumpGraphviz();
  ips.flush();
  ips.pipe().close();

  // Wait for the child process to exit
  childProcess.wait();

  std::stringstream stderrStream;
#if BOOST_VERSION >= 107000
  /* NOTE: This implementation of buffer transfers in boost process has a bug
   * that isn't fixed before Boost 1.70.
   */
  os << ps.rdbuf();
  stderrStream << err.rdbuf();
#else
  // Workaround: cast to a parent class implementing rdbuf() correctly.
  using BasicIOSReference = std::basic_ios<char, std::char_traits<char>>&;
  // Feed the results into our ostream
  os << static_cast<BasicIOSReference>(ps).rdbuf();
  stderrStream << static_cast<BasicIOSReference>(err).rdbuf();
#endif

  return os.str();
}

void init_molecule(pybind11::module& m) {
  using namespace Scine::molassembler;
  using namespace Scine::Utils;

  pybind11::class_<Molecule> molecule(
    m,
    "Molecule",
    "Models a molecule as a graph and a list of stereopermutators"
  );

  /* Constructors */
  molecule.def(
    pybind11::init<>(),
    R"delim(
      Initialize a hydrogen molecule

      >>> h2 = Molecule()
      >>> h2.graph.N
      2
      >>> h2.graph.B
      1
    )delim"
  );

  molecule.def(
    pybind11::init<ElementType>(),
    R"delim(
      Initialize a single-atom molecule.

      This is a bit of a paradox, yes, and it might have been preferable for
      the concept of a molecule to contain at least two bonded atoms, but
      unfortunately single atoms occur everywhere and enforcing the concept
      would complicate many interfaces.

      >>> import scine_utils_os as utils
      >>> f = Molecule(utils.ElementType.F)
      >>> f.graph.N
      1
      >>> f.graph.B
      0
    )delim"
  );

  molecule.def(
    pybind11::init<ElementType, ElementType, BondType>(),
    pybind11::arg("first_element"),
    pybind11::arg("second_element"),
    pybind11::arg("bond_type") = BondType::Single,
    R"delim(
      Initialize a molecule from two element types and a mutual :class:`BondType`

      >>> import scine_utils_os as utils
      >>> hf = Molecule(utils.ElementType.H, utils.ElementType.F)
      >>> hf.graph.N == 2
      True
    )delim"
  );

  molecule.def(
    pybind11::init<OuterGraph>(),
    pybind11::arg("graph"),
    R"delim(
      Initialize a molecule from connectivity alone, inferring shapes and
      stereopermutators from the graph.

      >>> # Rebuild a molecule with an assigned stereopermutator from just the graph
      >>> import molassembler as masm
      >>> a = masm.io.experimental.from_smiles("[C@](F)(Cl)(C)[H]")
      >>> a.stereopermutators.has_unassigned_permutators()
      False
      >>> b = Molecule(a.graph)
      >>> b.stereopermutators.has_unassigned_permutators()
      True
    )delim"
  );

  molecule.def(
    "hash",
    &Molecule::hash,
    R"delim(
      Calculates a convoluted hash of a molecule. The molecule must be at least
      partially canonical. Hashes between molecules of different canonicity are
      not comparable.

      >>> import molassembler as masm
      >>> from copy import copy
      >>> spiro = masm.io.experimental.from_smiles("C12(CCC1)CCC2")
      >>> # We make two variants of the molecule that have different canonicalization states
      >>> # to demonstrate that their hashes are unequal. We discard the mappings
      >>> # we get from canonicalize()
      >>> partially_canonical = copy(spiro)
      >>> _ = partially_canonical.canonicalize(masm.AtomEnvironmentComponents.ElementsAndBonds)
      >>> fully_canonical = copy(spiro)
      >>> _ = fully_canonical.canonicalize()
      >>> partially_canonical == fully_canonical
      True
      >>> partially_canonical.hash() == fully_canonical.hash()
      False
    )delim"
  );

  molecule.def(
    "__hash__",
    &Molecule::hash
  );

  molecule.def_static(
    "apply_canonicalization_map",
    &Molecule::applyCanonicalizationMap,
    pybind11::arg("canonicalization_index_map"),
    pybind11::arg("atom_collection"),
    R"delim(
      Reorders an atom collection according to an index mapping from
      canonicalization.

      :param canonicalization_index_map: Index mapping saved from previous
        canonicalization
      :param atom_collection: Atom collection to reorder
      :return: Reordered atom collection
    )delim"
  );

  /* Modifiers */
  molecule.def(
    "add_atom",
    &Molecule::addAtom,
    pybind11::arg("element"),
    pybind11::arg("adjacent_to"),
    pybind11::arg("bond_type") = BondType::Single,
    R"delim(
      Add an atom to the molecule, attaching it to an existing atom by a
      specified bond type.

      :param element: Element type of the new atom
      :param adjacent_to: Atom to which the new atom is added
      :param bond_type: :class:`BondType` with which the new atom is attached

      >>> import scine_utils_os as utils
      >>> mol = Molecule() # Default constructor makes H2
      >>> _ = mol.add_atom(utils.ElementType.H, 0) # Make linear H3
    )delim"
  );

  molecule.def(
    "add_bond",
    &Molecule::addBond,
    pybind11::arg("first_atom"),
    pybind11::arg("second_atom"),
    pybind11::arg("bond_type") = BondType::Single,
    R"delim(
      Adds a bond between two existing atoms.

      :param first_atom: First atom to bond
      :param second_atom: Second atom to bond
      :param bond_type: :class:`BondType` with which to bond the atoms

      >>> import scine_utils_os as utils
      >>> mol = Molecule() # Default constructor makes H2
      >>> _ = mol.add_atom(utils.ElementType.H, 0) # Make linear H3
      >>> _ = mol.add_bond(1, 2) # Make triangular H3
    )delim"
  );

  molecule.def(
    "assign_stereopermutator",
    pybind11::overload_cast<AtomIndex, const boost::optional<unsigned>&>(
      &Molecule::assignStereopermutator
    ),
    pybind11::arg("atom"),
    pybind11::arg("assignment_option"),
    R"delim(
      Sets the atom stereopermutator assignment at a particular atom

      :param atom: Atom index of the :class:`AtomStereopermutator` to set
      :param assignment_option: An assignment integer if the stereopermutator
        is to be assigned or ``None`` if the stereopermutator is to be dis-assigned.

      >>> # Assign an unspecified asymmetric carbon atom and then dis-assign it
      >>> import molassembler as masm
      >>> mol = masm.io.experimental.from_smiles("F[CH1](Br)C")
      >>> asymmetric_carbon_index = 1
      >>> mol.assign_stereopermutator(asymmetric_carbon_index, 0)
      >>> mol.stereopermutators.option(asymmetric_carbon_index).assigned
      0
      >>> mol.assign_stereopermutator(asymmetric_carbon_index, None)
      >>> mol.stereopermutators.option(asymmetric_carbon_index).assigned is None
      True
    )delim"
  );

  molecule.def(
    "assign_stereopermutator",
    pybind11::overload_cast<const BondIndex&, const boost::optional<unsigned>&>(
      &Molecule::assignStereopermutator
    ),
    pybind11::arg("bond_index"),
    pybind11::arg("assignment_option"),
    R"delim(
      Sets the bond stereopermutator assignment at a particular bond

      :param bond_index: :class:`BondIndex` of the :class:`BondStereopermutator` to set
      :param assignment_option: An assignment integer if the stereopermutator
        is to be assigned or ``None`` if the stereopermutator is to be
        dis-assigned.

      >>> import molassembler as masm
      >>> mol = masm.io.experimental.from_smiles("C/C=C\C")
      >>> double_bond_index = masm.BondIndex(1, 2)
      >>> assert mol.graph.bond_type(double_bond_index) == masm.BondType.Double
      >>> mol.stereopermutators.option(double_bond_index).assigned is not None
      True
      >>> mol.assign_stereopermutator(double_bond_index, None)
      >>> mol.stereopermutators.option(double_bond_index).assigned is not None
      False
    )delim"
  );

  molecule.def(
    "assign_stereopermutator_randomly",
    [](Molecule& mol, AtomIndex a) {
      mol.assignStereopermutatorRandomly(a, randomnessEngine());
    },
    pybind11::arg("atom"),
    R"delim(
      Assigns an :class:`AtomStereopermutator` at random (assignments are
      weighted by relative statistical occurence).

      :param atom: Atom index of the stereopermutator to assign randomly.

      >>> # Assign an unspecified chiral center
      >>> import molassembler as masm
      >>> mol = masm.io.experimental.from_smiles("S[As](F)(Cl)(Br)N")
      >>> as_index = 1
      >>> mol.stereopermutators.option(as_index).assigned is None
      True
      >>> mol.assign_stereopermutator_randomly(1)
      >>> mol.stereopermutators.option(as_index).assigned is None
      False
    )delim"
  );

  molecule.def(
    "assign_stereopermutator_randomly",
    [](Molecule& mol, const BondIndex& b) {
      mol.assignStereopermutatorRandomly(b, randomnessEngine());
    },
    pybind11::arg("bond_index"),
    R"delim(
      Assigns a :class:`BondStereopermutator` at random.

      :param bond_index: :class:`BondIndex` of the stereopermutator to assign randomly.

      >>> # Assign an unspecified double bond randomly
      >>> import molassembler as masm
      >>> mol = masm.io.experimental.from_smiles("CC=CC")
      >>> double_bond_index = masm.BondIndex(1, 2)
      >>> assert mol.graph.bond_type(double_bond_index) == masm.BondType.Double
      >>> mol.stereopermutators.option(double_bond_index).assigned is None
      True
      >>> mol.assign_stereopermutator_randomly(double_bond_index)
      >>> mol.stereopermutators.option(double_bond_index).assigned is None
      False
    )delim"
  );

  molecule.def(
    "canonicalize",
    &Molecule::canonicalize,
    pybind11::arg("components_bitmask") = AtomEnvironmentComponents::All,
    R"delim(
      Transform the molecule to a canonical form. Invalidates all atom and bond
      indices.

      :param components_bitmask: The components of the molecular graph to
        include in the canonicalization procedure.
      :return: Flat index mapping/permutation from old indices to new

      >>> # Create two different representations of the same molecule
      >>> import molassembler as masm
      >>> a = masm.io.experimental.from_smiles("N[C@](Br)(O)C")
      >>> b = masm.io.experimental.from_smiles("Br[C@](O)(N)C")
      >>> # a and be represent the same molecule, but have different vertex order
      >>> a == b # Equality operators perform an isomorphism for non-canonical pairs
      True
      >>> amap = a.canonicalize()
      >>> bmap = b.canonicalize()
      >>> amap == bmap # This shows the vertex order was different
      False
      >>> a == b # Equality operators perform a same-graph test for canonical pairs (faster)
      True
    )delim"
  );

  molecule.def(
    "remove_atom",
    &Molecule::removeAtom,
    pybind11::arg("atom"),
    R"delim(
      Remove an atom from the graph, including bonds to it, after checking
      that removing it is safe, i.e. the removal does not disconnect the graph.
      Invalidates all atom and bond indices.

      :param atom: Atom to remove
    )delim"
  );

  molecule.def(
    "remove_bond",
    pybind11::overload_cast<AtomIndex, AtomIndex>(&Molecule::removeBond),
    pybind11::arg("first_atom"),
    pybind11::arg("second_atom"),
    R"delim(
      Remove a bond from the graph, after checking that removing it is safe,
      i.e. the removal does not disconnect the graph. Invalidates all atom and
      bond indices.

      :param first_atom: First atom of the bond to be removed
      :param second_atom: Second atom of the bond to be removed
    )delim"
  );

  molecule.def(
    "remove_bond",
    pybind11::overload_cast<const BondIndex&>(&Molecule::removeBond),
    pybind11::arg("bond_index"),
    R"delim(
      Remove a bond from the graph, after checking that removing it is safe,
      i.e. the removal does not disconnect the graph.

      :param bond_index: :class:`BondIndex` of the bond to be removed
    )delim"
  );

  molecule.def(
    "set_bond_type",
    &Molecule::setBondType,
    pybind11::arg("first_atom"),
    pybind11::arg("second_atom"),
    pybind11::arg("bond_type"),
    R"delim(
      Change the bond type between two atoms. Inserts the bond if it doesn't
      yet exist.

      :param first_atom: First atom of the bond to be changed
      :param second_atom: Second atom of the bond to be changed
      :param bond_type: The new :class:`BondType`
      :return: Whether the bond already existed

      >>> # You really do have full freedom when it comes to your graphs:
      >>> import molassembler as masm
      >>> h2 = masm.Molecule()
      >>> _ = h2.set_bond_type(0, 1, masm.BondType.Double) # Double bonded hydrogen atoms!
    )delim"
  );

  molecule.def(
    "set_element_type",
    &Molecule::setElementType,
    pybind11::arg("atom"),
    pybind11::arg("element"),
    R"delim(
      Change the element type of an atom.

      :param atom: Atom index of the atom to alter
      :param element: New element type to set

      >>> # Transform H2 into HF
      >>> import molassembler as masm
      >>> import scine_utils_os as utils
      >>> from copy import copy
      >>> H2 = masm.Molecule()
      >>> HF = copy(H2)
      >>> HF.set_element_type(0, utils.ElementType.F)
      >>> HF == H2
      False
    )delim"
  );

  molecule.def(
    "set_shape_at_atom",
    &Molecule::setShapeAtAtom,
    pybind11::arg("atom"),
    pybind11::arg("shape"),
    R"delim(
      Change the local shape at an atom.

      This sets the local shape at a specific atom index. There are a number of
      cases that this function treats differently, besides faulty arguments: If
      there is already a AtomStereopermutator instantiated at this atom index,
      its underlying shape is altered. If there is no AtomStereopermutator at
      this index, one is instantiated. In all cases, new or modified
      stereopermutators are default-assigned if there is only one possible
      assignment.

      >>> # Make methane square planar
      >>> import molassembler as masm
      >>> from copy import copy
      >>> methane = masm.io.experimental.from_smiles("C")
      >>> square_planar_methane = copy(methane)
      >>> square_planar_methane.set_shape_at_atom(0, masm.shapes.Shape.Square)
      >>> methane == square_planar_methane
      False
    )delim"
  );

  /* Information */
  molecule.def(
    "dump_graphviz",
    &Molecule::dumpGraphviz,
    "Returns a graphviz string representation of the molecule"
  );

  molecule.def_property_readonly(
    "graph",
    &Molecule::graph,
    R"delim(
      Read only access to the graph representation

      :rtype: :class:`Graph`
    )delim"
  );

  molecule.def_property_readonly(
    "stereopermutators",
    &Molecule::stereopermutators,
    R"delim(
      Read only access to the list of stereopermutators

      :rtype: :class:`StereopermutatorList`
    )delim"
  );

  molecule.def_property_readonly(
    "canonical_components",
    &Molecule::canonicalComponents,
    R"delim(
      Yields the components of the molecule that were used in a previous
      canonicalization. Can be ``None`` if the molecule was never
      canonicalized.

      :rtype: :class:`AtomEnvironmentComponents` or ``None``
    )delim"
  );

  molecule.def(
    "canonical_compare",
    &Molecule::canonicalCompare,
    pybind11::arg("other"),
    pybind11::arg("components_bitmask") = AtomEnvironmentComponents::All,
    R"delim(
      Modular comparison of this Molecule with another, assuming that both are
      in a canonical form.

      For comparisons of fully canonical molecule pairs, regular equality
      comparison will just call this function instead of performing a full
      isomorphism.

      :param other: The other (canonical) molecule to compare against
      :param components_bitmask: The components of an atom's environment to
        include in the comparison. You should use the same bitmask as when
        canonicalizing the molecules you are comparing here. It may be possible
        to use a bitmask with fewer components, but certainly not one with more.
    )delim"
  );

  molecule.def(
    "partial_compare",
    &Molecule::modularCompare,
    pybind11::arg("other"),
    pybind11::arg("components_bitmask"),
    R"delim(
      Modular comparison of this Molecule with another.

      This permits detailed specification of which elements of the molecular
      information you want to use in the comparison.

      Equality comparison is performed in several stages: First, at each atom
      position, a hash is computed that encompasses all local information that
      is specified to be used by the components_bitmask parameter. This hash is
      then used during graph isomorphism calculation to avoid finding an
      isomorphism that does not consider the specified factors.

      If an isomorphism is found, it is then validated. Bond orders and
      stereopermutators across both molecules are compared using the found
      isomorphism as an index map.

      Note that this function is not faster for molecules stored in any
      (possibly partially) canonical form. Use canonical_compare for molecules
      that have been canonicalized to some degree. Note also that equality
      comparison defaults to fast comparisons if both instances are fully
      canonical.

      :param other: The molecule to compare against
      :param components_bitmask: The components of the molecule to use in the
        comparison
    )delim"
  );

  molecule.def(pybind11::self == pybind11::self);
  molecule.def(pybind11::self != pybind11::self);

  /* Integration with IPython / Jupyter */
  molecule.def(
    "_repr_svg_",
    &::pipeSVG,
    "Generates an SVG representation of the molecule"
  );

  // Shell integration
  molecule.def(
    "__repr__",
    [](const Molecule& mol) -> std::string {
      std::string repr = "Molecule of elemental composition ";
      repr += Scine::Utils::generateChemicalFormula(mol.graph().elementCollection());
      const unsigned A = mol.stereopermutators().A();
      const unsigned B = mol.stereopermutators().B();

      if(A > 0) {
        repr += " with " + std::to_string(A) + " atom ";

        if(B > 0) {
          repr += "and " + std::to_string(B) + " bond ";
        }
        repr += "stereopermutator";
        if(A + B > 1) {
          repr += "s";
        }
      }

      return repr;
    },
    "Generate a string representation of the molecule"
  );

  /* Pickling support (allows copying) */
  molecule.def(
    pybind11::pickle(
      [](const Molecule& mol) {
        JSONSerialization serialization(mol);
        std::string serialized = serialization;
        return serialized;
      },
      [](const std::string& serialized) {
        JSONSerialization serialization(serialized);
        Molecule mol = serialization;
        return mol;
      }
    )
  );
}
