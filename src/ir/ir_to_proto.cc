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

#include <memory>
#include <variant>
#include "src/ir/ir_to_proto.h"
#include "src/ir/ssa_names.h"

namespace raksha::ir {

IRProto IRToProto::FoldResult(IRProto accumulator,
                  IRProto child_result) {
  if (std::holds_alternative<proto::Module>(accumulator)) {
    proto::BlockWithId* block_proto =
        std::get<proto::Module>(accumulator).mutable_blocks()->Add();
    *block_proto =
        std::get<proto::BlockWithId>(std::move(child_result));
  }
  if (std::holds_alternative<proto::BlockWithId>(accumulator)) {
    proto::OperationWithId* operation_proto =
        std::get<proto::BlockWithId>(accumulator).mutable_block()->mutable_operations()->Add();
    *operation_proto =
        std::get<proto::OperationWithId>(std::move(child_result));
  }
  return accumulator;
}

IRProto IRToProto::PreVisit(const Module& module) {
  proto::Module module_proto;
  return module_proto;
}

IRProto IRToProto::PreVisit(const Block& block) {
  proto::BlockWithId block_proto;
  // TODO(#619): Consider preserving inputs/outputs.
  // TODO(FILE ISSUE FOR TRAVERSAL): Traverse results.
  for (const std::pair<std::string, Value> result : block.results()) {
    block_proto.mutable_block()->mutable_results()->mutable_values()->insert(
        {result.first, ValueToProto(result.second)});
  }
  block_proto.set_id(GetID(block));
  return block_proto;
}

IRProto IRToProto::PreVisit(const Operation& operation) {
  proto::OperationWithId operation_proto;
  // TODO(#619): Consider preserving results and impl_module.
  operation_proto.set_id(GetID(operation));
  operation_proto.mutable_operation()->set_operator_name(std::string(operation.op().name()));

  // TODO(FILE ISSUE FOR TRAVERSAL): Traverse inputs.
  for (const auto& input : operation.inputs()) {
    operation_proto.mutable_operation()->mutable_inputs()->Add(ValueToProto(input));
  }
  // TODO(FILE ISSUE FOR TRAVERSAL): Traverse attributes.
  for (const auto& attribute : operation.attributes()) {
    operation_proto.mutable_operation()->mutable_attributes()->mutable_attributes()->insert(
        {attribute.first, AttributeToProto(attribute.second)});
  }
  return operation_proto;
}

proto::Value IRToProto::ValueToProto(const Value& value) {
  proto::Value value_proto;
  if (const auto ptr = value.If<value::Any>()) {
    (*value_proto.mutable_any_value()) = AnyToProto(*ptr);
  } else if (const auto ptr = value.If<value::OperationResult>()) {
    (*value_proto.mutable_operation_result_value()) =
        OperationResultToProto(*ptr);
  } else if (const auto ptr = value.If<value::BlockArgument>()) {
    (*value_proto.mutable_block_argument_value()) = BlockArgumentToProto(*ptr);
  } else {
    CHECK(false) << "Unexpected variant of Value from IR";
  }
  return value_proto;
}

proto::AnyValue IRToProto::AnyToProto(const value::Any&) {
  proto::AnyValue any_proto;
  // Any is empty.
  return any_proto;
}

proto::BlockArgumentValue IRToProto::BlockArgumentToProto(
    const value::BlockArgument& block_argument) {
  proto::BlockArgumentValue block_argument_proto;
  block_argument_proto.set_block_id(GetID(block_argument.element()));
  block_argument_proto.set_block_argument_name(
      std::string(block_argument.name()));
  return block_argument_proto;
}

proto::OperationResultValue IRToProto::OperationResultToProto(
    const value::OperationResult& operation_result) {
  SsaNames ssa_names;
  const auto id = GetID(operation_result.element());
  proto::OperationResultValue operation_result_value_proto;
  operation_result_value_proto.set_operation_id(id);
  operation_result_value_proto.set_output_name(
      std::string(operation_result.name()));
  return operation_result_value_proto;
}

proto::AttributePayload IRToProto::AttributeToProto(
    const Attribute& attribute) {
  proto::AttributePayload payload;
  const auto int_ptr = attribute.GetIf<Int64Attribute>();
  if (int_ptr != nullptr) {
    payload.set_int64_payload(int_ptr->value());
    return payload;
  }
  const auto str_ptr = attribute.GetIf<StringAttribute>();
  if (str_ptr == nullptr) {
    CHECK(false) << "Unexpected Attribute variant";
  }
  payload.set_string_payload(std::string(str_ptr->value()));
  return payload;
}

}  // namespace raksha::ir
