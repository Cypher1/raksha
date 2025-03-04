/*
 * Copyright 2022 Google LLC.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_IR_AUTH_LOGIC_SOUFFLE_EMITTER_H_
#define SRC_IR_AUTH_LOGIC_SOUFFLE_EMITTER_H_

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "src/ir/datalog/program.h"
#include "src/utils/map_iter.h"


namespace raksha::ir::auth_logic {


class SouffleEmitter {
 public:
  static std::string EmitProgram(const datalog::DLIRProgram& program) {
    SouffleEmitter emitter;
    std::string body = emitter.EmitProgramBody(program);
    std::string outputs = emitter.EmitOutputs(program);
    std::string decls = emitter.EmitDeclarations();
    return absl::StrCat(std::move(body), "\n", std::move(outputs), "\n",
                        std::move(decls), "\n");
  }

 private:
  SouffleEmitter() = default;

  // This function produces a normalized version of predicates to
  // be used when generating declarations of the predicates. It replaces
  // argument names with generic numbered ones. It is applied
  // to predicates before they are added to the set of declarations so that
  // there are no duplicate delcarations (which would otherwise happen
  // whenever a predicate is referenced more than once with different
  // arguments).
  // To prevent instances of negated and non-negated uses of the predicate
  // from generating two declarations, the sign here is always positive.
  datalog::Predicate PredToDeclaration(const datalog::Predicate& predicate) {
    int i = 0;
    return datalog::Predicate(predicate.name(),
                     utils::MapIter<std::string, std::string>(
                         predicate.args(),
                         [i](const std::string& arg) mutable {
                           return absl::StrCat("x", std::to_string(i++));
                         }),
                     datalog::kPositive);
  }

  std::string EmitPredicate(const datalog::Predicate& predicate) {
    // Whenever a new predicate is encountered, it is added to the set of
    // declarations (which does not include duplicates because it is a set).
    declarations_.insert(PredToDeclaration(predicate));
    return absl::StrCat(predicate.sign() == datalog::kNegated ? "!" : "",
                        predicate.name(), "(",
                        absl::StrJoin(predicate.args(), ", "), ")");
  }

  std::string EmitAssertionInner(const datalog::Predicate& predicate) {
    return absl::StrCat(EmitPredicate(predicate), ".");
  }

  std::string EmitAssertionInner(const datalog::DLIRCondAssertion& cond_assertion) {
    std::vector rhs_translated = utils::MapIter<datalog::Predicate, std::string>(
        cond_assertion.rhs(),
        [this](const datalog::Predicate& arg) { return EmitPredicate(arg); });
    return absl::StrCat(EmitPredicate(cond_assertion.lhs()), " :- ",
                        absl::StrJoin(rhs_translated, ", "), ".");
  }

  std::string EmitAssertion(const datalog::DLIRAssertion& assertion) {
    return std::visit([this](auto value) { return EmitAssertionInner(value); },
                      assertion.GetValue());
  }

  std::string EmitProgramBody(const datalog::DLIRProgram& program) {
    return absl::StrJoin(
        utils::MapIter<datalog::DLIRAssertion, std::string>(
            program.assertions(),
            [this](const datalog::DLIRAssertion& astn) { return EmitAssertion(astn); }),
        "\n");
  }

  std::string EmitOutputs(const datalog::DLIRProgram& program) {
    return absl::StrJoin(program.outputs(), "\n",
                         [](std::string* out, const std::string& prog_out) {
                           return absl::StrAppend(out, ".output ", prog_out);
                         });
  }

  std::string EmitDeclaration(const datalog::Predicate& predicate) {
    std::string arguments = absl::StrJoin(
        predicate.args(), ", ", [](std::string* out, const std::string& arg) {
          return absl::StrAppend(out, arg, ": symbol");
        });
    return absl::StrCat(".decl ", predicate.name(), "(", arguments, ")");
  }

  std::string EmitDeclarations() {
    std::vector elements_vec(std::make_move_iterator(declarations_.begin()),
                             std::make_move_iterator(declarations_.end()));
    std::sort(elements_vec.begin(), elements_vec.end());
    return absl::StrJoin(elements_vec, "\n",
                         [this](std::string* out, auto decl) {
                           return absl::StrAppend(out, EmitDeclaration(decl));
                         });
  }

  absl::flat_hash_set<datalog::Predicate> declarations_;
};

}  // namespace raksha::ir::auth_logic

#endif  // SRC_IR_AUTH_LOGIC_SOUFFLE_EMITTER_H_
