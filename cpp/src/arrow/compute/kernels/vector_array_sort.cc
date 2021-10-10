// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numeric>
#include <type_traits>
#include <utility>

#include "arrow/array/data.h"
#include "arrow/compute/api_vector.h"
#include "arrow/compute/kernels/common.h"
#include "arrow/compute/kernels/util_internal.h"
#include "arrow/compute/kernels/vector_sort_internal.h"
#include "arrow/type_traits.h"
#include "arrow/util/bit_block_counter.h"
#include "arrow/util/bitmap.h"
#include "arrow/util/bitmap_ops.h"
#include "arrow/util/checked_cast.h"
#include "arrow/visitor_inline.h"

namespace arrow {

using internal::checked_cast;

namespace compute {
namespace internal {

namespace {

// ----------------------------------------------------------------------
// partition_nth_indices implementation

// We need to preserve the options
using PartitionNthToIndicesState = internal::OptionsWrapper<PartitionNthOptions>;

template <typename OutType, typename InType>
struct PartitionNthToIndices {
  using ArrayType = typename TypeTraits<InType>::ArrayType;

  static Status Exec(KernelContext* ctx, const ExecBatch& batch, Datum* out) {
    using GetView = GetViewType<InType>;

    if (ctx->state() == nullptr) {
      return Status::Invalid("NthToIndices requires PartitionNthOptions");
    }
    const auto& options = PartitionNthToIndicesState::Get(ctx);

    ArrayType arr(batch[0].array());

    const int64_t pivot = options.pivot;
    if (pivot > arr.length()) {
      return Status::IndexError("NthToIndices index out of bound");
    }
    ArrayData* out_arr = out->mutable_array();
    uint64_t* out_begin = out_arr->GetMutableValues<uint64_t>(1);
    uint64_t* out_end = out_begin + arr.length();
    std::iota(out_begin, out_end, 0);
    if (pivot == arr.length()) {
      return Status::OK();
    }
    const auto p = PartitionNulls<ArrayType, NonStablePartitioner>(
        out_begin, out_end, arr, 0, options.null_placement);
    auto nth_begin = out_begin + pivot;
    if (nth_begin >= p.non_nulls_begin && nth_begin < p.non_nulls_end) {
      std::nth_element(p.non_nulls_begin, nth_begin, p.non_nulls_end,
                       [&arr](uint64_t left, uint64_t right) {
                         const auto lval = GetView::LogicalValue(arr.GetView(left));
                         const auto rval = GetView::LogicalValue(arr.GetView(right));
                         return lval < rval;
                       });
    }
    return Status::OK();
  }
};

template <typename OutType>
struct PartitionNthToIndices<OutType, NullType> {
  static Status Exec(KernelContext* ctx, const ExecBatch& batch, Datum* out) {
    if (ctx->state() == nullptr) {
      return Status::Invalid("NthToIndices requires PartitionNthOptions");
    }
    ArrayData* out_arr = out->mutable_array();
    uint64_t* out_begin = out_arr->GetMutableValues<uint64_t>(1);
    uint64_t* out_end = out_begin + batch.length;
    std::iota(out_begin, out_end, 0);
    return Status::OK();
  }
};

// ----------------------------------------------------------------------
// Array sorting implementations

template <typename ArrayType, typename VisitorNotNull, typename VisitorNull>
inline void VisitRawValuesInline(const ArrayType& values,
                                 VisitorNotNull&& visitor_not_null,
                                 VisitorNull&& visitor_null) {
  const auto data = values.raw_values();
  VisitBitBlocksVoid(
      values.null_bitmap(), values.offset(), values.length(),
      [&](int64_t i) { visitor_not_null(data[i]); }, [&]() { visitor_null(); });
}

template <typename VisitorNotNull, typename VisitorNull>
inline void VisitRawValuesInline(const BooleanArray& values,
                                 VisitorNotNull&& visitor_not_null,
                                 VisitorNull&& visitor_null) {
  if (values.null_count() != 0) {
    const uint8_t* data = values.data()->GetValues<uint8_t>(1, 0);
    VisitBitBlocksVoid(
        values.null_bitmap(), values.offset(), values.length(),
        [&](int64_t i) { visitor_not_null(BitUtil::GetBit(data, values.offset() + i)); },
        [&]() { visitor_null(); });
  } else {
    // Can avoid GetBit() overhead in the no-nulls case
    VisitBitBlocksVoid(
        values.data()->buffers[1], values.offset(), values.length(),
        [&](int64_t i) { visitor_not_null(true); }, [&]() { visitor_not_null(false); });
  }
}

template <typename ArrowType>
class ArrayCompareSorter {
  using ArrayType = typename TypeTraits<ArrowType>::ArrayType;
  using GetView = GetViewType<ArrowType>;

 public:
  // `offset` is used when this is called on a chunk of a chunked array
  NullPartitionResult operator()(uint64_t* indices_begin, uint64_t* indices_end,
                                 const Array& array, int64_t offset,
                                 const ArraySortOptions& options) {
    const auto& values = checked_cast<const ArrayType&>(array);

    const auto p = PartitionNulls<ArrayType, StablePartitioner>(
        indices_begin, indices_end, values, offset, options.null_placement);
    if (options.order == SortOrder::Ascending) {
      std::stable_sort(
          p.non_nulls_begin, p.non_nulls_end,
          [&values, &offset](uint64_t left, uint64_t right) {
            const auto lhs = GetView::LogicalValue(values.GetView(left - offset));
            const auto rhs = GetView::LogicalValue(values.GetView(right - offset));
            return lhs < rhs;
          });
    } else {
      std::stable_sort(
          p.non_nulls_begin, p.non_nulls_end,
          [&values, &offset](uint64_t left, uint64_t right) {
            const auto lhs = GetView::LogicalValue(values.GetView(left - offset));
            const auto rhs = GetView::LogicalValue(values.GetView(right - offset));
            // We don't use 'left > right' here to reduce required operator.
            // If we use 'right < left' here, '<' is only required.
            return rhs < lhs;
          });
    }
    return p;
  }
};

template <typename ArrowType>
class ArrayCountSorter {
  using ArrayType = typename TypeTraits<ArrowType>::ArrayType;
  using c_type = typename ArrowType::c_type;

 public:
  ArrayCountSorter() = default;

  explicit ArrayCountSorter(c_type min, c_type max) { SetMinMax(min, max); }

  // Assume: max >= min && (max - min) < 4Gi
  void SetMinMax(c_type min, c_type max) {
    min_ = min;
    value_range_ = static_cast<uint32_t>(max - min) + 1;
  }

  NullPartitionResult operator()(uint64_t* indices_begin, uint64_t* indices_end,
                                 const Array& array, int64_t offset,
                                 const ArraySortOptions& options) const {
    const auto& values = checked_cast<const ArrayType&>(array);

    // 32bit counter performs much better than 64bit one
    if (values.length() < (1LL << 32)) {
      return SortInternal<uint32_t>(indices_begin, indices_end, values, offset, options);
    } else {
      return SortInternal<uint64_t>(indices_begin, indices_end, values, offset, options);
    }
  }

 private:
  c_type min_{0};
  uint32_t value_range_{0};

  // `offset` is used when this is called on a chunk of a chunked array
  template <typename CounterType>
  NullPartitionResult SortInternal(uint64_t* indices_begin, uint64_t* indices_end,
                                   const ArrayType& values, int64_t offset,
                                   const ArraySortOptions& options) const {
    const uint32_t value_range = value_range_;

    // first and last slot reserved for prefix sum (depending on sort order)
    std::vector<CounterType> counts(2 + value_range);
    NullPartitionResult p;

    if (options.order == SortOrder::Ascending) {
      // counts will be increasing, starting with 0 and ending with (length - null_count)
      CountValues(values, &counts[1]);
      for (uint32_t i = 1; i <= value_range; ++i) {
        counts[i] += counts[i - 1];
      }

      if (options.null_placement == NullPlacement::AtStart) {
        p = NullPartitionResult::NullsAtStart(indices_begin, indices_end,
                                              indices_end - counts[value_range]);
      } else {
        p = NullPartitionResult::NullsAtEnd(indices_begin, indices_end,
                                            indices_begin + counts[value_range]);
      }
      EmitIndices(p, values, offset, &counts[0]);
    } else {
      // counts will be decreasing, starting with (length - null_count) and ending with 0
      CountValues(values, &counts[0]);
      for (uint32_t i = value_range; i >= 1; --i) {
        counts[i - 1] += counts[i];
      }

      if (options.null_placement == NullPlacement::AtStart) {
        p = NullPartitionResult::NullsAtStart(indices_begin, indices_end,
                                              indices_end - counts[0]);
      } else {
        p = NullPartitionResult::NullsAtEnd(indices_begin, indices_end,
                                            indices_begin + counts[0]);
      }
      EmitIndices(p, values, offset, &counts[1]);
    }
    return p;
  }

  template <typename CounterType>
  void CountValues(const ArrayType& values, CounterType* counts) const {
    VisitRawValuesInline(
        values, [&](c_type v) { ++counts[v - min_]; }, []() {});
  }

  template <typename CounterType>
  void EmitIndices(const NullPartitionResult& p, const ArrayType& values, int64_t offset,
                   CounterType* counts) const {
    int64_t index = offset;
    CounterType count_nulls = 0;
    VisitRawValuesInline(
        values, [&](c_type v) { p.non_nulls_begin[counts[v - min_]++] = index++; },
        [&]() { p.nulls_begin[count_nulls++] = index++; });
  }
};

template <>
class ArrayCountSorter<BooleanType> {
 public:
  ArrayCountSorter() = default;

  // `offset` is used when this is called on a chunk of a chunked array
  NullPartitionResult operator()(uint64_t* indices_begin, uint64_t* indices_end,
                                 const Array& array, int64_t offset,
                                 const ArraySortOptions& options) {
    const auto& values = checked_cast<const BooleanArray&>(array);

    std::array<int64_t, 3> counts{0, 0, 0};  // false, true, null

    const int64_t nulls = values.null_count();
    const int64_t ones = values.true_count();
    const int64_t zeros = values.length() - ones - nulls;

    NullPartitionResult p;
    if (options.null_placement == NullPlacement::AtStart) {
      p = NullPartitionResult::NullsAtStart(indices_begin, indices_end,
                                            indices_begin + nulls);
    } else {
      p = NullPartitionResult::NullsAtEnd(indices_begin, indices_end,
                                          indices_end - nulls);
    }

    if (options.order == SortOrder::Ascending) {
      // ones start after zeros
      counts[1] = zeros;
    } else {
      // zeros start after ones
      counts[0] = ones;
    }

    int64_t index = offset;
    VisitRawValuesInline(
        values, [&](bool v) { p.non_nulls_begin[counts[v]++] = index++; },
        [&]() { p.nulls_begin[counts[2]++] = index++; });
    return p;
  }
};

// Sort integers with counting sort or comparison based sorting algorithm
// - Use O(n) counting sort if values are in a small range
// - Use O(nlogn) std::stable_sort otherwise
template <typename ArrowType>
class ArrayCountOrCompareSorter {
  using ArrayType = typename TypeTraits<ArrowType>::ArrayType;
  using c_type = typename ArrowType::c_type;

 public:
  // `offset` is used when this is called on a chunk of a chunked array
  NullPartitionResult operator()(uint64_t* indices_begin, uint64_t* indices_end,
                                 const Array& array, int64_t offset,
                                 const ArraySortOptions& options) {
    const auto& values = checked_cast<const ArrayType&>(array);

    if (values.length() >= countsort_min_len_ && values.length() > values.null_count()) {
      c_type min, max;
      std::tie(min, max) = GetMinMax<c_type>(*values.data());

      // For signed int32/64, (max - min) may overflow and trigger UBSAN.
      // Cast to largest unsigned type(uint64_t) before subtraction.
      if (static_cast<uint64_t>(max) - static_cast<uint64_t>(min) <=
          countsort_max_range_) {
        count_sorter_.SetMinMax(min, max);
        return count_sorter_(indices_begin, indices_end, values, offset, options);
      }
    }

    return compare_sorter_(indices_begin, indices_end, values, offset, options);
  }

 private:
  ArrayCompareSorter<ArrowType> compare_sorter_;
  ArrayCountSorter<ArrowType> count_sorter_;

  // Cross point to prefer counting sort than stl::stable_sort(merge sort)
  // - array to be sorted is longer than "count_min_len_"
  // - value range (max-min) is within "count_max_range_"
  //
  // The optimal setting depends heavily on running CPU. Below setting is
  // conservative to adapt to various hardware and keep code simple.
  // It's possible to decrease array-len and/or increase value-range to cover
  // more cases, or setup a table for best array-len/value-range combinations.
  // See https://issues.apache.org/jira/browse/ARROW-1571 for detailed analysis.
  static const uint32_t countsort_min_len_ = 1024;
  static const uint32_t countsort_max_range_ = 4096;
};

class ArrayNullSorter {
 public:
  NullPartitionResult operator()(uint64_t* indices_begin, uint64_t* indices_end,
                                 const Array& values, int64_t offset,
                                 const ArraySortOptions& options) {
    return NullPartitionResult::NullsOnly(indices_begin, indices_end,
                                          options.null_placement);
  }
};

//
// Generic Array sort dispatcher for physical types
//

template <typename Type, typename Enable = void>
struct ArraySorter {};

template <>
struct ArraySorter<NullType> {
  ArrayNullSorter impl;
};

template <>
struct ArraySorter<BooleanType> {
  ArrayCountSorter<BooleanType> impl;
};

template <>
struct ArraySorter<UInt8Type> {
  ArrayCountSorter<UInt8Type> impl;
  ArraySorter() : impl(0, 255) {}
};

template <>
struct ArraySorter<Int8Type> {
  ArrayCountSorter<Int8Type> impl;
  ArraySorter() : impl(-128, 127) {}
};

template <typename Type>
struct ArraySorter<Type, enable_if_t<is_integer_type<Type>::value &&
                                     (sizeof(typename Type::c_type) > 1)>> {
  static constexpr bool is_supported = true;
  ArrayCountOrCompareSorter<Type> impl;
};

template <typename Type>
struct ArraySorter<
    Type, enable_if_t<is_floating_type<Type>::value || is_base_binary_type<Type>::value ||
                      is_fixed_size_binary_type<Type>::value>> {
  ArrayCompareSorter<Type> impl;
};

struct ArraySorterFactory {
  ArraySortFunc sorter;

  Status Visit(const DataType& type) {
    return Status::TypeError("Sorting not supported for type ", type.ToString());
  }

  template <typename T, typename U = decltype(ArraySorter<T>::impl)>
  Status Visit(const T& type, U* = nullptr) {
    sorter = ArraySortFunc(std::move(ArraySorter<T>{}.impl));
    return Status::OK();
  }

  Result<ArraySortFunc> MakeSorter(const DataType& type) {
    RETURN_NOT_OK(VisitTypeInline(type, this));
    DCHECK(sorter);
    return std::move(sorter);
  }
};

using ArraySortIndicesState = internal::OptionsWrapper<ArraySortOptions>;

template <typename OutType, typename InType>
struct ArraySortIndices {
  using ArrayType = typename TypeTraits<InType>::ArrayType;

  static Status Exec(KernelContext* ctx, const ExecBatch& batch, Datum* out) {
    const auto& options = ArraySortIndicesState::Get(ctx);
    ArrayType arr(batch[0].array());
    ARROW_ASSIGN_OR_RAISE(auto sorter, GetArraySorter(*GetPhysicalType(arr.type())));

    ArrayData* out_arr = out->mutable_array();
    uint64_t* out_begin = out_arr->GetMutableValues<uint64_t>(1);
    uint64_t* out_end = out_begin + arr.length();
    std::iota(out_begin, out_end, 0);

    sorter(out_begin, out_end, arr, 0, options);
    return Status::OK();
  }
};

template <template <typename...> class ExecTemplate>
void AddArraySortingKernels(VectorKernel base, VectorFunction* func) {
  // null type
  base.signature = KernelSignature::Make({InputType::Array(null())}, uint64());
  base.exec = ExecTemplate<UInt64Type, NullType>::Exec;
  DCHECK_OK(func->AddKernel(base));

  // bool type
  base.signature = KernelSignature::Make({InputType::Array(boolean())}, uint64());
  base.exec = ExecTemplate<UInt64Type, BooleanType>::Exec;
  DCHECK_OK(func->AddKernel(base));

  // duration type
  base.signature = KernelSignature::Make({InputType::Array(Type::DURATION)}, uint64());
  base.exec = GenerateNumeric<ExecTemplate, UInt64Type>(*int64());
  DCHECK_OK(func->AddKernel(base));

  for (const auto& ty : NumericTypes()) {
    auto physical_type = GetPhysicalType(ty);
    base.signature = KernelSignature::Make({InputType::Array(ty)}, uint64());
    base.exec = GenerateNumeric<ExecTemplate, UInt64Type>(*physical_type);
    DCHECK_OK(func->AddKernel(base));
  }
  for (const auto& ty : TemporalTypes()) {
    auto physical_type = GetPhysicalType(ty);
    base.signature = KernelSignature::Make({InputType::Array(ty->id())}, uint64());
    base.exec = GenerateNumeric<ExecTemplate, UInt64Type>(*physical_type);
    DCHECK_OK(func->AddKernel(base));
  }
  for (const auto id : {Type::DECIMAL128, Type::DECIMAL256}) {
    base.signature = KernelSignature::Make({InputType::Array(id)}, uint64());
    base.exec = GenerateDecimal<ExecTemplate, UInt64Type>(id);
    DCHECK_OK(func->AddKernel(base));
  }
  for (const auto& ty : BaseBinaryTypes()) {
    auto physical_type = GetPhysicalType(ty);
    base.signature = KernelSignature::Make({InputType::Array(ty)}, uint64());
    base.exec = GenerateVarBinaryBase<ExecTemplate, UInt64Type>(*physical_type);
    DCHECK_OK(func->AddKernel(base));
  }
  base.signature =
      KernelSignature::Make({InputType::Array(Type::FIXED_SIZE_BINARY)}, uint64());
  base.exec = ExecTemplate<UInt64Type, FixedSizeBinaryType>::Exec;
  DCHECK_OK(func->AddKernel(base));
}

const auto kDefaultArraySortOptions = ArraySortOptions::Defaults();

const FunctionDoc array_sort_indices_doc(
    "Return the indices that would sort an array",
    ("This function computes an array of indices that define a stable sort\n"
     "of the input array.  By default, Null values are considered greater\n"
     "than any other value and are therefore sorted at the end of the array.\n"
     "For floating-point types, NaNs are considered greater than any\n"
     "other non-null value, but smaller than null values.\n"
     "\n"
     "The handling of nulls and NaNs can be changed in ArraySortOptions."),
    {"array"}, "ArraySortOptions");

const FunctionDoc partition_nth_indices_doc(
    "Return the indices that would partition an array around a pivot",
    ("This functions computes an array of indices that define a non-stable\n"
     "partial sort of the input array.\n"
     "\n"
     "The output is such that the `N`'th index points to the `N`'th element\n"
     "of the input in sorted order, and all indices before the `N`'th point\n"
     "to elements in the input less or equal to elements at or after the `N`'th.\n"
     "\n"
     "By default, null values are considered greater than any other value\n"
     "and are therefore partitioned towards the end of the array.\n"
     "For floating-point types, NaNs are considered greater than any\n"
     "other non-null value, but smaller than null values.\n"
     "\n"
     "The pivot index `N` must be given in PartitionNthOptions.\n"
     "The handling of nulls and NaNs can also be changed in PartitionNthOptions."),
    {"array"}, "PartitionNthOptions");

}  // namespace

Result<ArraySortFunc> GetArraySorter(const DataType& type) {
  ArraySorterFactory factory;
  return factory.MakeSorter(type);
}

void RegisterVectorArraySort(FunctionRegistry* registry) {
  // The kernel outputs into preallocated memory and is never null
  VectorKernel base;
  base.mem_allocation = MemAllocation::PREALLOCATE;
  base.null_handling = NullHandling::OUTPUT_NOT_NULL;

  auto array_sort_indices = std::make_shared<VectorFunction>(
      "array_sort_indices", Arity::Unary(), &array_sort_indices_doc,
      &kDefaultArraySortOptions);
  base.init = ArraySortIndicesState::Init;
  AddArraySortingKernels<ArraySortIndices>(base, array_sort_indices.get());
  DCHECK_OK(registry->AddFunction(std::move(array_sort_indices)));

  // partition_nth_indices has a parameter so needs its init function
  auto part_indices = std::make_shared<VectorFunction>(
      "partition_nth_indices", Arity::Unary(), &partition_nth_indices_doc);
  base.init = PartitionNthToIndicesState::Init;
  AddArraySortingKernels<PartitionNthToIndices>(base, part_indices.get());
  DCHECK_OK(registry->AddFunction(std::move(part_indices)));
}

}  // namespace internal
}  // namespace compute
}  // namespace arrow
