#define BOOST_TEST_MODULE templeTests
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "temple/Containers.h"
#include "temple/Random.h"

#include "temple/constexpr/Math.h"
#include "temple/constexpr/Containers.h"
#include "temple/constexpr/Array.h"
#include "temple/constexpr/DynamicArray.h"
#include "temple/constexpr/DynamicSet.h"
#include "temple/constexpr/DynamicMap.h"
#include "temple/constexpr/TupleType.h"
#include "temple/constexpr/LogicalOperatorTests.h"
#include "temple/constexpr/FloatingPointComparison.h"
#include "temple/constexpr/UIntArray.h"
#include "temple/constexpr/DynamicUIntArray.h"
#include "temple/constexpr/BTree.h"
#include "temple/constexpr/ConsecutiveCompare.h"

#include <iostream>
#include <iomanip>

#include <boost/test/results_collector.hpp>

inline bool lastTestPassed() {
  using namespace boost::unit_test;
  test_case::id_t id = framework::current_test_case().p_id;
  test_results rez = results_collector.results(id);
  return rez.passed();
}

using namespace std::string_literals;

namespace ArrayTests {

constexpr auto testArr = temple::Array<unsigned, 3> {4, 3, 5};

template<typename T, size_t size>
constexpr temple::Array<T, size> modifyArray(
  const temple::Array<T, size>& array
) {
  auto arrayCopy = array;
  inPlaceSwap(arrayCopy, 0, 1);
  return arrayCopy;
}
  
constexpr auto modf = modifyArray(testArr);

static_assert(
  modf == temple::Array<unsigned, 3> {3, 4, 5},
  "Swap doesn't work as expected"
);

static_assert(
  temple::arrayPop(testArr) == temple::Array<unsigned, 2> {4, 3},
  "Pop doesn't work"
);

static_assert(
  temple::arrayPush(testArr, 9u) == temple::Array<unsigned, 4> {4, 3, 5, 9},
  "Push doesn't work"
);

static_assert(
  temple::arrayPush(testArr, 9u) == temple::Array<unsigned, 4> {4, 3, 5, 9},
  "arrayPush doesn't work on temple::Array"
);

constexpr auto stdTestArr = std::array<unsigned, 3> {{4, 3, 5}};

static_assert(
  temple::arraysEqual(
    temple::arrayPush(stdTestArr, 9u),
    std::array<unsigned, 4> {{4, 3, 5, 9}}
  ),
  "arrayPush doesn't work on std::array"
);

template<size_t size>
constexpr void testIteration(const temple::Array<unsigned, size>& array) {
  for(const auto& element : array) {
    std::cout << element << std::endl;
  }
}

constexpr auto sortedArr = temple::Array<unsigned, 4> {4, 6, 9, 11};
constexpr auto oneMore = temple::insertIntoSorted(sortedArr, 5u);

static_assert(
  oneMore == temple::Array<unsigned, 5> {4, 5, 6, 9, 11},
  "InsertIntoSorted does not work as expected."
);

static_assert(temple::Math::factorial(5) == 120, "Factorial is incorrect");
static_assert(temple::Math::factorial(0) == 1, "Factorial is incorrect");

// C++17 with std::array
/*
constexpr auto stdSortedArr = std::array<unsigned, 4> {{4, 6, 9, 11}};
constexpr auto stdOneMore = temple::insertIntoSorted(stdSortedArr, 5u);

static_assert(
  temple::arraysEqual(stdOneMore, std::array<unsigned, 5> {4, 5, 6, 9, 11}),
  "InsertIntoSorted does not work as expected with std::array"
);
// std::array::operator == (const std::array& other) isn't constexpr in C++17
*/

static_assert(
  std::is_same<
    decltype(temple::makeArray(4, 3, 9)),
    temple::Array<int, 3>
  >::value,
  "makeArray does not work as expected"
);

} // namespace ArrayTests

BOOST_AUTO_TEST_CASE( mathApproxEqual ) {
  const unsigned numTests = 100;

  constexpr double accuracy = 1e-12;

  static_assert(
    accuracy >= std::numeric_limits<double>::epsilon(),
    "Testing accuracy must be greater than machine epsilon!"
  );

  // sqrt
  const auto randomPositiveNumbers = temple::random.getN<double>(0, 1e6, numTests);
  auto sqrt_passes = temple::map(
    randomPositiveNumbers,
    [&](const double& randomPositiveNumber) -> bool {
      return temple::floating::isCloseRelative(
        temple::Math::sqrt(randomPositiveNumber),
        std::sqrt(randomPositiveNumber),
        accuracy
      );
    }
  );

  bool all_sqrt_pass = temple::all_of(sqrt_passes);

  BOOST_CHECK(all_sqrt_pass);

  if(!all_sqrt_pass) {
    std::cout << "Square-root implementation is lacking! Failures: " << std::endl;

    for(unsigned i = 0; i < sqrt_passes.size(); i++) {
      if(!sqrt_passes[i]) {
        std::cout << "  x = " << std::setw(12) << randomPositiveNumbers[i]
          << ", sqrt = " << std::setw(12) << temple::Math::sqrt(randomPositiveNumbers[i])
          << ", std::sqrt = " << std::setw(12) << std::sqrt(randomPositiveNumbers[i])
          << ", |Δ| = " << std::setw(12) << std::fabs(
            temple::Math::sqrt(randomPositiveNumbers[i])
            - std::sqrt(randomPositiveNumbers[i])
          ) << std::endl;
      }
    }

    std::cout << std::endl;
  }

  // asin
  const auto randomInverseTrigNumbers = temple::random.getN<double>(
    -1 + std::numeric_limits<double>::epsilon(),
    1 - std::numeric_limits<double>::epsilon(),
    numTests
  );

  auto asin_passes = temple::map(
    randomInverseTrigNumbers,
    [&](const double& randomInverseTrigNumber) -> bool {
      return temple::floating::isCloseRelative(
        temple::Math::asin(randomInverseTrigNumber),
        std::asin(randomInverseTrigNumber),
        1e-8
      );
    }
  );

  bool all_asin_pass = temple::all_of(asin_passes);

  BOOST_CHECK(all_asin_pass);

  if(!all_asin_pass) {
    std::cout << "Inverse sine implementation is lacking! Failures: " << std::endl;

    for(unsigned i = 0; i < asin_passes.size(); i++) {
      if(!asin_passes[i]) {
        std::cout << "  x = " << std::setw(12) << randomInverseTrigNumbers[i]
          << ", asin = " << std::setw(12) << temple::Math::asin(randomInverseTrigNumbers[i])
          << ", std::asin = " << std::setw(12) << std::asin(randomInverseTrigNumbers[i])
          << ", |Δ| = " << std::setw(12) << std::fabs(
            temple::Math::asin(randomInverseTrigNumbers[i])
            - std::asin(randomInverseTrigNumbers[i])
          ) << std::endl;
      }
    }

    std::cout << std::endl;
  }

  auto testPow = [&](const double& number, const int& exponent) -> bool {
    const double test = temple::Math::pow(number, exponent);
    const double reference = std::pow(number, exponent);

    bool passes = temple::floating::isCloseRelative(
      test,
      reference,
      accuracy
    );

    if(!passes) {
      std::cout << "  x = " << std::setw(12) << number
        << ", exp = " << std::setw(4) << exponent
        << ", pow = " << std::setw(12) << test
        << ", std::pow = " << std::setw(12) << reference
        << ", |Δ| = " << std::setw(12) << std::fabs(test - reference) << ", max permissible diff: "
        << (
          accuracy * std::max(
            std::fabs(test),
            std::fabs(reference)
          )
        ) << std::endl;
    }

    return passes;
  };

  BOOST_CHECK(
    temple::all_of(
      temple::zipMap(
        temple::random.getN<double>(-1e5, 1e5, numTests),
        temple::random.getN<int>(-40, 40, numTests),
        testPow
      )
    )
  );
  
  auto testRecPow = [&](const double& number, const unsigned& exponent) -> bool {
    const double test = temple::Math::recPow(number, exponent);
    const double reference = std::pow(number, exponent);

    bool passes = temple::floating::isCloseRelative(
      test,
      reference,
      accuracy
    );

    if(!passes) {
      std::cout << "  x = " << std::setw(12) << number
        << ", exp = " << std::setw(4) << exponent
        << ", recPow = " << std::setw(12) << test
        << ", std::pow = " << std::setw(12) << reference
        << ", |Δ| = " << std::setw(12) << std::fabs(test - reference) << ", max permissible diff: "
        << (
          accuracy * std::max(
            std::fabs(test),
            std::fabs(reference)
          )
        ) << std::endl;
    }

    return passes;
  };

  BOOST_CHECK(
    temple::all_of(
      temple::zipMap(
        temple::random.getN<double>(-1e5, 1e5, numTests),
        temple::random.getN<unsigned>(0, 40, numTests),
        testRecPow
      )
    )
  );


  // ln
  const auto randomZ = temple::random.getN<double>(1e-10, 1e10, numTests);
  bool all_ln_pass = temple::all_of(
    randomZ,
    [&](const auto& z) -> bool {
      bool pass = temple::floating::isCloseRelative(
        temple::Math::ln(z),
        std::log(z),
        accuracy
      );

      if(!pass) {
        std::cout << "ln deviates for z = " << std::setw(12) << z 
          << ", ln(z) = " << std::setw(12) << temple::Math::ln(z) 
          << ", std::log(z) = " << std::setw(12) << std::log(z)
          << ", |Δ| = " << std::setw(12) << std::fabs(
            temple::Math::ln(z) - std::log(z)
          ) << std::endl;
      }

      return pass;
    }
  );

  BOOST_CHECK(all_ln_pass);

  BOOST_CHECK(
    temple::all_of(
      temple::random.getN<double>(-100, 100, numTests),
      [](const double& x) -> bool {
        return(temple::Math::floor(x) <= x);
      }
    )
  );

  BOOST_CHECK(
    temple::all_of(
      temple::random.getN<double>(-100, 100, numTests),
      [](const double& x) -> bool {
        return(temple::Math::ceil(x) >= x);
      }
    )
  );

  BOOST_CHECK(
    temple::all_of(
      temple::random.getN<double>(-100, 100, numTests),
      [](const double& x) -> bool {
        const double rounded = temple::Math::round(x);
        return(
          rounded == temple::Math::floor(x)
          || rounded == temple::Math::ceil(x)
        );
      }
    )
  );

  BOOST_CHECK(
    temple::all_of(
      temple::random.getN<double>(-M_PI / 2, M_PI / 2, numTests),
      [&](const double& x) -> bool {
        return temple::floating::isCloseRelative(
          temple::Math::atan(x),
          std::atan(x),
          accuracy
        );
      }
    )
  );
}

BOOST_AUTO_TEST_CASE(arrayPermutation) {
  std::array<unsigned, 4> base {{0, 1, 2, 3}};
  std::array<unsigned, 4> STLComparison {{0, 1, 2, 3}};

  bool customHasNext = true;
  bool STLHasNext = true;

  do {
    customHasNext = temple::inPlaceNextPermutation(base);
    STLHasNext = std::next_permutation(STLComparison.begin(), STLComparison.end());

    BOOST_CHECK_MESSAGE(
      base == STLComparison,
      "In forward permutation, base is {" << temple::condenseIterable(base)
      << "} and and STL is {" << temple::condenseIterable(STLComparison)
      << "}"
    );
  } while(customHasNext && STLHasNext);

  BOOST_CHECK_MESSAGE(
   !customHasNext && !STLHasNext,
   "The two permutation algorithms don't terminate at the same time"
  );

  base = {{3, 2, 1, 0}};
  STLComparison = {{3, 2, 1, 0}};
  customHasNext = true;
  STLHasNext = true;

  do {
    customHasNext = temple::inPlacePreviousPermutation(base);
    STLHasNext = std::prev_permutation(STLComparison.begin(), STLComparison.end());

    BOOST_CHECK_MESSAGE(
      base == STLComparison,
      "In backward permutation, base is {" << temple::condenseIterable(base)
      << "} and and STL is {" << temple::condenseIterable(STLComparison)
      << "}"
    );
  } while(customHasNext && STLHasNext);

  BOOST_CHECK_MESSAGE(
   !customHasNext && !STLHasNext,
   "The two permutation algorithms don't terminate at the same time"
  );

  // Variants of limited sections of the array

  base = {{0, 1, 2, 3}};
  STLComparison = {{0, 1, 2, 3}};
  customHasNext = true;
  STLHasNext = true;

  do {
    customHasNext = temple::inPlaceNextPermutation(base, 1, 3);
    STLHasNext = std::next_permutation(STLComparison.begin() + 1, STLComparison.end() - 1);

    BOOST_CHECK_MESSAGE(
      base == STLComparison,
      "In limited forward permutation, base is {" << temple::condenseIterable(base)
      << "} and and STL is {" << temple::condenseIterable(STLComparison)
      << "}"
    );
  } while(customHasNext && STLHasNext);

  BOOST_CHECK_MESSAGE(
   !customHasNext && !STLHasNext,
   "The two permutation algorithms don't terminate at the same time in limited "
   "forward permutation"
  );

  base = {{3, 2, 1, 0}};
  STLComparison = {{3, 2, 1, 0}};
  customHasNext = true;
  STLHasNext = true;

  do {
    customHasNext = temple::inPlacePreviousPermutation(base, 1, 3);
    STLHasNext = std::prev_permutation(STLComparison.begin() + 1, STLComparison.end() - 1);

    BOOST_CHECK_MESSAGE(
      base == STLComparison,
      "In limited backward permutation, base is {" << temple::condenseIterable(base)
      << "} and and STL is {" << temple::condenseIterable(STLComparison)
      << "}"
    );
  } while(customHasNext && STLHasNext);

  BOOST_CHECK_MESSAGE(
   !customHasNext && !STLHasNext,
   "The two permutation algorithms don't terminate at the same time in limited "
   "backward permutation"
  );
}

constexpr bool compileTimeDynTest() {
  temple::DynamicArray<unsigned, 10> nonConstArr {4, 3, 6};
  nonConstArr.push_back(9);
  return nonConstArr.size() == 4;
}

constexpr bool dynArrSpliceTest() {
  temple::DynamicArray<unsigned, 10> nonConstArr {4, 3, 6, 5, 1, 9};
  auto spliced = nonConstArr.splice(2);
  
  return (
    spliced == temple::DynamicArray<unsigned, 10> {6, 5, 1, 9}
    && nonConstArr == temple::DynamicArray<unsigned, 10> {4, 3}
  );
}

BOOST_AUTO_TEST_CASE(dynamicArrayTests) {
  constexpr temple::DynamicArray<unsigned, 10> arr {4, 3, 5};

  static_assert(
    arr.size() == 3,
    "Array size isn't initialized correctly from parameter pack ctor"
  );
  static_assert(
    compileTimeDynTest(),
    "non-const dynamic array push_back does not work as expected"
  );
  static_assert(
    dynArrSpliceTest(),
    "non-const dynamic array splice does not work as expected"
  );
  static_assert(
    arr.end() - arr.begin() == 3,
    "Subtracting begin/end iterators does not yield dynamic length"
  );

  static_assert(
    arr.begin() - arr.end() == -3,
    "Subtracting begin/end iterators does not yield dynamic length"
  );

  constexpr temple::Array<unsigned, 10> values {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};

  constexpr auto grouped = groupByEquality(
    values,
    std::equal_to<unsigned>()
  );

  static_assert(
    grouped.size() == 4
    && grouped.at(0).size() == 1
    && grouped.at(1).size() == 2
    && grouped.at(2).size() == 3
    && grouped.at(3).size() == 4,
    "Grouping does not work as expected"
  );

  constexpr temple::DynamicArray<unsigned, 14> fromFixed {values};

  static_assert(fromFixed.size() == 10, "Construction from fixed doesn't work");
}

template<typename T, size_t size, class Comparator>
constexpr bool isSorted(const temple::DynamicSet<T, size, Comparator>& set) {
  Comparator comparator;

  auto left = set.begin();
  auto right = set.begin(); ++right;

  while(right != set.end()) {
    if(comparator(*right, *left)) {
      return false;
    }

    ++right;
    ++left;
  }

  return true;
}

BOOST_AUTO_TEST_CASE(dynamicSetTests) {
  temple::DynamicSet<unsigned, 10> set;

  BOOST_CHECK(set.size() == 0);
  BOOST_CHECK(
    std::distance(
      set.begin(),
      set.end()
    ) == 0u
  );

  for(const auto& item : {9u, 3u, 5u}) {
    set.insert(item);
  }

  BOOST_CHECK(set.size() == 3);
  BOOST_CHECK(
    std::distance(
      set.begin(),
      set.end()
    ) == 3u
  );
  BOOST_CHECK(set.contains(3) && set.contains(5) && set.contains(9));
  for(const auto& item : {2u, 4u, 8u, 10u}) {
    BOOST_CHECK_MESSAGE(
      !set.contains(item),
      "Set says it contains " << item << " when it shouldn't (set is {"
        << temple::condenseIterable(set)
        << "}."
    );
  }
  BOOST_CHECK(isSorted(set));

  temple::DynamicSet<unsigned, 10> setInitList {
    temple::DynamicArray<unsigned, 10> {
      4u, 9u, 13u
    }
  };

  BOOST_CHECK(setInitList.size() == 3);
  BOOST_CHECK(
    std::distance(
      setInitList.begin(),
      setInitList.end()
    ) == 3u
  );

  setInitList.insert(0u);

  BOOST_CHECK(setInitList.size() == 4);

  BOOST_CHECK_MESSAGE(
    setInitList.contains(4)
    && setInitList.contains(9)
    && setInitList.contains(13)
    && setInitList.contains(0)
    && !setInitList.contains(1)
    && !setInitList.contains(25),
    "setInitList {"
      << temple::condenseIterable(setInitList)
      << "} does not conform to expectations concerning contains:\n"
      << std::boolalpha
      << "contains 4, expect true:" << setInitList.contains(4)
      << "\ncontains 9, expect true:" << setInitList.contains(9)
      << "\ncontains 13, expect true:" << setInitList.contains(13)
      << "\ncontains 0, expect true:" << setInitList.contains(0)
      << "\ncontains 1, expect false:" << setInitList.contains(1)
      << "\ncontains 25, expect false:" << setInitList.contains(25)
  );
  // Set of arrays
  temple::Array<
    temple::Array<unsigned, 4>,
    5
  > sampleArrays {
    temple::Array<unsigned, 4> {1u, 2u, 3u, 4u},
    temple::Array<unsigned, 4> {1u, 2u, 4u, 3u},
    temple::Array<unsigned, 4> {1u, 4u, 3u, 2u},
    temple::Array<unsigned, 4> {1u, 4u, 2u, 3u},
    temple::Array<unsigned, 4> {2u, 1u, 3u, 4u}
  };

  temple::DynamicSet<
    temple::Array<unsigned, 4>,
    10
  > arraysSet;

  arraysSet.insert(sampleArrays.at(0));
  arraysSet.insert(sampleArrays.at(2));
  arraysSet.insert(sampleArrays.at(3));

  BOOST_CHECK(arraysSet.contains(sampleArrays.at(0)));
  BOOST_CHECK(arraysSet.contains(sampleArrays.at(2)));
  BOOST_CHECK(arraysSet.contains(sampleArrays.at(3)));
  BOOST_CHECK(!arraysSet.contains(sampleArrays.at(1)));
  BOOST_CHECK(!arraysSet.contains(sampleArrays.at(4)));

  BOOST_CHECK(arraysSet.size() == 3);
  BOOST_CHECK(
    std::distance(
      arraysSet.begin(), 
      arraysSet.end()
    ) == 3
  );
}

template<typename T, size_t size>
bool validate(const temple::DynamicSet<T, size>& set) {
  // Is the set ordered?
  auto leftIter = set.begin();
  auto rightIter = leftIter; ++rightIter; 
  auto bound = set.end(); --bound;

  if(leftIter == set.end() || rightIter == set.end()) {
    return true;
  }

  while(rightIter != bound) {
    if(*leftIter >= *rightIter) {
      std::cout << "*left >= *right -> " << *leftIter << " >= " << *rightIter << std::endl;
      return false;
    }

    ++leftIter;
    ++rightIter;
  }

  // Is the reported size equal to a begin-end through-iteration?
  return (
    set.size() == static_cast<unsigned>(
      std::distance(
        set.begin(),
        set.end()
      )
    )
  );
}

BOOST_AUTO_TEST_CASE(arrayOperators) {
  temple::Array<unsigned, 4> a {4, 2, 3, 1};
  temple::Array<unsigned, 4> b {4, 3, 2, 1};
  
  BOOST_CHECK(temple::testLogicalOperators(a, b));
  BOOST_CHECK(temple::testLogicalOperators(a, a));

  temple::dynamic::explainLogicalOperatorFailures(a, b);
}

BOOST_AUTO_TEST_CASE(dynamicSetFuzzing) {
  for(unsigned N = 0; N < 100; ++N) {
    temple::DynamicSet<unsigned, 100> subject;

    std::vector<unsigned> numbers;
    numbers.resize(50);
    std::iota(
      numbers.begin(),
      numbers.end(),
      0
    );
    std::shuffle(
      numbers.begin(),
      numbers.end(),
      temple::random.randomEngine
    );

    for(const auto& number : numbers) {
      subject.insert(number);

      bool isValid = validate(subject);
      if(!isValid) {
        std::cout << "After inserting " << number 
          << ", set is left in invalid state. Set: {"
          << temple::condenseIterable(subject) << "}\ninsert sequence {"
          << temple::condenseIterable(numbers) << "}."
          << std::endl;
      }

      BOOST_REQUIRE(isValid);

      BOOST_CHECK(subject.contains(number));
    }
  }
}

namespace TupleTypeTests {

struct Apple {
  static constexpr unsigned number = 4;
};

struct Banana {
  static constexpr unsigned number = 3;
};

struct Cherry {
  static constexpr unsigned number = 50;
};

using Fruit = std::tuple<Apple, Banana, Cherry>;

template<typename ... FruitClasses>
struct sumNumbersValue {
  static constexpr unsigned initializer() {
    const std::array<unsigned, sizeof...(FruitClasses)> sizes = {{
      FruitClasses::number...
    }};

    unsigned sum = 0;

    for(unsigned i = 0; i < sizes.size(); ++i) {
      sum += sizes[i];
    }

    return sum;
  };

  static constexpr unsigned value = initializer();
};

template<typename ... FruitClasses>
struct sumNumbersFunctor {
  static constexpr unsigned value() {
    const std::array<unsigned, sizeof...(FruitClasses)> sizes = {{
      FruitClasses::number...
    }};

    unsigned sum = 0;

    for(unsigned i = 0; i < sizes.size(); ++i) {
      sum += sizes[i];
    }

    return sum;
  };
};

template<typename FruitClass>
struct getNumberFunctor {
  static constexpr unsigned value() {
    return FruitClass::number;
  }
};

template<typename FruitClass>
struct getNumberValue {
  static constexpr unsigned initialize() {
    return FruitClass::number;
  }

  static constexpr unsigned value = initialize();
};

template<typename FruitClass>
constexpr unsigned getNumberFunction() {
  return FruitClass::number;
}

template<typename A, typename B>
struct pairSumFunctor {
  static constexpr unsigned value() {
    return A::number + B::number;
  }
};

template<typename A, typename B>
struct pairSumValue {
  static constexpr unsigned initialize() {
    return A::number + B::number;
  }

  static constexpr unsigned value = initialize();
};

static_assert(
  temple::TupleType::unpackToFunction<Fruit, sumNumbersValue>() == 57,
  "Unpacking fruit tuple to valueValue does not yield expected result"
);

static_assert(
  temple::TupleType::unpackToFunction<Fruit, sumNumbersFunctor>() == 57,
  "Unpacking fruit tuple to valueFunctor does not yield expected result"
);

static_assert(
  temple::arraysEqual(
    temple::TupleType::map<Fruit, getNumberValue>(),
    temple::Array<unsigned, 3> {{
      Apple::number, Banana::number, Cherry::number 
    }}
  ),
  "Mapping with getNumberValue does not yield expected result!"
);

static_assert(
  temple::arraysEqual(
    temple::TupleType::map<Fruit, getNumberFunctor>(),
    temple::Array<unsigned, 3> {{
      Apple::number, Banana::number, Cherry::number 
    }}
  ),
  "Mapping with getNumberFunctor does not yield expected result!"
);

static_assert(
  temple::arraysEqual(
    temple::TupleType::mapAllPairs<Fruit, pairSumValue>(),
    temple::Array<unsigned, 3> {{
      Apple::number + Banana::number,
      Apple::number + Cherry::number,
      Banana::number + Cherry::number
    }}
  ),
  "Mapping pairs with pairSumValue does not yield expected result!"
);

static_assert(
  temple::arraysEqual(
    temple::TupleType::mapAllPairs<Fruit, pairSumFunctor>(),
    temple::Array<unsigned, 3> {{
      Apple::number + Banana::number,
      Apple::number + Cherry::number,
      Banana::number + Cherry::number
    }}
  ),
  "Mapping pairs with pairSumFunctor does not yield expected result!"
);

using countTestType = std::tuple<unsigned, float, double, unsigned, size_t>;

static_assert(
  temple::TupleType::countType<countTestType, unsigned>() == 2,
  "Counting unsigned in countTestType does not return two!"
);

} // namespace TupleTypeTests

namespace FloatingPointComparisonTests {

template<typename T>
constexpr bool testAbsoluteComparison(const T& a, const T& b, const T& tolerance) {
  temple::floating::ExpandedAbsoluteEqualityComparator<T> comparator {
    tolerance
  };

  return (
    temple::Math::XOR(
      (
        comparator.isLessThan(a, b)
        && comparator.isMoreThan(b, a)
        && comparator.isUnequal(a, b)
      ),
      (
        comparator.isLessThan(b, a)
        && comparator.isMoreThan(a, b)
        && comparator.isUnequal(a, b)
      ),
      (
        !comparator.isLessThan(a, b)
        && !comparator.isMoreThan(a, b)
        && comparator.isEqual(a, b)
      )
    ) && temple::Math::XOR(
      comparator.isEqual(a, b),
      comparator.isUnequal(a, b)
    )
  );
}

template<typename T>
constexpr bool testRelativeComparison(const T& a, const T& b, const T& tolerance) {
  temple::floating::ExpandedRelativeEqualityComparator<T> comparator {
    tolerance
  };

  return (
    temple::Math::XOR(
      (
        comparator.isLessThan(a, b)
        && comparator.isMoreThan(b, a)
        && comparator.isUnequal(a, b)
      ),
      (
        comparator.isLessThan(b, a)
        && comparator.isMoreThan(a, b)
        && comparator.isUnequal(a, b)
      ),
      (
        !comparator.isLessThan(a, b)
        && !comparator.isMoreThan(a, b)
        && comparator.isEqual(a, b)
      )
    ) && temple::Math::XOR(
      comparator.isEqual(a, b),
      comparator.isUnequal(a, b)
    )
  );
}

static_assert(
  testAbsoluteComparison(4.3, 3.9, 1e-4)
  && testAbsoluteComparison(4.3, 3.9, 1.0)
  && testAbsoluteComparison(4.4, 4.4, 1e-10),
  "absolute comparison has inconsistent operators!"
);

static_assert(
  testRelativeComparison(4.3, 3.9, 1e-4)
  && testRelativeComparison(4.3, 3.9, 1.0)
  && testRelativeComparison(4.4, 4.4, 1e-10),
  "relative comparison has inconsistent operators!"
);

} // namespace FloatingPointComparisonTests

namespace ConcatenationTests {

constexpr temple::Array<unsigned, 4> f {4, 2, 9, 3};
constexpr temple::Array<unsigned, 4> g {11, 22, 33, 44};
constexpr temple::Array<unsigned, 4> h {234, 292, 912, 304};
constexpr temple::Array<unsigned, 8> fg {
  4, 2, 9, 3,
  11, 22, 33, 44
};
constexpr temple::Array<unsigned, 12> fgh {
  4, 2, 9, 3,
  11, 22, 33, 44,
  234, 292, 912, 304
};

static_assert(
  temple::arraysEqual(
    temple::arrayConcatenate(f, g),
    fg
  ),
  "Pairwise concatenation does not preserve sequence!"
);

static_assert(
  temple::arraysEqual(
    temple::arrayConcatenate(f, g, h),
    fgh
  ),
  "Variadic concatenation does not work as expected"
);

} // namespace ConcatenationTests

namespace DynamicMapTests {

constexpr temple::DynamicMap<unsigned, int, 20> generateMap() {
  temple::DynamicMap<unsigned, int, 20> myMap;

  myMap.insert(4, -2);
  myMap.insert(1, 4);
  myMap.insert(3, 9);

  return myMap;
}

constexpr auto a = generateMap();

static_assert(a.at(4u) == -2, "Map does not find element with key 4");
static_assert(a.at(1u) == 4, "Map does not find element with key 1");
static_assert(a.at(3u) == 9, "Map does not find element with key 3");

} // namespace DynamicMapTests

namespace UpperTriangularMatrixTests {

// Can default-construct
constexpr auto defaultMatr = temple::UpperTriangularMatrix<bool, 15> {};

constexpr auto matr = temple::makeUpperTriangularMatrix(
  std::array<unsigned, 6> {{1, 2, 3, 4, 5, 6}}
);

/*constexpr auto failing = temple::makeUpperTriangularMatrix(
  std::array<unsigned, 5> {{1, 2, 3, 4, 5}}
);*/

constexpr auto fromArray = temple::makeUpperTriangularMatrix(
  temple::Array<unsigned, 6> {{1, 2, 3, 4, 5, 6}}
);

} // namespace UpperTriangularMatrixTests

namespace UIntArrayTests {

using Small = temple::UIntArray<unsigned>;
using Medium = temple::UIntArray<unsigned long>;
using Large = temple::UIntArray<unsigned long long>;

static_assert(Small::N == 9, "Small variant can store 9 integers");
static_assert(Medium::N == 19, "Medium variant can store 19 integers");
static_assert(Large::N == 19, "Large variant can store 19 integers");

//constexpr auto sampleArr = Small {1234567};
// constexpr auto sampleArr = Small {Array<unsigned, 7> {7, 6, 5, 4, 3, 2, 1}};
constexpr auto sampleArr = Small {7, 6, 5, 4, 3, 2, 1};

static_assert(sampleArr.at(0) == 7, "At doesn't work");
static_assert(sampleArr.at(1) == 6, "At doesn't work");
static_assert(sampleArr.at(2) == 5, "At doesn't work");
static_assert(sampleArr.at(3) == 4, "At doesn't work");
static_assert(sampleArr.at(4) == 3, "At doesn't work");
static_assert(sampleArr.at(5) == 2, "At doesn't work");
static_assert(sampleArr.at(6) == 1, "At doesn't work");

constexpr bool tryModifyArray() {
  Small arr {1, 2, 3, 4, 5, 6, 7};

  arr.at(0) = 4u;

  return arr.at(0) == 4;
}

static_assert(tryModifyArray(), "Modifying the array works");

} // namespace UIntArrayTests

BOOST_AUTO_TEST_CASE(dynamicUIntArrayTests) {
  constexpr temple::DynamicUIntArray<unsigned> arr {4, 3, 5};

  static_assert(
    arr.size() == 3,
    "Array size isn't initialized correctly from parameter pack ctor"
  );
  static_assert(
    arr.end() - arr.begin() == 3,
    "Subtracting begin/end iterators does not yield dynamic length"
  );

  static_assert(
    arr.begin() - arr.end() == -3,
    "Subtracting begin/end iterators does not yield dynamic length"
  );

  static_assert(
    *arr.begin() == 4,
    "Pointer to begin isn't correct"
  );

  static_assert(arr.front() == 4, "Front isn't right");
  static_assert(arr.back() == 5, "Back isn't right");

  temple::DynamicUIntArray<unsigned> changeable {4, 9, 1, 3, 5};

  BOOST_CHECK_MESSAGE(
    *changeable.begin() == 4
    && *(--changeable.end()) == 5
    && changeable.front() == 4
    && changeable.back() == 5,
    "non-const iterators don't work right"
  );

  constexpr temple::DynamicUIntArray<unsigned long> values {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};

  constexpr auto grouped = groupByEquality(
    values,
    std::equal_to<unsigned>()
  );

  constexpr temple::Array<unsigned, 4> f {4, 1, 9};
  constexpr auto initFromFixed = temple::DynamicUIntArray<unsigned> {f};

  BOOST_CHECK_MESSAGE(
    grouped.size() == 4
    && grouped.at(0).size() == 1
    && grouped.at(1).size() == 2
    && grouped.at(2).size() == 3
    && grouped.at(3).size() == 4,
    "Grouped doesn't work as expected, result is a size " << grouped.size()
    << " split"
  );
}

namespace BTreeStaticTests {

constexpr temple::BTree<unsigned, 3, 20> generateTree() {
  temple::BTree<unsigned, 3, 20> tree;

  tree.insert(9);
  tree.insert(3);
  tree.insert(5);
  tree.insert(20);

  return tree;
}

constexpr auto testTree = generateTree();

static_assert(
  /* BTree of minimum order 3 has max 5 keys per node and max 6 children per node
   *
   * height  nodes       keys
   * 0       1           5
   * 1       1 + 6       5 + 6*5
   * 2       1 + 6 + 36  5 + 6*5 + 36*5
   *
   * #nodes(h) = sum_{i = 0}^{h} (2t)^i
   *
   *     (2t)^{h + 1} - 1
   *  N = ----------------
   *         2t - 1
   *
   * -> N * (2t - 1) + 1 = (2t)^{h + 1}
   *
   * -> log_2t [N * (2t - 1) + 1] = h + 1
   *
   * -> h = log_2t [N * (2t - 1) + 1] - 1
   *
   */
  temple::BTreeProperties::minHeight(5, 3) == 0
  && temple::BTreeProperties::minHeight(35, 3) == 1
  && temple::BTreeProperties::minHeight(215, 3) == 2,
  "minHeight function is wrong"
);

static_assert(
  temple::BTreeProperties::maxNodesInTree(0, 3) == 1
  && temple::BTreeProperties::maxNodesInTree(1, 3) == 7
  && temple::BTreeProperties::maxNodesInTree(2, 3) == 43
  && temple::BTreeProperties::maxNodesInTree(3, 3) == 259,
  "maxNodesInTree is wrong"
);

} // namespace BTreeStaticTests

unsigned popRandom(std::set<unsigned>& values) {
  auto it = values.begin();

  std::advance(
    it,
    temple::random.getSingle<unsigned>(0, values.size() - 1)
  );

  auto value = *it;

  values.erase(it);

  return value;
}

BOOST_AUTO_TEST_CASE(BTreeTests) {
  constexpr unsigned nKeys = 100;

  using namespace std::string_literals;

  std::vector<unsigned> values (nKeys);

  std::iota(
    values.begin(),
    values.end(),
    0
  );

  std::set<unsigned> notInTree {values.begin(), values.end()};
  std::set<unsigned> inTree;

  temple::BTree<unsigned, 3, nKeys> tree;

  std::string lastTreeGraph;

  std::vector<std::string> decisions;

  auto addElement = [&](const std::string& lastTreeGraph) {
    // Add an element
    auto toAdd = popRandom(notInTree);
    decisions.emplace_back("i"s + std::to_string(toAdd));

    BOOST_CHECK_NO_THROW(tree.insert(toAdd));
    BOOST_REQUIRE_MESSAGE(
      lastTestPassed(),
      "Element insertion failed. Operation sequence: "
        << temple::condenseIterable(decisions)
        << ". Prior to last operation: \n"
        << lastTreeGraph << "\n\n After last operation: \n"
        << tree.dumpGraphviz()
    );

    inTree.insert(toAdd);
  };

  auto removeElement = [&](const std::string& lastTreeGraph) {
    // Remove an element
    auto toRemove = popRandom(inTree);
    decisions.emplace_back("r"s + std::to_string(toRemove));

    BOOST_CHECK_NO_THROW(tree.remove(toRemove));
    BOOST_REQUIRE_MESSAGE(
      lastTestPassed(),
      "Tree element removal failed. Operation sequence: "
        << temple::condenseIterable(decisions)
        << ". Prior to last operation: \n"
        << lastTreeGraph << "\n\n After last operation: \n"
        << tree.dumpGraphviz()
    );

    notInTree.insert(toRemove);
  };

  auto fullValidation = [&](const std::string& lastTreeGraph) {
    // Validate the tree
    BOOST_CHECK_NO_THROW(tree.validate());
    BOOST_REQUIRE_MESSAGE(
      lastTestPassed(),
      "Tree validation failed. Operation sequence: " 
        << temple::condenseIterable(decisions)
        << ". Prior to last operation: \n"
        << lastTreeGraph << "\n\n After last operation: \n"
        << tree.dumpGraphviz()
    );

    auto notInsertedNotContained = temple::mapToVector(
      notInTree,
      [&](const auto& notInTreeValue) -> bool {
        return !tree.contains(notInTreeValue);
      }
    );

    // Check that all elements are truly contained or not
    BOOST_REQUIRE_MESSAGE(
      temple::all_of(notInsertedNotContained),
      "Not all elements recorded as not in the tree are recognized as such!\n" 
        << "Found in the tree, but should not be present: "
        << temple::condenseIterable(
          temple::moveIf(
            temple::zipMap(
              notInsertedNotContained,
              notInTree,
              [](const bool& passed, const unsigned& value) -> std::string {
                if(!passed) {
                  return std::to_string(value);
                }

                return "";
              }
            ),
            [](const std::string& str) -> bool {
              return str != "";
            }
          )
        ) << "\nSequence of operations: " 
        << temple::condenseIterable(decisions)
        << ". Prior to last operation: \n"
        << lastTreeGraph << "\n\n After last operation: \n"
        << tree.dumpGraphviz()
    );

    auto insertedContained = temple::mapToVector(
      inTree,
      [&](const auto& inTreeValue) -> bool {
        return tree.contains(inTreeValue);
      }
    );

    BOOST_REQUIRE_MESSAGE(
      temple::all_of(insertedContained),
      "Not all elements recorded as contained in the tree are recognized as such!\n" 
        << "Not found in the tree: "
        << temple::condenseIterable(
          temple::moveIf(
            temple::zipMap(
              insertedContained,
              inTree,
              [](const bool& passed, const unsigned& value) -> std::string {
                if(!passed) {
                  return std::to_string(value);
                }

                return "";
              }
            ),
            [](const std::string& str) -> bool {
              return str != "";
            }
          )
        ) << "\nSequence of operations: " 
        << temple::condenseIterable(decisions)
        << ". Prior to last operation: \n"
        << lastTreeGraph << "\n\n After last operation: \n"
        << tree.dumpGraphviz()
    );
  };

  for(unsigned i = 0; i < 10; ++i) {
    decisions.clear();

    // Heavy insert-delete workload
    for(unsigned nSteps = 0; nSteps < 1000; ++nSteps) {
      lastTreeGraph = tree.dumpGraphviz();

      // Decide whether to insert or remove a random item
      auto decisionFloat = temple::random.getSingle<double>(0.0, 1.0);
      if(decisionFloat >= static_cast<double>(inTree.size()) / nKeys) {
        addElement(lastTreeGraph);
      } else {
        removeElement(lastTreeGraph);
      }

      fullValidation(lastTreeGraph);
    }

    auto matchingThroughIteration = temple::zipMap(
      tree,
      inTree,
      [&](const unsigned& treeValue, const unsigned& testValue) -> bool {
        if(treeValue != testValue) {
          std::cout << "Expected " << testValue << ", got " << treeValue << std::endl;
          return false;
        }

        return true;
      }
    );

    BOOST_REQUIRE_MESSAGE(
      temple::all_of(matchingThroughIteration),
      "BTree through-iteration does not yield the same elements as expected!\n"
        << tree.dumpGraphviz()
    );

    // Fill'er up all the way
    while(inTree.size() != nKeys) {
      std::string lastTreeGraph = tree.dumpGraphviz();

      addElement(lastTreeGraph);
      fullValidation(lastTreeGraph);
    }

    // Empty the tree
    while(inTree.size() > 0) {
      std::string lastTreeGraph = tree.dumpGraphviz();

      removeElement(lastTreeGraph);
      fullValidation(lastTreeGraph);
    }
  }
}

/* Test that if a BTree is instantiated with a specific size, that size
 * definitely fits in the tree
 */

template<size_t minOrder, size_t nElements> 
constexpr bool BTreeAllocatedSizeSufficient() {
  temple::BTree<unsigned, minOrder, nElements> tree;

  for(unsigned i = 0; i < nElements; ++i) {
    tree.insert(i);
  }

  return true;
}

template<size_t minOrder, size_t ... nElements>
constexpr bool testAllBTrees(std::index_sequence<nElements...>) {
  temple::Array<bool, sizeof...(nElements)> results {{
    BTreeAllocatedSizeSufficient<minOrder, 5 + nElements>()...
  }};

  for(unsigned i = 0; i < sizeof...(nElements); ++i) {
    if(!results.at(i)) {
      return false;
    }
  }

  return true;
}

template<size_t ... minOrders>
constexpr bool testAllBTrees(std::index_sequence<minOrders...>) {
  temple::Array<bool, sizeof...(minOrders)> results {{
    testAllBTrees<2 + minOrders>(std::make_index_sequence<45>{})... // Test sizes 5->50
  }};

  for(unsigned i = 0; i < sizeof...(minOrders); ++i) {
    if(!results.at(i)) {
      return false;
    }
  }

  return true;
}

constexpr bool testAllBTrees() {
  return testAllBTrees(std::make_index_sequence<3>{}); // Test min orders 2->5
}

static_assert(
  testAllBTrees(),
  "For some B-Trees, you cannot fit as many elements in as requested at instantiation"
);


namespace ConsecutiveCompareConstexprTests {
  static_assert(
    temple::consecutiveCompare(
      std::less<int>(),
      -4,
      -4,
      std::greater<unsigned>(),
      11,
      10
    ),
    "consecutive compare does not yield true"
  );

  constexpr int x = 4, y = 4;
  constexpr unsigned f = 5, g = 4;

  static_assert(
    temple::consecutiveCompare(
      std::less<int>(),
      x,
      y,
      std::greater<unsigned>(),
      f,
      g
    ),
    "Consecutive compare with references does not yield true"
  );
} // namespace ConsecutiveCompareConstexprTests