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

// Class to represent source level profile.

#include "profile.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "symbol_map.h"
#include "sample_reader.h"
#include "llvm/Support/CommandLine.h"
#include <sstream>
#include <fstream>
#define DEBUG(x) {};
llvm::cl::opt<bool> UseLbr("use-lbr",llvm::cl::desc("Whether to use lbr profile."),
                     llvm::cl::init(true));

namespace autofdo {
    using namespace std;
Profile::ProfileMaps *Profile::GetProfileMaps(InstructionLocation addr) {
  const string *name ;
  uint64_t start_addr, end_addr;
  if (symbol_map_->GetSymbolInfoByAddr(addr.offset, &name,
                                       &start_addr, &end_addr)) {
      DEBUG(std::cout << std::hex << addr << ", resolves to function " << *name << ", starts " << start_addr << ", ends " << end_addr << std::dec << std::endl);
      std::pair<SymbolProfileMaps::iterator, bool> ret =
        symbol_profile_maps_.insert(SymbolProfileMaps::value_type(*name, NULL));
    if (ret.second) {
      ret.first->second = new ProfileMaps(InstructionLocation{addr.objectFile, start_addr }
                                         ,InstructionLocation{addr.objectFile, end_addr});
    }
    return ret.first->second;
  } else {
      DEBUG(std::cout << std::hex << addr << " unresolved " << std::dec << std::endl);

      return NULL;
  }
}

void Profile::AggregatePerFunctionProfile() {
    DEBUG(symbol_map_->dumpaddressmap());

    const AddressCountMap *count_map = &sample_reader_->address_count_map();
  for (const auto &addr_count : *count_map) {
    ProfileMaps *maps = GetProfileMaps(addr_count.first);
    if (maps != NULL) {
      maps->address_count_map[addr_count.first] += addr_count.second;
    }
  }
  const RangeCountMap *range_map = &sample_reader_->range_count_map();
  for (const auto &range_count : *range_map) {
    ProfileMaps *maps = GetProfileMaps(range_count.first.begin);
    if (maps != NULL) {
      maps->range_count_map[range_count.first] += range_count.second;
    }
  }
  const BranchCountMap *branch_map = &sample_reader_->branch_count_map();
  for (const auto &branch_count : *branch_map) {
    ProfileMaps *maps = GetProfileMaps(branch_count.first.instruction);
    if (maps != NULL) {
      maps->branch_count_map[branch_count.first] += branch_count.second;
    }
  }
}

uint64_t Profile::ProfileMaps::GetAggregatedCount() const {
  uint64_t ret = 0;

  if (range_count_map.size() > 0) {
    for (const auto &range_count : range_count_map) {
      ret += range_count.second * (range_count.first.end - range_count.first.begin);
    }
  } else {
    for (const auto &addr_count : address_count_map) {
      ret += addr_count.second;
    }
  }
  return ret;
}

void Profile::ProcessPerFunctionProfile(string func_name,
                                        const ProfileMaps &maps) {
  //std::ofstream("./create_llvm_prof/"+funct_name);
  std::stringstream out2;
  out2 << "Maps for " << func_name << std::endl;
  out2 << maps << std::endl ;

  AddressCountMap map;
  std::stringstream out ;
  std::stringstream out3;
  out << "function : " << func_name << " -- "
    << std::hex << symbolizer.getVaddressFromFileOffset(maps.start_addr) << " : " << symbolizer.getVaddressFromFileOffset(maps.end_addr) << std::dec << std::endl ;
  const AddressCountMap *map_ptr;
  if (UseLbr) {
    if (maps.range_count_map.size() == 0) {
      return;
    }
    for (const auto &range_count : maps.range_count_map) {
        DEBUG(std::cout << range_count.first << std::endl);
      for (InstructionLocation loc = range_count.first.begin ; loc <= range_count.first.end;++loc) {
          DEBUG(std::cout << loc << std::endl) ;
        map[loc] += range_count.second;
      }
    }
    map_ptr = &map;
  } else {
    map_ptr = &maps.address_count_map;
  }
  out3 << "-addresses-" << std::endl ;
  for (const auto &address_count : *map_ptr) {
    out3 << std::hex << symbolizer.getVaddressFromFileOffset(address_count.first) << std::dec ;
    out3 << " --> " << address_count.second  << std::endl ;
    auto &sourceInfo = symbolizer.symbolizeInstruction(address_count.first);

    if (!sourceInfo) {
        DEBUG(std::cout << "Should never happen" << std::endl) ;
      continue;
    }


    if (sourceInfo.get().getNumberOfFrames() > 0) {
      auto duplicationFactor = InstructionSymbolizer::getDuplicationFactor(
              sourceInfo.get().getFrame(0));
      out << std::hex << symbolizer.getVaddressFromFileOffset(address_count.first) << std::dec ;
      out << " --> " ; symbolizer.print(sourceInfo.get(),out);
      out << " --> " << address_count.second * duplicationFactor  << std::endl ;
      out << " --> " << symbolizer.Offset(sourceInfo.get().getFrame(0)) << std::endl;
      symbol_map_->AddSourceCount(
          func_name, sourceInfo.get(),
          address_count.second * duplicationFactor, 0,
          SymbolMap::MAX);
    }
  }
  out  << "-branches-" << std::endl ;
  for (const auto &branch_count : maps.branch_count_map) {
    DEBUG(std::cout << "Processing : " << std::hex << branch_count.first << std::dec << std::endl);
      auto &instInfo = symbolizer.symbolizeInstruction(branch_count.first.instruction);

    if (!instInfo) {
      DEBUG(std::cout << "Missing Instruction in branch " << std::endl);
      continue;
    }

    DEBUG(std::cout << "Found branch target" << std:: hex << branch_count.first << std::dec <<std::endl) ;

    const string *callee = symbol_map_->GetSymbolNameByStartAddr(branch_count.first.target);
    if (!callee) {
        DEBUG(std::cout << "No callee found" << std::hex << branch_count.first.target << std::dec <<std::endl) ;
      continue;
    }
    if (symbol_map_->map().count(*callee)) {
      out << std::hex << symbolizer.getVaddressFromFileOffset(branch_count.first.instruction) << std::dec;
      /*<< " --> " ;
      symbolizer.print(instInfo.get(),out);*/
      out << " --> " << branch_count.second  << std::endl ;

      symbol_map_->AddSymbolEntryCount(*callee, branch_count.second);
      symbol_map_->AddIndirectCallTarget(func_name, instInfo.get(), *callee, branch_count.second);
    }
  }

  for (const auto &addr_count : *map_ptr) {
    global_addr_count_map_[addr_count.first] = addr_count.second;
  }
     std::ofstream outfile("./llvm_create_prof_file/"+func_name);
        outfile << out3.rdbuf();
           outfile.close();
}

void Profile::ComputeProfile() {
  symbol_map_->CalculateThresholdFromTotalCount(sample_reader_->GetTotalCount());
  AggregatePerFunctionProfile();

  // First add all symbols that needs to be outputted to the symbol_map_. We
  // need to do this before hand because ProcessPerFunctionProfile will call
  // AddSymbolEntryCount for other symbols, which may or may not had been
  // processed by ProcessPerFunctionProfile.
  for (const auto &symbol_profile : symbol_profile_maps_) {
    //std::cout << "Symbol " << symbol_profile.first << " : " << symbol_profile.second->GetAggregatedCount();
    if (symbol_map_->ShouldEmit(symbol_profile.second->GetAggregatedCount())){
      symbol_map_->AddSymbol(symbol_profile.first);
      //std::cout << " added " << std::endl ;
    } else {
       //std::cout << "skipped" << std::endl ;
    }
  }

  // Traverse the symbol map to process the profiles.
  for (const auto &symbol_profile : symbol_profile_maps_) {
    if (symbol_map_->ShouldEmit(symbol_profile.second->GetAggregatedCount()))
      ProcessPerFunctionProfile(symbol_profile.first, *symbol_profile.second);
  }
  symbol_map_->Merge();
  //symbol_map_->ComputeWorkingSets();
}

Profile::~Profile() {
  for (auto &symbol_maps : symbol_profile_maps_) {
    delete symbol_maps.second;
  }
}
}  // namespace autofdo
