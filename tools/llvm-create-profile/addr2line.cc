// Copyright 2014 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Class to derive inline stack.

#include "addr2line.h"

#include <string.h>

#include "symbol_map.h"



namespace autofdo {

Addr2line *Addr2line::Create(const string &binary_name) {
  return CreateWithSampledFunctions(binary_name, NULL);
}

Addr2line *Addr2line::CreateWithSampledFunctions(
    const string &binary_name,
    const std::map<uint64_t, uint64_t> *sampled_functions) {
  Addr2line *addr2line = new LLVMAddr2line(binary_name,sampled_functions);
  if (!addr2line->Prepare()) {
    delete addr2line;
    return NULL;
  } else {
    return addr2line;
  }
}


}  // namespace autofdo
