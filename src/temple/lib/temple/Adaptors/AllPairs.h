#ifndef INCLUDE_MOLASSEMBLER_TEMPLE_ALL_PAIRS_ADAPTOR_H
#define INCLUDE_MOLASSEMBLER_TEMPLE_ALL_PAIRS_ADAPTOR_H

#include "temple/ContainerTraits.h"

#include <tuple>

namespace temple {

namespace adaptors {

namespace detail {

template<class Container>
struct SingleContainerPairsGenerator {
//!@name Types
//!@{
  // See tricks documentation
  using BoundContainer = std::conditional_t<
    std::is_rvalue_reference<Container&&>::value,
    std::decay_t<Container>,
    const Container&
  >;

  using ContainerValueType = decltype(
    *std::begin(
      std::declval<const Container>()
    )
  );

  using PairType = std::pair<
    ContainerValueType,
    ContainerValueType
  >;

  using ContainerIteratorType = decltype(
    std::begin(std::declval<const Container>())
  );
//!@}

//!@name Public members
//!@{
  BoundContainer container;
//!@}

//!@name Special member functions
//!@{
  explicit SingleContainerPairsGenerator(Container&& passContainer)
    : container(std::forward<Container>(passContainer)) {}
//!@}

//!@name Information
//!@{
  std::enable_if_t<
    traits::hasSize<Container>::value,
    std::size_t
  > size() const {
    const std::size_t N  = container.size();
    return N * (N - 1) / 2;
  }
//!@}

//!@name Iterators
//!@{
  template<class ContainerIterator>
  class iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = PairType;
    using difference_type = int;
    using pointer = const PairType*;
    using reference = const PairType&;

    iterator() = default;
    iterator(ContainerIterator left, ContainerIterator right, ContainerIterator end)
      : _left(std::move(left)),
        _right(std::move(right)),
        _end(std::move(end))
    {}

    // Prefix increment
    iterator& operator ++ () {
      ++_right;
      if(_right == _end) {
        ++_left;
        _right = _left;
        ++_right;
      }

      return *this;
    }

    // Postfix increment
    iterator operator ++ (int) {
      iterator prior = *this;
      ++(*this);
      return prior;
    }

    bool operator == (const iterator& other) const {
      return _left == other._left && _right == other._right;
    }

    bool operator != (const iterator& other) const {
      return !(*this == other);
    }

    PairType operator * () const {
      return {
        *_left,
        *_right
      };
    }

  private:
    ContainerIterator _left, _right, _end;
  };

  iterator<ContainerIteratorType> begin() const {
    auto maybeNextToBegin = std::begin(container);
    if(maybeNextToBegin != std::end(container)) {
      ++maybeNextToBegin;
    }

    return {
      std::begin(container),
      std::move(maybeNextToBegin),
      std::end(container)
    };
  }

  iterator<ContainerIteratorType> end() const {
    auto maybePriorToEnd = std::end(container);
    if(maybePriorToEnd != std::begin(container)) {
      --maybePriorToEnd;
    }

    return {
      std::move(maybePriorToEnd),
      std::end(container),
      std::end(container)
    };
  }
//!@}
};


template<class ContainerT, class ContainerU>
struct TwoContainersAllPairsGenerator {
//!@name Types
//!@{
  using BoundContainerT = std::conditional_t<
    std::is_rvalue_reference<ContainerT&&>::value,
    std::decay_t<ContainerT>,
    const ContainerT&
  >;

  using BoundContainerU = std::conditional_t<
    std::is_rvalue_reference<ContainerU&&>::value,
    std::decay_t<ContainerU>,
    const ContainerU&
  >;

  using T = decltype(
    *std::begin(
      std::declval<ContainerT>()
    )
  );

  using U = decltype(
    *std::begin(
      std::declval<ContainerU>()
    )
  );

  using PairType = std::pair<T, U>;

  using ContainerTIterator = decltype(
    std::begin(std::declval<const ContainerT>())
  );
  using ContainerUIterator = decltype(
    std::begin(std::declval<const ContainerU>())
  );
//!@}

//!@name Public members
//!@{
  BoundContainerT containerT;
  BoundContainerU containerU;
//!@}

//!@name Special member functions
//!@{
  TwoContainersAllPairsGenerator(
    ContainerT&& t,
    ContainerU&& u
  ) : containerT(std::forward<ContainerT>(t)),
      containerU(std::forward<ContainerU>(u))
  {}
//!@}

//!@name Information
//!@{
  std::enable_if_t<
    (
      traits::hasSize<ContainerT>::value
      && traits::hasSize<ContainerU>::value
    ),
    std::size_t
  > size() const {
    return containerT.size() * containerU.size();
  }
//!@}

//!@name Iterators
//!@{
  template<class TIterator, class UIterator>
  class iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = PairType;
    using difference_type = int;
    using pointer = const PairType*;
    using reference = const PairType&;

    iterator() = default;
    iterator(
      TIterator tBegin,
      TIterator tEnd,
      UIterator uBegin,
      UIterator uEnd
    ) : _tIter(std::move(tBegin)),
        _tEnd(std::move(tEnd)),
        _uBegin(std::move(uBegin)),
        _uIter(_uBegin),
        _uEnd(std::move(uEnd))
    {}

    iterator& operator ++ () {
      ++_uIter;
      if(_uIter == _uEnd) {
        ++_tIter;
        _uIter = _uBegin;
      }

      return *this;
    }

    iterator operator ++ (int) {
      iterator prior = *this;
      ++(*this);
      return prior;
    }

    bool operator == (const iterator& other) const {
      return (
        std::tie(_tIter, _tEnd, _uIter, _uEnd)
        == std::tie(other._tIter, other._tEnd, other._uIter, other._uEnd)
      );
    }

    bool operator != (const iterator& other) const {
      return !(*this == other);
    }

    PairType operator * () const {
      return {
        *_tIter,
        *_uIter
      };
    }

  private:
    TIterator _tIter, _tEnd;
    UIterator _uBegin, _uIter, _uEnd;
  };

  iterator<ContainerTIterator, ContainerUIterator> begin() const {
    return {
      std::begin(containerT),
      std::end(containerT),
      std::begin(containerU),
      std::end(containerU)
    };
  }

  iterator<ContainerTIterator, ContainerUIterator> end() const {
    return {
      std::end(containerT),
      std::end(containerT),
      std::begin(containerU),
      std::end(containerU)
    };
  }
//!@}
};

} // namespace detail

template<class Container>
auto allPairs(Container&& container) {
  return detail::SingleContainerPairsGenerator<Container>(
    std::forward<Container>(container)
  );
}

template<class ContainerT, class ContainerU>
auto allPairs(ContainerT&& t, ContainerU&& u) {
  return detail::TwoContainersAllPairsGenerator<ContainerT, ContainerU>(
    std::forward<ContainerT>(t),
    std::forward<ContainerU>(u)
  );
}

} // namespace adaptors

} // namespace temple
#endif