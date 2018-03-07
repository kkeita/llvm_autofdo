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

#ifndef AUTOFDO_SAMPLE_READER_H_
#define AUTOFDO_SAMPLE_READER_H_

#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>


namespace autofdo {
using namespace std;
// All counter type is using uint64_t instead of int64 because GCC's gcov
// functions only takes unsigned variables.
typedef map<uint64_t, uint64_t> AddressCountMap;
typedef pair<uint64_t, uint64_t> Range;
typedef map<Range, uint64_t> RangeCountMap;
typedef pair<uint64_t, uint64_t> Branch;
typedef map<Branch, uint64_t> BranchCountMap;

// Reads in the profile data, and represent it in address_count_map_.
class SampleReader {
 public:
  SampleReader() : total_count_(0) {}
  virtual ~SampleReader() {}

  bool ReadAndSetTotalCount();

  const AddressCountMap &address_count_map() const {
    return address_count_map_;
  }

  const RangeCountMap &range_count_map() const {
    return range_count_map_;
  }

  const BranchCountMap &branch_count_map() const {
    return branch_count_map_;
  }

  set<uint64_t> GetSampledAddresses() const;

  // Returns the sample count for a given instruction.
  uint64_t GetSampleCountOrZero(uint64_t addr) const;
  // Returns the total sampled count.
  uint64_t GetTotalSampleCount() const;
  // Returns the max count.
  uint64_t GetTotalCount() const {
    return total_count_;
  }
  // Clear all maps to release memory.
  void Clear() {
    address_count_map_.clear();
    range_count_map_.clear();
    branch_count_map_.clear();
  }

 protected:
  // Virtual read function to read from different types of profiles.
  virtual bool Read() = 0;

  uint64_t total_count_;
  AddressCountMap address_count_map_;
  RangeCountMap range_count_map_;
  BranchCountMap branch_count_map_;
};

// Base class that reads in the profile from a sample data file.
class FileSampleReader : public SampleReader {
 public:
  explicit FileSampleReader(const string &profile_file)
      : profile_file_(profile_file) {}

  virtual bool Append(const string &profile_file) = 0;

 protected:
  virtual bool Read();

  string profile_file_;
};

// Reads/Writes sample data from/to text file.
// The text file format:
//
// number of entries in range_count_map
// from_1-to_1:count_1
// from_2-to_2:count_2
// ......
// from_n-to_n:count_n
// number of entries in address_count_map
// addr_1:count_1
// addr_2:count_2
// ......
// addr_n:count_n
class TextSampleReaderWriter : public FileSampleReader {
 public:
  explicit TextSampleReaderWriter(const string &profile_file) :
      FileSampleReader(profile_file) { }
  explicit TextSampleReaderWriter() : FileSampleReader("") { }
  virtual bool Append(const string &profile_file);
  void Merge(const SampleReader &reader);
  // Writes the profile to file, and appending aux_info at the end.
  bool Write(const char *aux_info);
  bool IsFileExist() const;
  void IncAddress(uint64_t addr) {
    address_count_map_[addr]++;
  }
  void IncRange(uint64_t start, uint64_t end) {
    range_count_map_[Range(start, end)]++;
  }
  void IncBranch(uint64_t from, uint64_t to) {
    branch_count_map_[Branch(from, to)]++;
  }
  void set_profile_file(const string &file) {
    profile_file_ = file;
  }

 private:
};

}  // namespace autofdo

#endif  // AUTOFDO_SAMPLE_READER_H_
