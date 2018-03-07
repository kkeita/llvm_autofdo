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

#include "source_info.h"
#include <cstring>
#include <ostream>
namespace {
    using namespace std;
int StrcmpMaybeNull(const char *a, const char *b) {
  if (a == nullptr) {
    a = "";
  }
  if (b == nullptr) {
    b = "";
  }
  return strcmp(a, b);
}
}  // namespace

namespace autofdo {
bool SourceInfo::operator<(const SourceInfo &p) const {
  if (line != p.line) {
    return line < p.line;
  }
  if (start_line != p.start_line) {
    return start_line < p.start_line;
  }
  if (discriminator != p.discriminator) {
    return discriminator < p.discriminator;
  }
  int ret = StrcmpMaybeNull(func_name.c_str(), p.func_name.c_str());
  if (ret != 0) {
    return ret < 0;
  }
  ret = StrcmpMaybeNull(file_name.c_str(), p.file_name.c_str());
  if (ret != 0) {
    return ret < 0;
  }
  return StrcmpMaybeNull(dir_name.c_str(), p.dir_name.c_str()) < 0;
}
    bool SourceInfo::operator==(const SourceInfo &rhs) const {
        return func_name == rhs.func_name &&
             dir_name == rhs.dir_name &&
             file_name == rhs.file_name &&
             start_line == rhs.start_line &&
             line == rhs.line &&
             discriminator == rhs.discriminator;
    }

    bool SourceInfo::operator!=(const SourceInfo &rhs) const {
      return !(rhs == *this);
    }

    std::ostream &operator<<(std::ostream &os, const SourceInfo &info) {
      os << "func_name: " << info.func_name << " dir_name: " << info.dir_name << " file_name: " << info.file_name
         << " start_line: " << info.start_line << " line: " << info.line << " discriminator: " << info.discriminator
         ;
      return os;
    }
}  // namespace autofdo
