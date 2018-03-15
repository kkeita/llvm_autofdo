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
  return CreateWithSampledFunctions();
}

Addr2line *Addr2line::CreateWithSampledFunctions() {
  Addr2line *addr2line = new LLVMAddr2line();
    return addr2line ;
}


}  // namespace autofdo
