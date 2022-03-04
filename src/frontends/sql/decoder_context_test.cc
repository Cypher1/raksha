//-----------------------------------------------------------------------------
// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//----------------------------------------------------------------------------

#include "src/frontends/sql/decoder_context.h"

#include <string>

#include "src/common/testing/gtest.h"

namespace raksha::frontends::sql {

using testing::Combine;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;
using testing::Pair;
using testing::ResultOf;
using testing::TestWithParam;
using testing::UnorderedElementsAre;
using testing::Values;
using testing::ValuesIn;

using ir::Attribute;
using ir::Block;
using ir::IRContext;
using ir::NamedValueMap;
using ir::Operation;
using ir::Operator;
using ir::Storage;
using ir::Value;
using ir::types::TypeBase;
using ir::value::Any;
using ir::value::BlockArgument;
using ir::value::OperationResult;
using ir::value::StoredValue;

TEST(DecoderContextTest, BuildTopLevelBlock) {
  IRContext context;
  DecoderContext decoder_context(context);

  const Block &block = decoder_context.BuildTopLevelBlock();
  EXPECT_EQ(block.parent_module(), &decoder_context.global_module());
}

TEST(DecoderContextTest, GetOrCreateStorageIdempotence) {
  IRContext context;
  DecoderContext decoder_context(context);
  const Storage &store1 = decoder_context.GetOrCreateStorage("Table1");
  const Storage &store2 = decoder_context.GetOrCreateStorage("Disk1");
  const Storage &store1_again = decoder_context.GetOrCreateStorage("Table1");
  const Storage &store2_again = decoder_context.GetOrCreateStorage("Disk1");

  EXPECT_EQ(&store1, &store1_again);
  EXPECT_EQ(&store2, &store2_again);
  EXPECT_NE(&store1, &store2);

  EXPECT_EQ(store1.type().type_base().kind(), TypeBase::Kind::kPrimitive);
  EXPECT_EQ(store2.type().type_base().kind(), TypeBase::Kind::kPrimitive);
}

class LiteralOpTest : public TestWithParam<absl::string_view> {};

TEST_P(LiteralOpTest, MakeLiteralOperationTest) {
  IRContext context;
  DecoderContext decoder_context(context);

  absl::string_view literal_str = GetParam();
  const Operation &lit_op = decoder_context.MakeLiteralOperation(literal_str);

  const Block &top_level_block = decoder_context.BuildTopLevelBlock();

  EXPECT_THAT(lit_op.parent(), &top_level_block);
  EXPECT_THAT(lit_op.impl_module(), IsNull());
  EXPECT_THAT(lit_op.inputs(), IsEmpty());
  EXPECT_EQ(lit_op.op().name(), DecoderContext::kSqlLiteralOpName);

  // Check that "attributes" is exactly "literal_str" associated with the
  // parameter value.
  EXPECT_THAT(
      lit_op.attributes(),
      UnorderedElementsAre(
          Pair(DecoderContext::kLiteralStrAttrName,
               ResultOf([](const Attribute &attr) { return attr->ToString(); },
                        Eq(literal_str)))));
}

INSTANTIATE_TEST_SUITE_P(LiteralOpTest, LiteralOpTest,
                         Values("val1", "1000", "3.1415", "true", ""));

TEST(DecoderContextTest, DecoderContextRegisterAndGetValueTest) {
  IRContext ir_context;
  DecoderContext decoder_context(ir_context);

  Storage storage("storage1", ir_context.type_factory().MakePrimitiveType());

  Value value1{Any()};
  decoder_context.RegisterValue(1, value1);
  EXPECT_THAT(value1.If<Any>(), NotNull());
  Value value2{StoredValue(storage)};
  decoder_context.RegisterValue(2, value2);
  Value get_value1 = decoder_context.GetValue(1);
  Value get_value2 = decoder_context.GetValue(2);
  EXPECT_EQ(get_value1, value1);
  EXPECT_EQ(get_value2, value2);
}

TEST(DecoderContextDeathTest, DecoderContextValueRegistryDeathTest) {
  IRContext ir_context;
  DecoderContext decoder_context(ir_context);

  decoder_context.RegisterValue(1, Value(Any()));
  decoder_context.RegisterValue(2, Value(Any()));

  EXPECT_DEATH({ decoder_context.RegisterValue(1, Value(Any())); },
               "id_to_value map has more than one value associated with the id "
               "1.");
  EXPECT_DEATH({ decoder_context.RegisterValue(2, Value(Any())); },
               "id_to_value map has more than one value associated with the id "
               "2.");
  EXPECT_DEATH({ decoder_context.GetValue(3); },
               "Could not find a value with id 3.");
}

class DecodeMergeOpTest
    : public TestWithParam<std::tuple<std::vector<Value>, std::vector<Value>>> {
};

static bool BeginsWith(absl::string_view str1, absl::string_view str2) {
  if (str2.size() > str1.size()) return false;
  return str1.substr(0, str2.size()) == str2;
}

static std::optional<uint64_t> ExtractIdxAfterPrefix(absl::string_view str,
                                                     absl::string_view prefix) {
  if (!BeginsWith(str, prefix)) return std::nullopt;
  uint64_t result = 0;
  std::string index_str(str.substr(prefix.size()));
  EXPECT_NO_THROW({ result = std::stoul(index_str); });
  return result;
}

TEST_P(DecodeMergeOpTest, DecodeMergeOpTest) {
  auto &[direct_inputs, control_inputs] = GetParam();
  IRContext ir_context;
  DecoderContext decoder_context(ir_context);

  const Operation &op =
      decoder_context.MakeMergeOperation(direct_inputs, control_inputs);
  const Block &top_block = decoder_context.BuildTopLevelBlock();
  EXPECT_EQ(op.op().name(), DecoderContext::kSqlMergeOpName);
  EXPECT_EQ(op.parent(), &top_block);
  EXPECT_THAT(op.attributes(), IsEmpty());

  // Reconstruct the original direct and control input vectors from the map.
  // To do this, we start with two vectors filled entirely with dummy
  // `Operator` values, each of which is the size of the whole map. As we
  // traverse through the unordered map, we can then overwrite the vector
  // at the index indicated by the key with the value from the map. We then
  // resize each vector to the number of values inserted and compare with the
  // original vectors.
  Operator dummy_operator("dummy");
  Operation dummy_operation(nullptr, dummy_operator, {}, {});
  Value dummy_operation_result(OperationResult(dummy_operation, "dummy"));
  const NamedValueMap &found_input_map = op.inputs();

  std::vector found_direct_values(found_input_map.size(),
                                  dummy_operation_result);
  std::vector found_control_values(found_input_map.size(),
                                   dummy_operation_result);

  uint64_t num_direct_inputs_found = 0;
  uint64_t num_control_inputs_found = 0;
  for (auto &[key, value] : found_input_map) {
    if (std::optional<uint64_t> direct_idx = ExtractIdxAfterPrefix(
            key, DecoderContext::kMergeOpDirectInputPrefix)) {
      found_direct_values.at(*direct_idx) = value;
      ++num_direct_inputs_found;
    } else if (std::optional<uint64_t> control_idx = ExtractIdxAfterPrefix(
                   key, DecoderContext::kMergeOpControlInputPrefix)) {
      found_control_values.at(*control_idx) = value;
      ++num_control_inputs_found;
    } else {
      EXPECT_TRUE(false) << "Found a key that did not match either the direct"
                            " or control input prefix.";
    }
  }

  // Lop off any dummy values at the end of the vectors.
  found_direct_values.resize(num_direct_inputs_found, dummy_operation_result);
  found_control_values.resize(num_control_inputs_found, dummy_operation_result);

  // Test the vectors for equivalence.
  EXPECT_EQ(found_direct_values, direct_inputs);
  EXPECT_EQ(found_control_values, control_inputs);
}

// Create some example values for use in test inputs. Skip `StoredValue`
// because it's hard to construct and the actual kinds of values aren't
// as important as their indices for this test anyway.
static const Operator kExampleOp("example");
static const Operation kExampleOperation(nullptr, kExampleOp, {}, {});

static const Block kExampleBlock;

static const std::vector<Value> kSampleInputVectors[] = {
    std::vector<Value>{},
    {Value(Any())},
    {Value(OperationResult(kExampleOperation, "out")),
     Value(BlockArgument(kExampleBlock, "arg0")), Value(Any())}};

INSTANTIATE_TEST_SUITE_P(DecodeMergeOpTest, DecodeMergeOpTest,
                         Combine(ValuesIn(kSampleInputVectors),
                                 ValuesIn(kSampleInputVectors)));

}  // namespace raksha::frontends::sql
