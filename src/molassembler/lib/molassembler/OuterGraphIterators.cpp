/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 */

#include "molassembler/OuterGraph.h"

#include "boost/iterator/iterator_concepts.hpp"

#include "molassembler/Graph/Bridge.h"

/* The implementation of the bridging iterators proceeds in the following steps
 * to reduce the seemingly neverending boilerplate of wrapping other iterators
 *
 * - Write the outer implementation of InnerBasedIterator that just forwards to
 *   its private implementation
 * - Make sure that facade passes the necessary boost iterator concepts
 * - Since the implementation of the Impl struct for each template variation of
 *   InnerBasedIterator share some code, summarize the commonalities in
 *   BaseIteratorWrapper (from which we can derive to make the various Impl
 *   impls)
 * - Define the Impl structs for each InnerBasedIterator in terms of template
 *   specializations of BaseIteratorWrapper and a little bit of specific code
 * - Add actual instantiations of the InnerBasedIterator so that the symbols
 *   are in the library when somebody uses their interfaces from the header
 *   without a definition (and its enabled double-templated constructors too)
 *
 * Frankly, I'm still a little surprised it's not NDR ill-formed as it seems a
 * bit like magic to me. But as long as the symbols of the explicit
 * instantiations of InnerBasedIterator are there for consumers using the
 * templated declaration in the header and everything those iterators need is
 * defined at the point of instantiation here, it works like a charm.
 *
 * It's disallowed to instantiate InnerBasedIterator for any other Ts than the
 * ones for which we explicitly instantiate the definitions here (by
 * static_assert), so I don't think anyone can abuse the public nature of
 * InnerBasedIterator in any fashion to cause an NDR ill-formed program.
 */

namespace Scine {
namespace molassembler {

/* Implementation for iterator facade */
template<typename T, bool isVertexInitialized>
OuterGraph::InnerBasedIterator<T, isVertexInitialized>::InnerBasedIterator(
  InnerBasedIterator&& other
) noexcept = default;

template<typename T, bool isVertexInitialized>
OuterGraph::InnerBasedIterator<T, isVertexInitialized>&
OuterGraph::InnerBasedIterator<T, isVertexInitialized>::operator = (InnerBasedIterator&& other) noexcept = default;

template<typename T, bool isVertexInitialized>
OuterGraph::InnerBasedIterator<T, isVertexInitialized>::InnerBasedIterator(
  const InnerBasedIterator& other
) : _pImpl (
  std::make_unique<Impl>(*other._pImpl)
) { }

template<typename T, bool isVertexInitialized>
OuterGraph::InnerBasedIterator<T, isVertexInitialized>&
OuterGraph::InnerBasedIterator<T, isVertexInitialized>::operator = (const InnerBasedIterator& other) {
  *_pImpl = *other._pImpl;
  return *this;
}

template<typename T, bool isVertexInitialized>
OuterGraph::InnerBasedIterator<T, isVertexInitialized>::~InnerBasedIterator() = default;

template<typename T, bool isVertexInitialized>
OuterGraph::InnerBasedIterator<T, isVertexInitialized>::InnerBasedIterator() = default;

template<typename T, bool isVertexInitialized>
template<bool Dependent, std::enable_if_t<!Dependent, int>...>
OuterGraph::InnerBasedIterator<T, isVertexInitialized>::InnerBasedIterator(
  const InnerGraph& inner,
  bool begin
) : _pImpl (
  std::make_unique<Impl>(inner, begin)
) {}

template<typename T, bool isVertexInitialized>
template<bool Dependent, std::enable_if_t<Dependent, int>...>
OuterGraph::InnerBasedIterator<T, isVertexInitialized>::InnerBasedIterator(
  const AtomIndex a,
  const InnerGraph& inner,
  bool begin
) : _pImpl (
  std::make_unique<Impl>(a, inner, begin)
) {}

template<typename T, bool isVertexInitialized>
OuterGraph::InnerBasedIterator<T, isVertexInitialized>& OuterGraph::InnerBasedIterator<T, isVertexInitialized>::operator ++ () {
  ++(*_pImpl);
  return *this;
}

template<typename T, bool isVertexInitialized>
OuterGraph::InnerBasedIterator<T, isVertexInitialized> OuterGraph::InnerBasedIterator<T, isVertexInitialized>::operator ++ (int) {
  auto copy = *this;
  ++(*_pImpl);
  return copy;
}

template<typename T, bool isVertexInitialized>
typename OuterGraph::InnerBasedIterator<T, isVertexInitialized>::value_type OuterGraph::InnerBasedIterator<T, isVertexInitialized>::operator * () const {
  return *(*_pImpl);
}

template<typename T, bool isVertexInitialized>
bool OuterGraph::InnerBasedIterator<T, isVertexInitialized>::operator == (const InnerBasedIterator<T, isVertexInitialized>& other) const {
  return *_pImpl == *other._pImpl;
}

template<typename T, bool isVertexInitialized>
bool OuterGraph::InnerBasedIterator<T, isVertexInitialized>::operator != (const InnerBasedIterator<T, isVertexInitialized>& other) const {
  return !(*_pImpl == *other._pImpl);
}

/* Concept tests */
BOOST_CONCEPT_ASSERT((boost_concepts::ReadableIteratorConcept<OuterGraph::AtomIterator>));
BOOST_CONCEPT_ASSERT((boost_concepts::SinglePassIterator<OuterGraph::AtomIterator>));

BOOST_CONCEPT_ASSERT((boost_concepts::ReadableIteratorConcept<OuterGraph::BondIterator>));
BOOST_CONCEPT_ASSERT((boost_concepts::SinglePassIterator<OuterGraph::BondIterator>));

BOOST_CONCEPT_ASSERT((boost_concepts::ReadableIteratorConcept<OuterGraph::AdjacencyIterator>));
BOOST_CONCEPT_ASSERT((boost_concepts::SinglePassIterator<OuterGraph::AdjacencyIterator>));

BOOST_CONCEPT_ASSERT((boost_concepts::ReadableIteratorConcept<OuterGraph::IncidentEdgesIterator>));
BOOST_CONCEPT_ASSERT((boost_concepts::SinglePassIterator<OuterGraph::IncidentEdgesIterator>));

/* Impl base class for quicker implementation of each iterator Impl */
template<typename Iterator>
struct BaseIteratorWrapper {
  Iterator iterator;

  BaseIteratorWrapper() = default;
  explicit BaseIteratorWrapper(Iterator passIterator) : iterator(std::move(passIterator)) {}
  virtual ~BaseIteratorWrapper() = default;

  BaseIteratorWrapper& operator ++ () {
    ++iterator;
    return *this;
  }

  /* BaseIteratorWrapper operator ++ (int) is not needed since pImpled iterator
   * wrappers do not call it
   */

  bool operator == (const BaseIteratorWrapper& other) const {
    return iterator == other.iterator;
  }
};


/* Impl definitions and missing functions */
template<>
struct OuterGraph::AtomIterator::Impl
  : public BaseIteratorWrapper<InnerGraph::BglType::vertex_iterator>
{
  Impl(const InnerGraph& inner, bool begin) {
    auto vertexIterators = inner.vertices();
    if(begin) {
      iterator = std::move(vertexIterators.first);
    } else {
      iterator = std::move(vertexIterators.second);
    }
  }

  AtomIndex operator * () const {
    // OuterGraph::AtomIndex and InnerGraph::Vertex are the same type
    return *iterator;
  }
};

template<>
struct OuterGraph::BondIterator::Impl
  : public BaseIteratorWrapper<InnerGraph::BglType::edge_iterator>
{
  const InnerGraph* innerPtr;

  Impl(const InnerGraph& inner, bool begin) : innerPtr(&inner) {
    auto edgeIterators = inner.edges();
    if(begin) {
      iterator = std::move(edgeIterators.first);
    } else {
      iterator = std::move(edgeIterators.second);
    }
  }

  BondIndex operator * () const {
    return toOuter(*iterator, *innerPtr);
  }
};

template<>
struct OuterGraph::AdjacencyIterator::Impl
  : public BaseIteratorWrapper<InnerGraph::BglType::adjacency_iterator>
{
  Impl(
    const AtomIndex a,
    const InnerGraph& inner,
    bool begin
  ) {
    auto adjacencyIterators = inner.adjacents(a);
    if(begin) {
      iterator = std::move(adjacencyIterators.first);
    } else {
      iterator = std::move(adjacencyIterators.second);
    }
  }

  AtomIndex operator * () const {
    return *iterator;
  }
};

template<>
struct OuterGraph::IncidentEdgesIterator::Impl
  : public BaseIteratorWrapper<InnerGraph::BglType::out_edge_iterator>
{
  const InnerGraph* innerPtr;

  Impl(const AtomIndex a, const InnerGraph& inner, bool begin) : innerPtr(&inner) {
    auto edgeIterators = inner.edges(a);
    if(begin) {
      iterator = std::move(edgeIterators.first);
    } else {
      iterator = std::move(edgeIterators.second);
    }
  }

  BondIndex operator * () const {
    return toOuter(*iterator, *innerPtr);
  }
};

/* Template specializations for all variants (This adds the symbols for all
 * the needed template argument combinations into the library)
 *
 * This can only be done once all of the specific Impls have been defined since
 * the operators instantiated explicitly here need Impl to be a complete type.
 */
template class OuterGraph::InnerBasedIterator<AtomIndex, false>;
template class OuterGraph::InnerBasedIterator<BondIndex, false>;
template class OuterGraph::InnerBasedIterator<AtomIndex, true>;
template class OuterGraph::InnerBasedIterator<BondIndex, true>;

/* Explicitly instantiate the template class template constructor cases (which
 * are "double-templated" and hence not instantiated by the previous statement)
 *
 * These require only the function signature, not parameter names.
 */
template OuterGraph::InnerBasedIterator<AtomIndex, false>::InnerBasedIterator(
  const InnerGraph&,
  bool
);
template OuterGraph::InnerBasedIterator<BondIndex, false>::InnerBasedIterator(
  const InnerGraph&,
  bool
);
template OuterGraph::InnerBasedIterator<AtomIndex, true>::InnerBasedIterator(
  AtomIndex,
  const InnerGraph&, bool
);
template OuterGraph::InnerBasedIterator<BondIndex, true>::InnerBasedIterator(
  AtomIndex,
  const InnerGraph&, bool
);

} // namespace molassembler
} // namespace Scine
