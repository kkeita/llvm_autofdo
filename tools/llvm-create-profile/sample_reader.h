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
#include "PerfSampleReader.h"


namespace autofdo {

using namespace std;
using experimental::InstructionLocation;
using experimental::Range;
    using experimental::Branch;
// All counter type is using uint64_t instead of int64 because GCC's gcov
// functions only takes unsigned variables.
typedef map<InstructionLocation, uint64_t> AddressCountMap;
typedef map<Range, uint64_t> RangeCountMap;
typedef map<Branch, uint64_t> BranchCountMap;



    class AbstractSampleReader {
    public:
        ~AbstractSampleReader() {};
        virtual const AddressCountMap &address_count_map() const =0;
        virtual const RangeCountMap &range_count_map() const =0;
        virtual const BranchCountMap &branch_count_map() const =0;
        virtual bool readProfile() =0;
        //virtual bool ReadAndSetTotalCount();
        // Returns the total sampled count.
        virtual uint64_t GetTotalSampleCount() const =0;
        // Returns the max count.
        virtual uint64_t GetTotalCount() const = 0;
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
class TextSampleReaderWriter : public AbstractSampleReader {
 public:
  explicit TextSampleReaderWriter(const string &profile_file,const std::string objectFile) :
          objectFile(objectFile),
          profileFile(profile_file){ };
    virtual bool readProfile() override;

    const AddressCountMap &address_count_map() const {
      return address_count_map_;
    }

    const RangeCountMap &range_count_map() const {
      return range_count_map_;
    }

    const BranchCountMap &branch_count_map() const {
      return branch_count_map_;
    }

    std::set<InstructionLocation> GetSampledAddresses() const;

    // Returns the sample count for a given instruction.
    uint64_t GetSampleCountOrZero(uint64_t addr) const;
    // Returns the total sampled count.
    uint64_t GetTotalSampleCount() const;
    // Returns the max count.
    uint64_t GetTotalCount() const override  {
      return total_count_;
    }
  // Writes the profile to file, and appending aux_info at the end.
  bool Write(const char *aux_info);

 private:
    void Dump(std::ostream & out);
    uint64_t total_count_;
    std::string objectFile;
    std::string profileFile;
    AddressCountMap address_count_map_;
    RangeCountMap range_count_map_;
    BranchCountMap branch_count_map_;
};

}  // namespace autofdo

#endif  // AUTOFDO_SAMPLE_READER_H_
