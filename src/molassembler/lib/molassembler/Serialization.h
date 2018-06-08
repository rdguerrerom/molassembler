#ifndef INCLUDE_MOLASSEMBLER_SERIALIZE_H
#define INCLUDE_MOLASSEMBLER_SERIALIZE_H

#include "Molecule.h"

/*!@file
 *
 * Provides serialization / deserialization for Molecule instances
 */

namespace molassembler {

//! Serializes a molecule into a verbose JSON representation
std::string toJSON(const Molecule& molecule);
//! Serializes a molecule into compact binary object notation
std::vector<std::uint8_t> toCBOR(const Molecule& molecule);
//! Serializes a molecule into base 64 encoded compact binary object representation
std::string toBase64EncodedCBOR(const Molecule& molecule);

//! Deserialize a JSON string representation of a Molecule
Molecule fromJSON(const std::string& serializedMolecule);
//! Deserialize a compact binary object notation serialization of a molecule
Molecule fromCBOR(const std::vector<std::uint8_t>& cbor);
//! Deserialize a base 64 encoded compact binary object representation of a molecule
Molecule fromBase64EncodedCBOR(const std::string& base64EncodedCBOR);

} // namespace molassembler

#endif