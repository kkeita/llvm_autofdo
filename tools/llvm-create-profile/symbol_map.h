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

// Class to represent the symbol map. The symbol map is a map from
// symbol names to the symbol class.
// This class is thread-safe.

#ifndef AUTOFDO_SYMBOL_MAP_H_
#define AUTOFDO_SYMBOL_MAP_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "PerfSampleReader.h"
#include "InstructionSymbolizer.h"

#define DEBUG(x) {}
// Macros from gcc (profile.c)
//#define NUM_GCOV_WORKING_SETS 128
//#define WORKING_SET_INSN_PER_BB 10

namespace autofdo {
    using namespace std;
    using experimental::InstructionLocation;
    using experimental::Range ;
typedef std::map<std::string, uint64_t> CallTargetCountMap;
typedef std::pair<std::string, uint64_t> TargetCountPair;
typedef std::vector<TargetCountPair> TargetCountPairs;



// Returns a sorted vector of target_count pairs. target_counts is a pointer
// to an empty vector in which the output will be stored.
// Sorting is based on count in descending order.
void GetSortedTargetCountPairs(const CallTargetCountMap &call_target_count_map,
                               TargetCountPairs *target_counts);

// Represents profile information of a given source.
class ProfileInfo {
 public:
  ProfileInfo() : count(0), num_inst(0) {}
  ProfileInfo &operator+=(const ProfileInfo &other);

  uint64_t count;
  uint64_t num_inst;
  CallTargetCountMap target_map;
};



// Map from a source location (represented by offset+discriminator) to profile.
typedef std::map<uint32_t, ProfileInfo> PositionCountMap;

// callsite_location, callee_name
typedef std::pair<uint32_t, std::string> Callsite;

struct CallsiteLess {
  bool operator()(const Callsite& c1, const Callsite& c2) const {
    if (c1.first != c2.first)
      return c1.first < c2.first;
    //if ((c1.second == NULL || c2.second == NULL))
    //  return c1.second < c2.second;
    return   c1.second <  c2.second;
  }
};
class Symbol;
// Map from a callsite to the callee symbol.
typedef std::map<Callsite, Symbol *, CallsiteLess> CallsiteMap;

// Contains information about a specific symbol.
// There are two types of symbols:
// 1. Actual symbol: the symbol exists in the binary as a standalone function.
//                   It has the begin_address and end_address, and its name
//                   is always full assembler name.
// 2. Inlined symbol: the symbol is cloned in another function. It does not
//                    have the begin_address and end_address, and its name
//                    could be a short bfd_name.
class Symbol {
 public:
  // This constructor is used to create inlined symbol.
  Symbol(const  DILineInfo & info)
      : info(info),
        total_count(0), head_count(0) {}

  // This constructor is used to create aliased symbol.
  Symbol(const Symbol *src, const char *new_func_name)
      : info(src->info), total_count(src->total_count),
        head_count(src->head_count) {
    info.FunctionName = std::string(new_func_name);
  }

  Symbol() : total_count(0), head_count(0) {}

  ~Symbol();

  static string Name(const char *name) {
    return (name && strlen(name) > 0) ? name : "noname";
  }

    static string Name(std::string name) {
        return name != "" ? name : "noname" ;
    }
  string name() const {
    return Name(info.FunctionName.c_str());
  }

  // Merges profile stored in src symbol with this symbol.
  void Merge(const Symbol *src);

  // Returns the module name of the symbol. Module name is the source file
  // that the symbol belongs to. It is an attribute of the actual symbol.
  string ModuleName() const;

  // Returns true if the symbol is from a header file.
  bool IsFromHeader() const;

  // Dumps content of the symbol with a give indentation.
  void Dump(int indent) const;

  // Returns the max of pos_counts and callsites' pos_counts.
  uint64_t MaxPosCallsiteCount() const;

  // Source information about the the symbol (func_name, file_name, etc.)
  DILineInfo info;
  // The total sampled count.
  uint64_t total_count;
  // The total sampled count in the head bb.
  uint64_t head_count;
  // Map from callsite location to callee symbol.
  CallsiteMap callsites;
  // Map from source location to count and instruction number.
  PositionCountMap pos_counts;
};

// Maps function name to actual symbol. (Top level map).
typedef map<string, Symbol *> NameSymbolMap;
// Maps symbol's start address to its name and size.
typedef std::map<uint64_t, std::pair<string, uint64_t>> AddressSymbolMap;
// Maps from symbol's name to its start address.
typedef std::map<string, uint64_t> NameAddressMap;
// Maps function name to alias names.
typedef map<string, set<string> > NameAliasMap;

// SymbolMap stores the symbols in the binary, and maintains
// a map from symbol name to its related information.
class SymbolMap {
 public:
  explicit SymbolMap(const string &binary)
      : binary_(binary),
        base_addr_(0),
        count_threshold_(0),
        use_discriminator_encoding_(false) {
    if (!binary.empty()) {
      BuildSymbolMap();
      BuildNameAddressMap();
    }
  }

  SymbolMap()
      : base_addr_(0),
        count_threshold_(0),
        use_discriminator_encoding_(false) {}

  ~SymbolMap();

  uint64_t size() const {
    return map_.size();
  }

  void set_count_threshold(int64_t n) {count_threshold_ = n;}
  int64_t count_threshold() const {return count_threshold_;}

  // Returns true if the count is large enough to be emitted.
  bool ShouldEmit(int64_t count) const {
    return count > count_threshold_;
  }

  // Caculates sample threshold from given total count.
  void CalculateThresholdFromTotalCount(int64_t total_count);

  // Caculates sample threshold from symbol map.
  // All symbols should have been counted.
  void CalculateThreshold();

  // Returns relocation start address.
  uint64_t base_addr() const {
    return base_addr_;
  }

  void set_use_discriminator_encoding(bool v) {
    use_discriminator_encoding_ = v;
  }

  // Adds an empty named symbol.
  void AddSymbol(const string &name);

  const NameSymbolMap &map() const {
    return map_;
  }

  NameSymbolMap &map() {
    return map_;
  }
/*
  const gcov_working_set_info *GetWorkingSets() const {
    return working_set_;
  }
*/
  uint64_t GetSymbolStartAddr(const string &name) const {
    const auto &iter = name_addr_map_.find(name);
    if (iter == name_addr_map_.end()) {
      return 0;
    }
    return iter->second;
  }
/*
  void UpdateWorkingSet(int i, uint32_t num_counters, uint64_t min_counter) {
    if (working_set_[i].num_counters == 0) {
      working_set_[i].num_counters = num_counters;
    } else {
      // This path only happens during profile merge.
      // Different profiles will have similar num_counters, so calculating
      // average for each iteration will no lose much precision.
      working_set_[i].num_counters =
          (working_set_[i].num_counters + num_counters) / 2;
    }
    working_set_[i].min_counter += min_counter;
  }
*/
  const Symbol *GetSymbolByName(const string &name) const {
    NameSymbolMap::const_iterator ret = map_.find(name);
    if (ret != map_.end()) {
      return ret->second;
    } else {
      return NULL;
    }
  }

  // Merges symbols with suffixes like .isra, .part as a single symbol.
  void Merge();

  // Increments symbol's entry count.
  void AddSymbolEntryCount(const string &symbol, uint64_t count);

  typedef enum {INVALID = 1, SUM, MAX} Operation;
  // Increments source stack's count.
  //   symbol: name of the symbol in which source is located.
  //   source: source location (in terms of inlined source stack).
  //   count: total sampled count.
  //   num_inst: number of instructions that is mapped to the source.
  //   op: operation used to calculate count (SUM or MAX).
  void AddSourceCount(const string &symbol, const DIInliningInfo &source,
                      uint64_t count, uint64_t num_inst, Operation op);

  // Adds the indirect call target to source stack.
  //   symbol: name of the symbol in which source is located.
  //   source: source location (in terms of inlined source stack).
  //   target: indirect call target.
  //   count: total sampled count.
  void AddIndirectCallTarget(const string &symbol, const DIInliningInfo &source,
                             const string &target, uint64_t count);

  // Traverses the inline stack in source, update the symbol map by adding
  // count to the total count in the inlined symbol. Returns the leaf symbol.
  Symbol *TraverseInlineStack(const string &symbol, const DIInliningInfo &source,
                              uint64_t count);

  // Updates function name, start_addr, end_addr of a function that has a
  // given address. Returns false if no such symbol exists.
  const bool GetSymbolInfoByAddr(uint64_t addr, const string **name,
                                 uint64_t *start_addr, uint64_t *end_addr) const;

  // Returns a pointer to the symbol name for a given start address. Returns
  // NULL if no such symbol exists.
  const string *GetSymbolNameByStartAddr(const InstructionLocation& address) const;

  // Returns the overlap between two symbol maps. For two profiles, if
  // count_i_j denotes the function count of the ith function in profile j;
  // total_j denotes the total count of all functions in profile j. Then
  // overlap = sum(min(count_i_1/total_1, count_i_2/total_2))
  float Overlap(const SymbolMap &map) const;




  void Dump() const;
  void DumpFuncLevelProfileCompare(const SymbolMap &map) const;

  void AddAlias(const string& sym, const string& alias);

  // Validates if the current symbol map is sane.
  bool Validate() const;

 private:
  // Reads from the binary's elf section to build the symbol map.
  void BuildSymbolMap();

  // Reads from address_symbol_map_ and update name_addr_map_.
  void BuildNameAddressMap() {
    for (const auto &addr_symbol : address_symbol_map_) {
      name_addr_map_[addr_symbol.second.first] = addr_symbol.first;
    }
  }
public:
    void dumpaddressmap() {
        for (auto & addr : address_symbol_map_) {
            std::cout << std::hex << "offset : " << addr.first
                                 << ", name : "<<  addr.second.first <<
                                 std::dec << ", size : "<< addr.second.second
                                 << std::endl ;
        }
    }
    void dumpAliasMap(){
        for(auto & name : name_alias_map_) {
            std::cout << name.first  <<" : [";
            for (auto & alias : name.second){
                std::cout << alias << ", ";
            }
            std::cout << "] \n";
        }
    }
private:
  NameSymbolMap map_;
  NameAliasMap name_alias_map_;
  NameAddressMap name_addr_map_;
  AddressSymbolMap address_symbol_map_;
  const string binary_;
  uint64_t base_addr_;
  int64_t count_threshold_;
  bool use_discriminator_encoding_;
  /* working_set_[i] stores # of instructions that consumes
     i/NUM_GCOV_WORKING_SETS of total instruction counts.  */
  //gcov_working_set_info working_set_[NUM_GCOV_WORKING_SETS];

};
}  // namespace autofdo

#endif  // AUTOFDO_SYMBOL_MAP_H_
