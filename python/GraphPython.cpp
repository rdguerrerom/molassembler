/*!@file
 * @copyright This code is licensed under the 3-clause BSD license.
 *   Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.
 *   See LICENSE.txt for details.
 */
#include "TypeCasters.h"
#include "pybind11/operators.h"

#include "Molassembler/Cycles.h"
#include "Molassembler/Graph.h"
#include "Molassembler/GraphAlgorithms.h"
#include "Molassembler/Graph/GraphAlgorithms.h"

#include "Utils/Bonds/BondOrderCollection.h"
#include "Utils/Geometry/FormulaGenerator.h"
#include "Utils/Typenames.h"

#include "boost/process/child.hpp"
#include "boost/process/io.hpp"
#include "boost/process/search_path.hpp"

namespace {

bool graphvizInPath() {
  return !boost::process::search_path("dot").empty();
}

std::string pipeSVG(const Scine::Molassembler::Graph& graph) {
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
  ips << graph.dumpGraphviz();
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

} // namespace

void init_graph(pybind11::module& m) {
  using namespace Scine::Molassembler;
  pybind11::class_<Graph> graphClass(
    m,
    "Graph",
    R"delim(
      Molecular graph in which atoms are vertices and bonds are edges.

      >>> import scine_utilities as utils
      >>> ethane = io.experimental.from_smiles("CC")
      >>> g = ethane.graph
      >>> g.atoms_of_element(utils.ElementType.C)
      [0, 1]
      >>> g.degree(0)
      4
      >>> g.can_remove(0)
      False
      >>> g.can_remove(BondIndex(0, 1))
      False
      >>> hydrogen_indices = g.atoms_of_element(utils.ElementType.H)
      >>> can_remove = lambda a : g.can_remove(a)
      >>> all(map(can_remove, hydrogen_indices))
      True
      >>> g.N
      8
      >>> g.B
      7
    )delim"
  );

  graphClass.def(
    "adjacent",
    &Graph::adjacent,
    pybind11::arg("first_atom"),
    pybind11::arg("second_atom"),
    R"delim(
      Returns whether two atoms are bonded

      >>> ethane = io.experimental.from_smiles("CC")
      >>> ethane.graph.degree(0)
      4
      >>> [ethane.graph.adjacent(0, a) for a in range(1, ethane.graph.N)]
      [True, True, True, True, False, False, False]
    )delim"
  );

  graphClass.def(
    "atoms_of_element",
    &Graph::atomsOfElement,
    pybind11::arg("element_type"),
    R"delim(
      Returns atoms matching an element type

      >>> import scine_utilities as utils
      >>> ethanol = io.experimental.from_smiles("CCO")
      >>> ethanol.graph.atoms_of_element(utils.ElementType.O)
      [2]
      >>> ethanol.graph.atoms_of_element(utils.ElementType.C)
      [0, 1]
    )delim"
  );

  graphClass.def(
    "bond_orders",
    &Graph::bondOrders,
    R"delim(
      Generates a BondOrderCollection representation of the molecule connectivity

      >>> # Convert acetaldehyde's graph into a floating point bond order matrix
      >>> import scine_utilities as utils
      >>> acetaldehyde = io.experimental.from_smiles("CC=O")
      >>> bo = acetaldehyde.graph.bond_orders()
      >>> bo.empty()
      False
      >>> bo.get_order(0, 1) # The order between the carbon atoms
      1.0
      >>> bo.get_order(1, 2) # The order between a carbon and oxygen
      2.0
    )delim"
  );

  graphClass.def(
    "bond_type",
    &Graph::bondType,
    pybind11::arg("bond_index"),
    R"delim(
      Fetches the :class:`BondType` at a particular :class:`BondIndex`

      >>> # Look at some bond orders of an interesting model compound
      >>> compound = io.experimental.from_smiles("[Co]1(C#N)(C#O)C=C1")
      >>> compound.graph.bond_type(BondIndex(0, 1)) # Co-CN bond
      BondType.Single
      >>> compound.graph.bond_type(BondIndex(0, 5)) # Co-C=C bond
      BondType.Eta
      >>> compound.graph.bond_type(BondIndex(5, 6)) # C=C bond
      BondType.Double
      >>> compound.graph[BondIndex(1, 2)] # C#N bond by bond subsetting
      BondType.Triple
    )delim"
  );

  graphClass.def(
    "can_remove",
    pybind11::overload_cast<AtomIndex>(&Graph::canRemove, pybind11::const_),
    pybind11::arg("atom"),
    R"delim(
      Returns whether an atom can be removed without disconnecting the graph

      >>> # In graph terms, articulation vertices cannot be removed
      >>> methane = io.experimental.from_smiles("C")
      >>> methane.graph.can_remove(0) # We cannot remove the central carbon
      False
      >>> all([methane.graph.can_remove(i) for i in range(1, 5)]) # But hydrogens!
      True
    )delim"
  );

  graphClass.def(
    "can_remove",
    pybind11::overload_cast<const BondIndex&>(&Graph::canRemove, pybind11::const_),
    pybind11::arg("bond_index"),
    R"delim(
      Returns whether a bond can be removed without disconnecting the graph

      >>> # In graph terms, bridge edges cannot be removed
      >>> import scine_utilities as utils
      >>> from itertools import combinations
      >>> cyclopropane = io.experimental.from_smiles("C1CC1")
      >>> carbon_atoms = cyclopropane.graph.atoms_of_element(utils.ElementType.C)
      >>> cc_bonds = [BondIndex(a, b) for (a, b) in combinations(carbon_atoms, 2)]
      >>> can_remove = lambda b: cyclopropane.graph.can_remove(b)
      >>> all(map(can_remove, cc_bonds)) # We can remove any one of the bonds
      True
      >>> cyclopropane.remove_bond(cc_bonds[0]) # Remove one C-C bond
      >>> any(map(can_remove, cc_bonds[1:])) # Can we still remove any of the others?
      False
    )delim"
  );

  graphClass.def_property_readonly(
    "cycles",
    &Graph::cycles,
    "Fetch a reference to cycles information"
  );

  graphClass.def(
    "degree",
    &Graph::degree,
    pybind11::arg("atom"),
    R"delim(
      Returns the number of bonds incident upon an atom.

      >>> # A silly example
      >>> model = io.experimental.from_smiles("CNO[H]")
      >>> [model.graph.degree(i) for i in range(0, 4)]
      [4, 3, 2, 1]
    )delim"
  );

  graphClass.def(
    "elements",
    &Graph::elementCollection,
    R"delim(
      Generates an ElementCollection representation of the molecule's atoms' element types

      >>> # Some isotopes
      >>> import scine_utilities as utils
      >>> m = io.experimental.from_smiles("[1H]C([2H])([3H])[H]")
      >>> m.graph.elements()
      [ElementType.H1, ElementType.C, ElementType.D, ElementType.T, ElementType.H]
    )delim"
  );

  graphClass.def(
    "element_type",
    &Graph::elementType,
    pybind11::arg("atom"),
    R"delim(
      Fetch the element type of an atom

      >>> # Some isotopes
      >>> import scine_utilities as utils
      >>> m = io.experimental.from_smiles("[1H]C([2H])([3H])[H]")
      >>> m.graph.element_type(0)
      ElementType.H1
      >>> m.graph.element_type(2)
      ElementType.D
      >>> m.graph[4] # Subsettable with atom indices to get element types
      ElementType.H
    )delim"
  );

  graphClass.def_property_readonly("N", &Graph::N, "The number of atoms in the graph");
  graphClass.def_property_readonly("B", &Graph::B, "The number of bonds in the graph");

  graphClass.def(
    "split_along_bridge",
    &Graph::splitAlongBridge,
    pybind11::arg("bridge_bond"),
    R"delim(
      Determine which atoms belong to either side of a bond

      >>> # Hypothetically splitting a model compound
      >>> m = io.experimental.from_smiles("CN")
      >>> m.graph.split_along_bridge(BondIndex(0, 1))
      ([0, 2, 3, 4], [1, 5, 6])
    )delim"
  );

  graphClass.def(
    "atoms",
    [](const Graph& graph) {
      auto atomsRange = graph.atoms();
      return pybind11::make_iterator(
        std::move(atomsRange.first),
        std::move(atomsRange.second)
      );
    },
    R"delim(
      Iterate through all valid atom indices of the graph

      Fully equivalent to: ``range(graph.N)``
    )delim"
  );

  graphClass.def(
    "bonds",
    [](const Graph& graph) {
      auto bondsRange = graph.bonds();
      return pybind11::make_iterator(
        std::move(bondsRange.first),
        std::move(bondsRange.second)
      );
    },
    R"delim(
      Iterate through all valid bond indices of the graph

      >>> import scine_utilities as utils
      >>> model = io.experimental.from_smiles("F/C=C/I")
      >>> [b for b in model.graph.bonds()]
      [(0, 1), (1, 2), (2, 3), (1, 4), (2, 5)]
    )delim"
  );

  graphClass.def(
    "adjacents",
    [](const Graph& graph, AtomIndex a) {
      auto adjacentsRange = graph.adjacents(a);
      return pybind11::make_iterator(
        std::move(adjacentsRange.first),
        std::move(adjacentsRange.second)
      );
    },
    pybind11::arg("a"),
    R"delim(
      Iterate through all adjacent atom indices of an atom

      >>> import scine_utilities as utils
      >>> m = io.experimental.from_smiles("NC")
      >>> [a for a in m.graph.adjacents(0)]
      [1, 2, 3]
      >>> element = lambda a: m.graph.element_type(a)
      >>> [element(a) for a in m.graph.adjacents(0)]
      [ElementType.C, ElementType.H, ElementType.H]
    )delim"
  );

  graphClass.def(
    "bonds",
    [](const Graph& graph, AtomIndex a) {
      auto bondsRange = graph.bonds(a);
      return pybind11::make_iterator(
        std::move(bondsRange.first),
        std::move(bondsRange.second)
      );
    },
    pybind11::arg("a"),
    R"delim(
      Iterate through all incident bonds of an atom

      >>> import scine_utilities as utils
      >>> m = io.experimental.from_smiles("NC")
      >>> [b for b in m.graph.bonds(0)]
      [(0, 1), (0, 2), (0, 3)]
    )delim"
  );

  graphClass.def(
    "__repr__",
    [](const Graph& graph) {
      return (
        "Graph of elemental composition "
        + Scine::Utils::generateChemicalFormula(graph.elementCollection())
      );
    }
  );

  // Integration with IPython / Jupyter
  if(graphvizInPath()) {
    graphClass.def(
      "_repr_svg_",
      &::pipeSVG,
      "Generates an SVG representation of the graph"
    );
  }

  // Comparison operators
  graphClass.def(pybind11::self == pybind11::self);
  graphClass.def(pybind11::self != pybind11::self);

  // Square bracket operators
  graphClass.def(
    "__getitem__",
    [](const Graph& g, const AtomIndex i) -> Scine::Utils::ElementType {
      return g.elementType(i);
    }
  );

  graphClass.def(
    "__getitem__",
    [](const Graph& g, const BondIndex i) -> BondType {
      return g.bondType(i);
    }
  );

  m.def(
    "distance",
    &distance,
    pybind11::arg("source"),
    pybind11::arg("graph"),
    R"delim(
      Calculates graph distances from a single atom index to all others

      >>> m = io.experimental.from_smiles("CC(CC)C")
      >>> distances = distance(1, m.graph)
    )delim"
  );

  m.def(
    "sites",
    [](const Graph& graph, const AtomIndex v) {
      return GraphAlgorithms::sites(graph.inner(), v);
    },
    pybind11::arg("graph"),
    pybind11::arg("atom"),
    R"delim(
      Returns adjacents of an atom of the graph grouped into sites

      Sites consisting of multiple atoms are haptic.
    )delim"
  );
}
