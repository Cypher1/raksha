//-----------------------------------------------------------------------------
// Copyright 2021 Google LLC
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
//-----------------------------------------------------------------------------
#include "src/ir/proto/primitive_type.h"

#include "src/ir/types/primitive_type.h"
#include "src/ir/types/type_factory.h"

namespace raksha::ir::types::proto {

Type decode(TypeFactory& type_factory,
            const arcs::PrimitiveTypeProto& primitive_type_proto) {
  return type_factory.MakePrimitiveType();
}

arcs::PrimitiveTypeProto encode(const PrimitiveType& primitive_type) {
  // For now, just set all primitives to TEXT when serializing.
  return arcs::PrimitiveTypeProto::TEXT;
}

arcs::TypeProto encodeAsTypeProto(const PrimitiveType& primitive_type) {
  arcs::TypeProto type_proto;
  type_proto.set_primitive(encode(primitive_type));
  return type_proto;
}

}  // namespace raksha::ir::types::proto
