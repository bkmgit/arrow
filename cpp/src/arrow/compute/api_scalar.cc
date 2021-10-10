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

#include "arrow/compute/api_scalar.h"

#include <memory>
#include <sstream>
#include <string>

#include "arrow/array/array_base.h"
#include "arrow/compute/exec.h"
#include "arrow/compute/function_internal.h"
#include "arrow/compute/registry.h"
#include "arrow/status.h"
#include "arrow/type.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/logging.h"

namespace arrow {

namespace internal {
template <>
struct EnumTraits<compute::JoinOptions::NullHandlingBehavior>
    : BasicEnumTraits<compute::JoinOptions::NullHandlingBehavior,
                      compute::JoinOptions::NullHandlingBehavior::EMIT_NULL,
                      compute::JoinOptions::NullHandlingBehavior::SKIP,
                      compute::JoinOptions::NullHandlingBehavior::REPLACE> {
  static std::string name() { return "JoinOptions::NullHandlingBehavior"; }
  static std::string value_name(compute::JoinOptions::NullHandlingBehavior value) {
    switch (value) {
      case compute::JoinOptions::NullHandlingBehavior::EMIT_NULL:
        return "EMIT_NULL";
      case compute::JoinOptions::NullHandlingBehavior::SKIP:
        return "SKIP";
      case compute::JoinOptions::NullHandlingBehavior::REPLACE:
        return "REPLACE";
    }
    return "<INVALID>";
  }
};

template <>
struct EnumTraits<TimeUnit::type>
    : BasicEnumTraits<TimeUnit::type, TimeUnit::type::SECOND, TimeUnit::type::MILLI,
                      TimeUnit::type::MICRO, TimeUnit::type::NANO> {
  static std::string name() { return "TimeUnit::type"; }
  static std::string value_name(TimeUnit::type value) {
    switch (value) {
      case TimeUnit::type::SECOND:
        return "SECOND";
      case TimeUnit::type::MILLI:
        return "MILLI";
      case TimeUnit::type::MICRO:
        return "MICRO";
      case TimeUnit::type::NANO:
        return "NANO";
    }
    return "<INVALID>";
  }
};

template <>
struct EnumTraits<compute::CompareOperator>
    : BasicEnumTraits<
          compute::CompareOperator, compute::CompareOperator::EQUAL,
          compute::CompareOperator::NOT_EQUAL, compute::CompareOperator::GREATER,
          compute::CompareOperator::GREATER_EQUAL, compute::CompareOperator::LESS,
          compute::CompareOperator::LESS_EQUAL> {
  static std::string name() { return "compute::CompareOperator"; }
  static std::string value_name(compute::CompareOperator value) {
    switch (value) {
      case compute::CompareOperator::EQUAL:
        return "EQUAL";
      case compute::CompareOperator::NOT_EQUAL:
        return "NOT_EQUAL";
      case compute::CompareOperator::GREATER:
        return "GREATER";
      case compute::CompareOperator::GREATER_EQUAL:
        return "GREATER_EQUAL";
      case compute::CompareOperator::LESS:
        return "LESS";
      case compute::CompareOperator::LESS_EQUAL:
        return "LESS_EQUAL";
    }
    return "<INVALID>";
  }
};
template <>
struct EnumTraits<compute::AssumeTimezoneOptions::Ambiguous>
    : BasicEnumTraits<compute::AssumeTimezoneOptions::Ambiguous,
                      compute::AssumeTimezoneOptions::Ambiguous::AMBIGUOUS_RAISE,
                      compute::AssumeTimezoneOptions::Ambiguous::AMBIGUOUS_EARLIEST,
                      compute::AssumeTimezoneOptions::Ambiguous::AMBIGUOUS_LATEST> {
  static std::string name() { return "AssumeTimezoneOptions::Ambiguous"; }
  static std::string value_name(compute::AssumeTimezoneOptions::Ambiguous value) {
    switch (value) {
      case compute::AssumeTimezoneOptions::Ambiguous::AMBIGUOUS_RAISE:
        return "AMBIGUOUS_RAISE";
      case compute::AssumeTimezoneOptions::Ambiguous::AMBIGUOUS_EARLIEST:
        return "AMBIGUOUS_EARLIEST";
      case compute::AssumeTimezoneOptions::Ambiguous::AMBIGUOUS_LATEST:
        return "AMBIGUOUS_LATEST";
    }
    return "<INVALID>";
  }
};
template <>
struct EnumTraits<compute::AssumeTimezoneOptions::Nonexistent>
    : BasicEnumTraits<compute::AssumeTimezoneOptions::Nonexistent,
                      compute::AssumeTimezoneOptions::Nonexistent::NONEXISTENT_RAISE,
                      compute::AssumeTimezoneOptions::Nonexistent::NONEXISTENT_EARLIEST,
                      compute::AssumeTimezoneOptions::Nonexistent::NONEXISTENT_LATEST> {
  static std::string name() { return "AssumeTimezoneOptions::Nonexistent"; }
  static std::string value_name(compute::AssumeTimezoneOptions::Nonexistent value) {
    switch (value) {
      case compute::AssumeTimezoneOptions::Nonexistent::NONEXISTENT_RAISE:
        return "NONEXISTENT_RAISE";
      case compute::AssumeTimezoneOptions::Nonexistent::NONEXISTENT_EARLIEST:
        return "NONEXISTENT_EARLIEST";
      case compute::AssumeTimezoneOptions::Nonexistent::NONEXISTENT_LATEST:
        return "NONEXISTENT_LATEST";
    }
    return "<INVALID>";
  }
};

template <>
struct EnumTraits<compute::RoundMode>
    : BasicEnumTraits<compute::RoundMode, compute::RoundMode::DOWN,
                      compute::RoundMode::UP, compute::RoundMode::TOWARDS_ZERO,
                      compute::RoundMode::TOWARDS_INFINITY, compute::RoundMode::HALF_DOWN,
                      compute::RoundMode::HALF_UP, compute::RoundMode::HALF_TOWARDS_ZERO,
                      compute::RoundMode::HALF_TOWARDS_INFINITY,
                      compute::RoundMode::HALF_TO_EVEN, compute::RoundMode::HALF_TO_ODD> {
  static std::string name() { return "compute::RoundMode"; }
  static std::string value_name(compute::RoundMode value) {
    switch (value) {
      case compute::RoundMode::DOWN:
        return "DOWN";
      case compute::RoundMode::UP:
        return "UP";
      case compute::RoundMode::TOWARDS_ZERO:
        return "TOWARDS_ZERO";
      case compute::RoundMode::TOWARDS_INFINITY:
        return "TOWARDS_INFINITY";
      case compute::RoundMode::HALF_DOWN:
        return "HALF_DOWN";
      case compute::RoundMode::HALF_UP:
        return "HALF_UP";
      case compute::RoundMode::HALF_TOWARDS_ZERO:
        return "HALF_TOWARDS_ZERO";
      case compute::RoundMode::HALF_TOWARDS_INFINITY:
        return "HALF_TOWARDS_INFINITY";
      case compute::RoundMode::HALF_TO_EVEN:
        return "HALF_TO_EVEN";
      case compute::RoundMode::HALF_TO_ODD:
        return "HALF_TO_ODD";
    }
    return "<INVALID>";
  }
};
}  // namespace internal

namespace compute {

// ----------------------------------------------------------------------
// Function options

using ::arrow::internal::checked_cast;

namespace internal {
namespace {
using ::arrow::internal::DataMember;
static auto kArithmeticOptionsType = GetFunctionOptionsType<ArithmeticOptions>(
    DataMember("check_overflow", &ArithmeticOptions::check_overflow));
static auto kElementWiseAggregateOptionsType =
    GetFunctionOptionsType<ElementWiseAggregateOptions>(
        DataMember("skip_nulls", &ElementWiseAggregateOptions::skip_nulls));
static auto kRoundOptionsType = GetFunctionOptionsType<RoundOptions>(
    DataMember("ndigits", &RoundOptions::ndigits),
    DataMember("round_mode", &RoundOptions::round_mode));
static auto kRoundToMultipleOptionsType = GetFunctionOptionsType<RoundToMultipleOptions>(
    DataMember("multiple", &RoundToMultipleOptions::multiple),
    DataMember("round_mode", &RoundToMultipleOptions::round_mode));
static auto kJoinOptionsType = GetFunctionOptionsType<JoinOptions>(
    DataMember("null_handling", &JoinOptions::null_handling),
    DataMember("null_replacement", &JoinOptions::null_replacement));
static auto kMatchSubstringOptionsType = GetFunctionOptionsType<MatchSubstringOptions>(
    DataMember("pattern", &MatchSubstringOptions::pattern),
    DataMember("ignore_case", &MatchSubstringOptions::ignore_case));
static auto kSplitOptionsType = GetFunctionOptionsType<SplitOptions>(
    DataMember("max_splits", &SplitOptions::max_splits),
    DataMember("reverse", &SplitOptions::reverse));
static auto kSplitPatternOptionsType = GetFunctionOptionsType<SplitPatternOptions>(
    DataMember("pattern", &SplitPatternOptions::pattern),
    DataMember("max_splits", &SplitPatternOptions::max_splits),
    DataMember("reverse", &SplitPatternOptions::reverse));
static auto kReplaceSliceOptionsType = GetFunctionOptionsType<ReplaceSliceOptions>(
    DataMember("start", &ReplaceSliceOptions::start),
    DataMember("stop", &ReplaceSliceOptions::stop),
    DataMember("replacement", &ReplaceSliceOptions::replacement));
static auto kReplaceSubstringOptionsType =
    GetFunctionOptionsType<ReplaceSubstringOptions>(
        DataMember("pattern", &ReplaceSubstringOptions::pattern),
        DataMember("replacement", &ReplaceSubstringOptions::replacement),
        DataMember("max_replacements", &ReplaceSubstringOptions::max_replacements));
static auto kExtractRegexOptionsType = GetFunctionOptionsType<ExtractRegexOptions>(
    DataMember("pattern", &ExtractRegexOptions::pattern));
static auto kSetLookupOptionsType = GetFunctionOptionsType<SetLookupOptions>(
    DataMember("value_set", &SetLookupOptions::value_set),
    DataMember("skip_nulls", &SetLookupOptions::skip_nulls));
static auto kStrptimeOptionsType = GetFunctionOptionsType<StrptimeOptions>(
    DataMember("format", &StrptimeOptions::format),
    DataMember("unit", &StrptimeOptions::unit));
static auto kStrftimeOptionsType = GetFunctionOptionsType<StrftimeOptions>(
    DataMember("format", &StrftimeOptions::format));
static auto kAssumeTimezoneOptionsType = GetFunctionOptionsType<AssumeTimezoneOptions>(
    DataMember("timezone", &AssumeTimezoneOptions::timezone),
    DataMember("ambiguous", &AssumeTimezoneOptions::ambiguous),
    DataMember("nonexistent", &AssumeTimezoneOptions::nonexistent));
static auto kPadOptionsType = GetFunctionOptionsType<PadOptions>(
    DataMember("width", &PadOptions::width), DataMember("padding", &PadOptions::padding));
static auto kTrimOptionsType = GetFunctionOptionsType<TrimOptions>(
    DataMember("characters", &TrimOptions::characters));
static auto kSliceOptionsType = GetFunctionOptionsType<SliceOptions>(
    DataMember("start", &SliceOptions::start), DataMember("stop", &SliceOptions::stop),
    DataMember("step", &SliceOptions::step));
static auto kMakeStructOptionsType = GetFunctionOptionsType<MakeStructOptions>(
    DataMember("field_names", &MakeStructOptions::field_names),
    DataMember("field_nullability", &MakeStructOptions::field_nullability),
    DataMember("field_metadata", &MakeStructOptions::field_metadata));
static auto kDayOfWeekOptionsType = GetFunctionOptionsType<DayOfWeekOptions>(
    DataMember("count_from_zero", &DayOfWeekOptions::count_from_zero),
    DataMember("week_start", &DayOfWeekOptions::week_start));
static auto kWeekOptionsType = GetFunctionOptionsType<WeekOptions>(
    DataMember("week_starts_monday", &WeekOptions::week_starts_monday),
    DataMember("count_from_zero", &WeekOptions::count_from_zero),
    DataMember("first_week_is_fully_in_year", &WeekOptions::first_week_is_fully_in_year));
static auto kNullOptionsType = GetFunctionOptionsType<NullOptions>(
    DataMember("nan_is_null", &NullOptions::nan_is_null));
}  // namespace
}  // namespace internal

ArithmeticOptions::ArithmeticOptions(bool check_overflow)
    : FunctionOptions(internal::kArithmeticOptionsType), check_overflow(check_overflow) {}
constexpr char ArithmeticOptions::kTypeName[];

ElementWiseAggregateOptions::ElementWiseAggregateOptions(bool skip_nulls)
    : FunctionOptions(internal::kElementWiseAggregateOptionsType),
      skip_nulls(skip_nulls) {}
constexpr char ElementWiseAggregateOptions::kTypeName[];

RoundOptions::RoundOptions(int64_t ndigits, RoundMode round_mode)
    : FunctionOptions(internal::kRoundOptionsType),
      ndigits(ndigits),
      round_mode(round_mode) {
  static_assert(RoundMode::HALF_DOWN > RoundMode::DOWN &&
                    RoundMode::HALF_DOWN > RoundMode::UP &&
                    RoundMode::HALF_DOWN > RoundMode::TOWARDS_ZERO &&
                    RoundMode::HALF_DOWN > RoundMode::TOWARDS_INFINITY &&
                    RoundMode::HALF_DOWN < RoundMode::HALF_UP &&
                    RoundMode::HALF_DOWN < RoundMode::HALF_TOWARDS_ZERO &&
                    RoundMode::HALF_DOWN < RoundMode::HALF_TOWARDS_INFINITY &&
                    RoundMode::HALF_DOWN < RoundMode::HALF_TO_EVEN &&
                    RoundMode::HALF_DOWN < RoundMode::HALF_TO_ODD,
                "Invalid order of round modes. Modes prefixed with HALF need to be "
                "enumerated last with HALF_DOWN being the first among them.");
}
constexpr char RoundOptions::kTypeName[];

RoundToMultipleOptions::RoundToMultipleOptions(double multiple, RoundMode round_mode)
    : RoundToMultipleOptions(std::make_shared<DoubleScalar>(multiple), round_mode) {}
RoundToMultipleOptions::RoundToMultipleOptions(std::shared_ptr<Scalar> multiple,
                                               RoundMode round_mode)
    : FunctionOptions(internal::kRoundToMultipleOptionsType),
      multiple(std::move(multiple)),
      round_mode(round_mode) {}
constexpr char RoundToMultipleOptions::kTypeName[];

JoinOptions::JoinOptions(NullHandlingBehavior null_handling, std::string null_replacement)
    : FunctionOptions(internal::kJoinOptionsType),
      null_handling(null_handling),
      null_replacement(std::move(null_replacement)) {}
constexpr char JoinOptions::kTypeName[];

MatchSubstringOptions::MatchSubstringOptions(std::string pattern, bool ignore_case)
    : FunctionOptions(internal::kMatchSubstringOptionsType),
      pattern(std::move(pattern)),
      ignore_case(ignore_case) {}
MatchSubstringOptions::MatchSubstringOptions() : MatchSubstringOptions("", false) {}
constexpr char MatchSubstringOptions::kTypeName[];

SplitOptions::SplitOptions(int64_t max_splits, bool reverse)
    : FunctionOptions(internal::kSplitOptionsType),
      max_splits(max_splits),
      reverse(reverse) {}
constexpr char SplitOptions::kTypeName[];

SplitPatternOptions::SplitPatternOptions(std::string pattern, int64_t max_splits,
                                         bool reverse)
    : FunctionOptions(internal::kSplitPatternOptionsType),
      pattern(std::move(pattern)),
      max_splits(max_splits),
      reverse(reverse) {}
SplitPatternOptions::SplitPatternOptions() : SplitPatternOptions("", -1, false) {}
constexpr char SplitPatternOptions::kTypeName[];

ReplaceSliceOptions::ReplaceSliceOptions(int64_t start, int64_t stop,
                                         std::string replacement)
    : FunctionOptions(internal::kReplaceSliceOptionsType),
      start(start),
      stop(stop),
      replacement(std::move(replacement)) {}
ReplaceSliceOptions::ReplaceSliceOptions() : ReplaceSliceOptions(0, 0, "") {}
constexpr char ReplaceSliceOptions::kTypeName[];

ReplaceSubstringOptions::ReplaceSubstringOptions(std::string pattern,
                                                 std::string replacement,
                                                 int64_t max_replacements)
    : FunctionOptions(internal::kReplaceSubstringOptionsType),
      pattern(std::move(pattern)),
      replacement(std::move(replacement)),
      max_replacements(max_replacements) {}
ReplaceSubstringOptions::ReplaceSubstringOptions()
    : ReplaceSubstringOptions("", "", -1) {}
constexpr char ReplaceSubstringOptions::kTypeName[];

ExtractRegexOptions::ExtractRegexOptions(std::string pattern)
    : FunctionOptions(internal::kExtractRegexOptionsType), pattern(std::move(pattern)) {}
ExtractRegexOptions::ExtractRegexOptions() : ExtractRegexOptions("") {}
constexpr char ExtractRegexOptions::kTypeName[];

SetLookupOptions::SetLookupOptions(Datum value_set, bool skip_nulls)
    : FunctionOptions(internal::kSetLookupOptionsType),
      value_set(std::move(value_set)),
      skip_nulls(skip_nulls) {}
SetLookupOptions::SetLookupOptions() : SetLookupOptions({}, false) {}
constexpr char SetLookupOptions::kTypeName[];

StrptimeOptions::StrptimeOptions(std::string format, TimeUnit::type unit)
    : FunctionOptions(internal::kStrptimeOptionsType),
      format(std::move(format)),
      unit(unit) {}
StrptimeOptions::StrptimeOptions() : StrptimeOptions("", TimeUnit::SECOND) {}
constexpr char StrptimeOptions::kTypeName[];

StrftimeOptions::StrftimeOptions(std::string format, std::string locale)
    : FunctionOptions(internal::kStrftimeOptionsType),
      format(std::move(format)),
      locale(std::move(locale)) {}
StrftimeOptions::StrftimeOptions() : StrftimeOptions(kDefaultFormat) {}
constexpr char StrftimeOptions::kTypeName[];
constexpr const char* StrftimeOptions::kDefaultFormat;

AssumeTimezoneOptions::AssumeTimezoneOptions(std::string timezone, Ambiguous ambiguous,
                                             Nonexistent nonexistent)
    : FunctionOptions(internal::kAssumeTimezoneOptionsType),
      timezone(std::move(timezone)),
      ambiguous(ambiguous),
      nonexistent(nonexistent) {}
AssumeTimezoneOptions::AssumeTimezoneOptions() : AssumeTimezoneOptions("UTC") {}
constexpr char AssumeTimezoneOptions::kTypeName[];

PadOptions::PadOptions(int64_t width, std::string padding)
    : FunctionOptions(internal::kPadOptionsType),
      width(width),
      padding(std::move(padding)) {}
PadOptions::PadOptions() : PadOptions(0, " ") {}
constexpr char PadOptions::kTypeName[];

TrimOptions::TrimOptions(std::string characters)
    : FunctionOptions(internal::kTrimOptionsType), characters(std::move(characters)) {}
TrimOptions::TrimOptions() : TrimOptions("") {}
constexpr char TrimOptions::kTypeName[];

SliceOptions::SliceOptions(int64_t start, int64_t stop, int64_t step)
    : FunctionOptions(internal::kSliceOptionsType),
      start(start),
      stop(stop),
      step(step) {}
SliceOptions::SliceOptions() : SliceOptions(0, 0, 1) {}
constexpr char SliceOptions::kTypeName[];

MakeStructOptions::MakeStructOptions(
    std::vector<std::string> n, std::vector<bool> r,
    std::vector<std::shared_ptr<const KeyValueMetadata>> m)
    : FunctionOptions(internal::kMakeStructOptionsType),
      field_names(std::move(n)),
      field_nullability(std::move(r)),
      field_metadata(std::move(m)) {}

MakeStructOptions::MakeStructOptions(std::vector<std::string> n)
    : FunctionOptions(internal::kMakeStructOptionsType),
      field_names(std::move(n)),
      field_nullability(field_names.size(), true),
      field_metadata(field_names.size(), NULLPTR) {}

MakeStructOptions::MakeStructOptions() : MakeStructOptions(std::vector<std::string>()) {}
constexpr char MakeStructOptions::kTypeName[];

DayOfWeekOptions::DayOfWeekOptions(bool count_from_zero, uint32_t week_start)
    : FunctionOptions(internal::kDayOfWeekOptionsType),
      count_from_zero(count_from_zero),
      week_start(week_start) {}
constexpr char DayOfWeekOptions::kTypeName[];

WeekOptions::WeekOptions(bool week_starts_monday, bool count_from_zero,
                         bool first_week_is_fully_in_year)
    : FunctionOptions(internal::kWeekOptionsType),
      week_starts_monday(week_starts_monday),
      count_from_zero(count_from_zero),
      first_week_is_fully_in_year(first_week_is_fully_in_year) {}
constexpr char WeekOptions::kTypeName[];

NullOptions::NullOptions(bool nan_is_null)
    : FunctionOptions(internal::kNullOptionsType), nan_is_null(nan_is_null) {}
constexpr char NullOptions::kTypeName[];

namespace internal {
void RegisterScalarOptions(FunctionRegistry* registry) {
  DCHECK_OK(registry->AddFunctionOptionsType(kArithmeticOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kElementWiseAggregateOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kRoundOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kRoundToMultipleOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kJoinOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kMatchSubstringOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kSplitOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kSplitPatternOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kReplaceSliceOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kReplaceSubstringOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kExtractRegexOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kSetLookupOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kStrptimeOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kStrftimeOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kAssumeTimezoneOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kPadOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kTrimOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kSliceOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kMakeStructOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kDayOfWeekOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kWeekOptionsType));
  DCHECK_OK(registry->AddFunctionOptionsType(kNullOptionsType));
}
}  // namespace internal

#define SCALAR_EAGER_UNARY(NAME, REGISTRY_NAME)              \
  Result<Datum> NAME(const Datum& value, ExecContext* ctx) { \
    return CallFunction(REGISTRY_NAME, {value}, ctx);        \
  }

#define SCALAR_EAGER_BINARY(NAME, REGISTRY_NAME)                                \
  Result<Datum> NAME(const Datum& left, const Datum& right, ExecContext* ctx) { \
    return CallFunction(REGISTRY_NAME, {left, right}, ctx);                     \
  }

#define SCALAR_EAGER_TERNARY(NAME, REGISTRY_NAME)                               \
  Result<Datum> NAME(const Datum& value, const Datum& left, const Datum& right, \
                     ExecContext* ctx) {                                        \
    return CallFunction(REGISTRY_NAME, {value, left, right}, ctx);              \
  }

// ----------------------------------------------------------------------
// Arithmetic

#define SCALAR_ARITHMETIC_UNARY(NAME, REGISTRY_NAME, REGISTRY_CHECKED_NAME)            \
  Result<Datum> NAME(const Datum& arg, ArithmeticOptions options, ExecContext* ctx) {  \
    auto func_name = (options.check_overflow) ? REGISTRY_CHECKED_NAME : REGISTRY_NAME; \
    return CallFunction(func_name, {arg}, ctx);                                        \
  }

SCALAR_ARITHMETIC_UNARY(AbsoluteValue, "abs", "abs_checked")
SCALAR_ARITHMETIC_UNARY(Negate, "negate", "negate_checked")
SCALAR_EAGER_UNARY(Sign, "sign")
SCALAR_ARITHMETIC_UNARY(Sin, "sin", "sin_checked")
SCALAR_ARITHMETIC_UNARY(Cos, "cos", "cos_checked")
SCALAR_ARITHMETIC_UNARY(Asin, "asin", "asin_checked")
SCALAR_ARITHMETIC_UNARY(Acos, "acos", "acos_checked")
SCALAR_ARITHMETIC_UNARY(Tan, "tan", "tan_checked")
SCALAR_EAGER_UNARY(Atan, "atan")
SCALAR_ARITHMETIC_UNARY(Ln, "ln", "ln_checked")
SCALAR_ARITHMETIC_UNARY(Log10, "log10", "log10_checked")
SCALAR_ARITHMETIC_UNARY(Log2, "log2", "log2_checked")
SCALAR_ARITHMETIC_UNARY(Log1p, "log1p", "log1p_checked")

Result<Datum> Round(const Datum& arg, RoundOptions options, ExecContext* ctx) {
  return CallFunction("round", {arg}, &options, ctx);
}

Result<Datum> RoundToMultiple(const Datum& arg, RoundToMultipleOptions options,
                              ExecContext* ctx) {
  return CallFunction("round_to_multiple", {arg}, &options, ctx);
}

#define SCALAR_ARITHMETIC_BINARY(NAME, REGISTRY_NAME, REGISTRY_CHECKED_NAME)           \
  Result<Datum> NAME(const Datum& left, const Datum& right, ArithmeticOptions options, \
                     ExecContext* ctx) {                                               \
    auto func_name = (options.check_overflow) ? REGISTRY_CHECKED_NAME : REGISTRY_NAME; \
    return CallFunction(func_name, {left, right}, ctx);                                \
  }

SCALAR_ARITHMETIC_BINARY(Add, "add", "add_checked")
SCALAR_ARITHMETIC_BINARY(Subtract, "subtract", "subtract_checked")
SCALAR_ARITHMETIC_BINARY(Multiply, "multiply", "multiply_checked")
SCALAR_ARITHMETIC_BINARY(Divide, "divide", "divide_checked")
SCALAR_ARITHMETIC_BINARY(Power, "power", "power_checked")
SCALAR_ARITHMETIC_BINARY(ShiftLeft, "shift_left", "shift_left_checked")
SCALAR_ARITHMETIC_BINARY(ShiftRight, "shift_right", "shift_right_checked")
SCALAR_ARITHMETIC_BINARY(Logb, "logb", "logb_checked")
SCALAR_EAGER_BINARY(Atan2, "atan2")
SCALAR_EAGER_UNARY(Floor, "floor")
SCALAR_EAGER_UNARY(Ceil, "ceil")
SCALAR_EAGER_UNARY(Trunc, "trunc")

Result<Datum> MaxElementWise(const std::vector<Datum>& args,
                             ElementWiseAggregateOptions options, ExecContext* ctx) {
  return CallFunction("max_element_wise", args, &options, ctx);
}

Result<Datum> MinElementWise(const std::vector<Datum>& args,
                             ElementWiseAggregateOptions options, ExecContext* ctx) {
  return CallFunction("min_element_wise", args, &options, ctx);
}

// ----------------------------------------------------------------------
// Set-related operations

static Result<Datum> ExecSetLookup(const std::string& func_name, const Datum& data,
                                   const SetLookupOptions& options, ExecContext* ctx) {
  if (!options.value_set.is_arraylike()) {
    return Status::Invalid("Set lookup value set must be Array or ChunkedArray");
  }
  std::shared_ptr<DataType> data_type;
  if (data.type()->id() == Type::DICTIONARY) {
    data_type =
        arrow::internal::checked_pointer_cast<DictionaryType>(data.type())->value_type();
  } else {
    data_type = data.type();
  }

  if (options.value_set.length() > 0 && !data_type->Equals(options.value_set.type())) {
    std::stringstream ss;
    ss << "Array type didn't match type of values set: " << data_type->ToString()
       << " vs " << options.value_set.type()->ToString();
    return Status::Invalid(ss.str());
  }
  return CallFunction(func_name, {data}, &options, ctx);
}

Result<Datum> IsIn(const Datum& values, const SetLookupOptions& options,
                   ExecContext* ctx) {
  return ExecSetLookup("is_in", values, options, ctx);
}

Result<Datum> IsIn(const Datum& values, const Datum& value_set, ExecContext* ctx) {
  return ExecSetLookup("is_in", values, SetLookupOptions{value_set}, ctx);
}

Result<Datum> IndexIn(const Datum& values, const SetLookupOptions& options,
                      ExecContext* ctx) {
  return ExecSetLookup("index_in", values, options, ctx);
}

Result<Datum> IndexIn(const Datum& values, const Datum& value_set, ExecContext* ctx) {
  return ExecSetLookup("index_in", values, SetLookupOptions{value_set}, ctx);
}

// ----------------------------------------------------------------------
// Boolean functions

SCALAR_EAGER_UNARY(Invert, "invert")
SCALAR_EAGER_BINARY(And, "and")
SCALAR_EAGER_BINARY(KleeneAnd, "and_kleene")
SCALAR_EAGER_BINARY(Or, "or")
SCALAR_EAGER_BINARY(KleeneOr, "or_kleene")
SCALAR_EAGER_BINARY(Xor, "xor")
SCALAR_EAGER_BINARY(AndNot, "and_not")
SCALAR_EAGER_BINARY(KleeneAndNot, "and_not_kleene")

// ----------------------------------------------------------------------

Result<Datum> Compare(const Datum& left, const Datum& right, CompareOptions options,
                      ExecContext* ctx) {
  std::string func_name;
  switch (options.op) {
    case CompareOperator::EQUAL:
      func_name = "equal";
      break;
    case CompareOperator::NOT_EQUAL:
      func_name = "not_equal";
      break;
    case CompareOperator::GREATER:
      func_name = "greater";
      break;
    case CompareOperator::GREATER_EQUAL:
      func_name = "greater_equal";
      break;
    case CompareOperator::LESS:
      func_name = "less";
      break;
    case CompareOperator::LESS_EQUAL:
      func_name = "less_equal";
      break;
  }
  return CallFunction(func_name, {left, right}, nullptr, ctx);
}

SCALAR_EAGER_TERNARY(Between, "between")

// ----------------------------------------------------------------------
// Validity functions

SCALAR_EAGER_UNARY(IsValid, "is_valid")
SCALAR_EAGER_UNARY(IsNan, "is_nan")

Result<Datum> IfElse(const Datum& cond, const Datum& if_true, const Datum& if_false,
                     ExecContext* ctx) {
  return CallFunction("if_else", {cond, if_true, if_false}, ctx);
}

Result<Datum> CaseWhen(const Datum& cond, const std::vector<Datum>& cases,
                       ExecContext* ctx) {
  std::vector<Datum> args = {cond};
  args.reserve(cases.size() + 1);
  args.insert(args.end(), cases.begin(), cases.end());
  return CallFunction("case_when", args, ctx);
}

Result<Datum> IsNull(const Datum& arg, NullOptions options, ExecContext* ctx) {
  return CallFunction("is_null", {arg}, &options, ctx);
}

// ----------------------------------------------------------------------
// Temporal functions

SCALAR_EAGER_UNARY(Year, "year")
SCALAR_EAGER_UNARY(Month, "month")
SCALAR_EAGER_UNARY(Day, "day")
SCALAR_EAGER_UNARY(DayOfYear, "day_of_year")
SCALAR_EAGER_UNARY(ISOYear, "iso_year")
SCALAR_EAGER_UNARY(ISOWeek, "iso_week")
SCALAR_EAGER_UNARY(USWeek, "us_week")
SCALAR_EAGER_UNARY(ISOCalendar, "iso_calendar")
SCALAR_EAGER_UNARY(Quarter, "quarter")
SCALAR_EAGER_UNARY(Hour, "hour")
SCALAR_EAGER_UNARY(Minute, "minute")
SCALAR_EAGER_UNARY(Second, "second")
SCALAR_EAGER_UNARY(Millisecond, "millisecond")
SCALAR_EAGER_UNARY(Microsecond, "microsecond")
SCALAR_EAGER_UNARY(Nanosecond, "nanosecond")
SCALAR_EAGER_UNARY(Subsecond, "subsecond")

Result<Datum> DayOfWeek(const Datum& arg, DayOfWeekOptions options, ExecContext* ctx) {
  return CallFunction("day_of_week", {arg}, &options, ctx);
}

Result<Datum> AssumeTimezone(const Datum& arg, AssumeTimezoneOptions options,
                             ExecContext* ctx) {
  return CallFunction("assume_timezone", {arg}, &options, ctx);
}

Result<Datum> Week(const Datum& arg, WeekOptions options, ExecContext* ctx) {
  return CallFunction("week", {arg}, &options, ctx);
}

Result<Datum> Strftime(const Datum& arg, StrftimeOptions options, ExecContext* ctx) {
  return CallFunction("strftime", {arg}, &options, ctx);
}

}  // namespace compute
}  // namespace arrow

