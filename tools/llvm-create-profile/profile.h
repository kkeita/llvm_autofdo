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

// Class to extract function level profile from binary level samples.

#ifndef AUTOFDO_PROFILE_H_
#define AUTOFDO_PROFILE_H_

#include <map>
#include <set>
#include <string>

#include "sample_reader.h"
#include "InstructionSymbolizer.h"

namespace autofdo {
using namespace std;
class Addr2line;
class SymbolMap;

// Class to convert instruction level profile to source level profile.
class Profile {
 public:
  // Arguments:
  //   sample_reader: the sample reader provides the instruction level profile,
  //                  i.e. map from instruction/callstack to profile counts.
  //   binary_name: the binary file name.
  //   addr2line: an Addr2line.
  //   symbol_map: the symbol map is written by this class to store all symbol
  //               information.
  Profile(const AbstractSampleReader *sample_reader, const string &binary_name,
          SymbolMap *symbol_map)
      : sample_reader_(sample_reader),
        symbol_map_(symbol_map) {}

  ~Profile();

  // Builds the source level profile.
  void ComputeProfile();

 private:
 InstructionSymbolizer symbolizer;
  // Internal data structure that aggregates profile for each symbol.
  struct ProfileMaps {
    ProfileMaps(InstructionLocation start, InstructionLocation end) : start_addr(start), end_addr(end) {}
    uint64_t GetAggregatedCount() const;
    InstructionLocation start_addr;
    InstructionLocation end_addr;
    AddressCountMap address_count_map;
    RangeCountMap range_count_map;
    BranchCountMap branch_count_map;
    };

  typedef map<string, ProfileMaps*> SymbolProfileMaps;

  // Returns the profile maps for a give function.
  ProfileMaps *GetProfileMaps(InstructionLocation addr);

  // Aggregates raw profile for each symbol.
  void AggregatePerFunctionProfile();

  // Builds function level profile for specified function:
  //   1. Traverses all instructions to build instruction map.
  //   2. Unwinds the inline stack to add symbol count to each inlined symbol.
  void ProcessPerFunctionProfile(string func_name, const ProfileMaps &map);

  const AbstractSampleReader *sample_reader_;
  SymbolMap *symbol_map_;
  AddressCountMap global_addr_count_map_;
  SymbolProfileMaps symbol_profile_maps_;
friend std::ostream & operator<<(std::ostream &os, const Profile::ProfileMaps & maps) {
InstructionSymbolizer symbolizer;
  //auto &maps = *this;
         os << std::hex << "start_addr: " << symbolizer.getVaddressFromFileOffset(maps.start_addr)
           << " end_addr: " << symbolizer.getVaddressFromFileOffset(maps.end_addr) <<std::dec <<  std::endl;

        os << " address_count_map: "  << maps.address_count_map.size() << std::endl ;
        for (auto const & addr : maps.address_count_map) {
            os << std::hex << symbolizer.getVaddressFromFileOffset(addr.first) << std::dec << ":" << addr.second <<std::endl;
        }


        os << " range_count_map: "  << maps.range_count_map.size() << std::endl ;
        for (auto const & range : maps.range_count_map) {
            os << std::hex << symbolizer.getVaddressFromFileOffset(range.first.begin)<< "-" << symbolizer.getVaddressFromFileOffset(range.first.end) << std::dec
               << ":" << range.second <<std::endl;
        }


        os << " branch_count_map: "  << maps.branch_count_map.size() << std::endl ;
        for (auto const & branch : maps.branch_count_map) {
            os << std::hex << symbolizer.getVaddressFromFileOffset(branch.first.instruction) << "->" << symbolizer.getVaddressFromFileOffset(branch.first.target) << std::dec
               << ":" << branch.second <<std::endl;
        }
        return os;
    }


  //DISALLOW_COPY_AND_ASSIGN(Profile);
};
}  // namespace autofdo

#endif  // AUTOFDO_PROFILE_H_
