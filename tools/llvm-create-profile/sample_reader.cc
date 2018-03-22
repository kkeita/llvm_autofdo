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

// Read the samples from the profile datafile.

#include "sample_reader.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <utility>
#include <inttypes.h>
#define DEBUG(x) {}
namespace {
    using namespace std;
// Returns true if name equals full_name, or full_name is empty and name
// matches re.
bool MatchBinary(const string &name, const string &full_name) {
    return full_name == basename(name.c_str());
}
}  // namespace

namespace autofdo {



uint64_t TextSampleReaderWriter::GetTotalSampleCount() const {
  uint64_t ret = 0;

  if (range_count_map_.size() > 0) {
    for (const auto &range_count : range_count_map_) {
      ret += range_count.second;
    }
  } else {
    for (const auto &addr_count : address_count_map_) {
      ret += addr_count.second;
    }
  }
  return ret;
}


bool TextSampleReaderWriter::readProfile() {

  FILE *fp = fopen(profileFile.c_str(), "r");
  if (fp == NULL) {
    llvm::errs() << "Cannot open " << profileFile << "to read";
    return false;
  }
  uint64_t num_records;

  // Reads in the rangeCountMap
  if (1 != fscanf(fp, "%llu\n", &num_records)) {
    llvm::errs() << "Error reading from " << profileFile;
    fclose(fp);
    return false;
  }
  for (int i = 0; i < num_records; i++) {
    uint64_t from, to, count;
    if (3 != fscanf(fp, "%llx-%llx:%llu\n", &from, &to, &count)) {
      llvm::errs() << "Error reading from " << profileFile;
      fclose(fp);
      return false;
    }

    range_count_map_[Range{InstructionLocation{objectFile,from},
                           InstructionLocation{objectFile,to}}] += count;
  }

  // Reads in the addr_count_map
  if (1 != fscanf(fp, "%llu\n", &num_records)) {
    llvm::errs() << "Error reading from " << profileFile;
    fclose(fp);
    return false;
  }
  for (int i = 0; i < num_records; i++) {
    uint64_t addr, count;
    if (2 != fscanf(fp, "%llx:%llu\n", &addr, &count)) {
      llvm::errs() << "Error reading from " << profileFile;
      fclose(fp);
      return false;
    }
    address_count_map_[InstructionLocation{objectFile,addr}] += count;
  }

  // Reads in the branchCountMap
  if (1 != fscanf(fp, "%llu\n", &num_records)) {
    llvm::errs() << "Error reading from " << profileFile;
    fclose(fp);
    return false;
  }
  for (int i = 0; i < num_records; i++) {
    uint64_t from, to, count;
    if (3 != fscanf(fp, "%llx->%llx:%llu\n", &from, &to, &count)) {
      llvm::errs() << "Error reading from " << profileFile;
      fclose(fp);
      return false;
    }
    branch_count_map_[Branch{InstructionLocation{objectFile,from},
                             InstructionLocation{objectFile,to}}] += count;
  }
  fclose(fp);

    total_count_ = 0;
  if (range_count_map_.size() > 0) {
    for (const auto &range_count : range_count_map_) {
      total_count_ += range_count.second * (range_count.first.begin.offset -
                                            range_count.first.end.offset);
    }
  } else {
    for (const auto &addr_count : address_count_map_) {
      total_count_ += addr_count.second;
    }
  }

    DEBUG(Dump(std::cout));
  return true;
}


    void TextSampleReaderWriter::Dump(std::ostream & out) {
        out << "Address count " << "\n";
        for (auto const & addr : address_count_map())
            out << std::hex << addr.first << std::dec << " : " << addr.second << std::endl ;

        out << "Range count " << std::endl ;
        for (auto const & range : range_count_map())
            out << std::hex << range.first << std::dec << " : " << range.second << std::endl;

        out << "Branch count " << std::endl ;
        for (auto const & branch : branch_count_map())
            out << std::hex << branch.first << std::dec << " : " << branch.second << std::endl;

    };

bool TextSampleReaderWriter::Write(const char *aux_info) {
  return false ;
  /*
  FILE *fp = fopen(profile_file_.c_str(), "w");
  if (fp == NULL) {
    llvm::errs() << "Cannot open " << profile_file_ << " to write";
    return false;
  }

  fprintf(fp, "%" PRIu64 "\n", range_count_map_.size());
  for (const auto &range_count : range_count_map_) {
    fprintf(fp, "%llx-%llx:%llu\n", range_count.first.first,
            range_count.first.second, range_count.second);
  }
  fprintf(fp, "%" PRIu64 "\n", address_count_map_.size());
  for (const auto &addr_count : address_count_map_) {
    fprintf(fp, "%llx:%llu\n", addr_count.first, addr_count.second);
  }
  fprintf(fp, "%" PRIu64 "\n", branch_count_map_.size());
  for (const auto &branch_count : branch_count_map_) {
    fprintf(fp, "%llx->%llx:%llu\n", branch_count.first.first,
            branch_count.first.second, branch_count.second);
  }
  if (aux_info) {
    fprintf(fp, "%s", aux_info);
  }
  fclose(fp);
  return true;
   */
}


}  // namespace autofdo
