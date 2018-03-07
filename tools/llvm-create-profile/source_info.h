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

// Class to represent the source info.

#ifndef AUTOFDO_SOURCE_INFO_H_
#define AUTOFDO_SOURCE_INFO_H_

#include <vector>
#include <assert.h>
#include <iostream>

#include "config.h"

#include "llvm/IR/DebugInfoMetadata.h"

namespace autofdo {
using namespace std ;
// Represents the source position.
struct SourceInfo {
  SourceInfo()
      : func_name(),
        dir_name(),
        file_name(),
        start_line(0),
        line(0),
        discriminator(0) {}

  SourceInfo(const char *func_name, const char *dir_name,
             const char *file_name, uint32_t start_line, uint32_t line,
             uint32_t discriminator)
      : func_name(func_name == nullptr ? "" :func_name),
        dir_name(dir_name == nullptr ? "" : dir_name),
        file_name(file_name == nullptr ? "" : file_name),
        start_line(start_line),
        line(line),
        discriminator(discriminator) {}

  bool operator<(const SourceInfo &p) const;

    bool operator==(const SourceInfo &rhs) const;

    bool operator!=(const SourceInfo &rhs) const;

    friend ostream &operator<<(ostream &os, const SourceInfo &info);

    string RelativePath() const {
    if (!dir_name.empty())
      return string(dir_name) + "/" + string(file_name);
    if (!file_name.empty())
      return string(file_name);
    return string();
  }

  uint32_t Offset(bool use_discriminator_encoding) const {
    return ((line - start_line) << 16) |
           (use_discriminator_encoding
                ? llvm::DILocation::getBaseDiscriminatorFromDiscriminator(
                      discriminator)
                : discriminator);
  }

  uint32_t DuplicationFactor() const {
    return llvm::DILocation::getDuplicationFactorFromDiscriminator(
        discriminator);
  }

  std::string func_name;
  std::string dir_name;
  std::string file_name;
  uint32_t start_line;
  uint32_t line;
  uint32_t discriminator;
    uint64_t addr;
};

typedef vector<SourceInfo> SourceStack;
}  // namespace autofdo

#endif  // AUTOFDO_SOURCE_INFO_H_
