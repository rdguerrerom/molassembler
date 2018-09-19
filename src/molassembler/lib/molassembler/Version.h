// Copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
// See LICENSE.txt for details.

#ifndef INCLUDE_MOLASSEMBLER_VERSION_H
#define INCLUDE_MOLASSEMBLER_VERSION_H

#include <string>

/*! @file
 *
 * @brief Library versioning scheme information.
 *
 * This library adheres to semantic versioning
 * (http://semver.org/spec/v2.0.0.html).
 */

namespace molassembler {

namespace version {

//! The major version number. Incremented on incompatible API changes
constexpr unsigned major = 0;
//! The minor version number. Incremented on backwards-compatible functionality additions
constexpr unsigned minor = 1;
//! The patch version number. Incremented on backwards-compatible bug fixes
constexpr unsigned patch = 0;

//! Major.minor string
inline std::string majorMinor() {
  return std::to_string(major) + "." + std::to_string(minor);
}

//! Major.minor.patch string
inline std::string fullVersion() {
  return (
    std::to_string(major)
    + "." + std::to_string(minor)
    + "." + std::to_string(patch)
  );
}

} // namespace version

} // namespace molassembler

#endif
