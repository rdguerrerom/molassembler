/*!@file
 * @copyright This code is licensed under the 3-clause BSD license.
 *   Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.
 *   See LICENSE.txt for details.
 * @brief Algorithms available in the C++17 STL
 */

#ifndef INCLUDE_TEMPLE_STL_17
#define INCLUDE_TEMPLE_STL_17

#include <type_traits>
#include <cassert>
#include <functional>

namespace Scine {
namespace Molassembler {
namespace Temple {
namespace Stl17 {

// From cppreference, possible C++17 clamp implementation
template<class T, class Compare>
constexpr const T& clamp( const T& v, const T& lo, const T& hi, Compare comp ) {
    return assert( !comp(hi, lo) ),
        comp(v, lo) ? lo : comp(hi, v) ? hi : v;
}

template<class T>
constexpr const T& clamp( const T& v, const T& lo, const T& hi ) {
    return clamp( v, lo, hi, std::less<T>() );
}

template<class T>
constexpr std::add_const_t<T>& as_const(T& t) noexcept {
    return t;
}

} // namespace Stl17
} // namespace Temple
} // namespace Molassembler
} // namespace Scine

#endif
