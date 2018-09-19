// Copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
// See LICENSE.txt for details.

#ifndef INCLUDE_MOLASSEMBLER_BOND_ORDERS_H
#define INCLUDE_MOLASSEMBLER_BOND_ORDERS_H

// Forward-declarations
namespace Delib {
class BondOrderCollection;
class ElementTypeCollection;
} // namespace Delib

/*! @file
 *
 * @brief Interpret bond orders from 3D coordinates only
 */

namespace molassembler {

// Forward-declarations
class AngstromWrapper;

/*! Calculates a bond order collection via UFF-like bond distance modelling
 *
 * \warning UFF parameter bond order calculation is a very primitive
 *   approximation and carries a high risk of misinterpretation
 */
Delib::BondOrderCollection uffBondOrders(
  const Delib::ElementTypeCollection& elements,
  const AngstromWrapper& angstromWrapper
);

} // namespace molassembler

#endif
