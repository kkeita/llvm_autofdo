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

// Class to represent the symbol map.

#include <algorithm>
#include <map>
#include <set>

#include "addr2line.h"
#include "symbol_map.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include <iostream>

llvm::cl::opt<int> DumpCutoffPercent ("dump_cutoff_percent",
                                      llvm::cl::desc("functions that has total count lower than this percentage of"
                                                             "the max function count will not show in the dump"),
                                      llvm::cl::init(2));
llvm::cl::opt<double>
        SampleThresholdFrac ("sample_threshold_frac",
                             llvm::cl::desc("Sample threshold ratio. The threshold of total function cout is determined by max_sample_count * sample_threshold_frac.")
                             ,llvm::cl::init(0.000005));

namespace {
using namespace std ;
// Returns whether str ends with suffix.
inline bool HasSuffixString(const string &str,
                            const string &suffix) {
  uint32_t len = suffix.size();
  uint32_t str_len = str.size();
  if (str_len <= len) {
    return false;
  }
  return str.substr(str_len - len, len) == suffix;
}

string GetOriginalName(const char *name) {
  const char *split = strchr(name, '.');
  if (split) {
    return string(name, split - name);
  } else {
    return string(name);
  }
}

// Prints some blank space for identation.
void Identation(int ident) {
  for (int i = 0; i < ident; i++) {
    printf(" ");
  }
}

void PrintSourceLocation(uint32_t start_line, uint32_t offset, int ident) {
  Identation(ident);
  if (offset & 0xffff) {
    printf("%u.%u: ", (offset >> 16) + start_line, offset & 0xffff);
  } else {
    printf("%u: ", (offset >> 16) + start_line);
  }
}
}  // namespace

namespace autofdo {
ProfileInfo& ProfileInfo::operator+=(const ProfileInfo &s) {
  count += s.count;
  num_inst += s.num_inst;
  for (const auto &target_count : s.target_map) {
    target_map[target_count.first] += target_count.second;
  }
  return *this;
}

struct TargetCountCompare {
  bool operator()(const TargetCountPair &t1, const TargetCountPair &t2) const {
    if (t1.second != t2.second) {
      return t1.second > t2.second;
    } else {
      return t1.first > t2.first;
    }
  }
};

void GetSortedTargetCountPairs(const CallTargetCountMap &call_target_count_map,
                               TargetCountPairs *target_counts) {
  for (const auto &name_count : call_target_count_map) {
    target_counts->push_back(name_count);
  }
  std::sort(target_counts->begin(), target_counts->end(), TargetCountCompare());
}

SymbolMap::~SymbolMap() {
  // Different keys (function names) may map to a same symbol.
  // In order to prevent double free, we first merge all symbols
  // into a set, then remove every symbol from the set.
  set<Symbol *> delete_set;
  for (NameSymbolMap::iterator iter = map_.begin();
       iter != map_.end(); ++iter) {
    delete_set.insert(iter->second);
  }
  for (const auto &symbol : delete_set) {
    delete symbol;
  }
}

Symbol::~Symbol() {
  for (auto &callsite_symbol : callsites) {
    delete callsite_symbol.second;
  }
}

void Symbol::Merge(const Symbol *other) {
  total_count += other->total_count;
  head_count += other->head_count;
  if (info.file_name.empty()) {
      info.file_name = other->info.file_name;
      info.dir_name = other->info.dir_name;
  }
  for (const auto &pos_count : other->pos_counts)
    pos_counts[pos_count.first] += pos_count.second;
  // Traverses all callsite, recursively Merge the callee symbol.
  for (const auto &callsite_symbol : other->callsites) {
    std::pair<CallsiteMap::iterator, bool> ret = callsites.insert(
        CallsiteMap::value_type(callsite_symbol.first, NULL));
    // If the callsite does not exist in the current symbol, create a
    // new callee symbol with the clone's function name.
    if (ret.second) {
      ret.first->second = new Symbol();
      ret.first->second->info.func_name = ret.first->first.second;
    }
    ret.first->second->Merge(callsite_symbol.second);
  }
}

void SymbolMap::Merge() {
  for (auto &name_symbol : map_) {
    string name = GetOriginalName(name_symbol.first.c_str());
    std::pair<NameSymbolMap::iterator, bool> ret =
        map_.insert(NameSymbolMap::value_type(name, NULL));
    if (ret.second ||
        (name_symbol.first != name &&
         name_symbol.second == ret.first->second)) {
      ret.first->second = new Symbol();
      ret.first->second->info.func_name = ret.first->first.c_str();
    }
    if (ret.first->second != name_symbol.second) {
      ret.first->second->Merge(name_symbol.second);
      for (auto &n_s : map_) {
        if (n_s.second == name_symbol.second &&
            n_s.first != name_symbol.first) {
          n_s.second = ret.first->second;
        }
      }
      name_symbol.second->total_count = 0;
      name_symbol.second->head_count = 0;
    }
  }
}

void SymbolMap::AddSymbol(const string &name) {
  std::pair<NameSymbolMap::iterator, bool> ret = map_.insert(
      NameSymbolMap::value_type(name, NULL));
  if (ret.second) {
    ret.first->second = new Symbol(ret.first->first.c_str(), NULL, NULL, 0);
    NameAliasMap::const_iterator alias_iter = name_alias_map_.find(name);
    if (alias_iter != name_alias_map_.end()) {
      for (const auto &name : alias_iter->second) {
        map_[name] = ret.first->second;
      }
    }
  }
}



//FIX Me : we shouldnt any minimum here
const int64_t kMinSamples = 10;

void SymbolMap::CalculateThresholdFromTotalCount(int64_t total_count) {
  count_threshold_ = total_count * SampleThresholdFrac;
  if (count_threshold_ < kMinSamples) {
    count_threshold_ = kMinSamples;
  }
}

void SymbolMap::CalculateThreshold() {
  // If count_threshold_ is pre-calculated, use pre-caculated value.
  assert(count_threshold_ == 0);
  int64_t total_count = 0;
  std::set<string> visited;
  for (const auto &name_symbol : map_) {
    if (!visited.count(name_symbol.second->name())) {
      visited.insert(name_symbol.second->name());
      total_count += name_symbol.second->total_count;
    }
  }
  count_threshold_ = total_count * SampleThresholdFrac;
  if (count_threshold_ < kMinSamples) {
    count_threshold_ = kMinSamples;
  }
}

const bool SymbolMap::GetSymbolInfoByAddr(
    uint64_t addr, const string **name,
    uint64_t *start_addr, uint64_t *end_addr) const {
  AddressSymbolMap::const_iterator ret = address_symbol_map_.upper_bound(addr);
  if (ret == address_symbol_map_.begin()) {
    return false;
  }
  ret--;
  if (addr >= ret->first && addr < ret->first + ret->second.second) {
    if (name) {
      *name = &ret->second.first;
    }
    if (start_addr) {
      *start_addr = ret->first;
    }
    if (end_addr) {
      *end_addr = ret->first + ret->second.second;
    }
    return true;
  } else {
    return false;
  }
}

const string *SymbolMap::GetSymbolNameByStartAddr(uint64_t addr) const {
  AddressSymbolMap::const_iterator ret = address_symbol_map_.find(addr);
  if (ret == address_symbol_map_.end()) {
    return NULL;
  }
  return &ret->second.first;
}


void SymbolMap::BuildSymbolMap() {
    auto expected_file = llvm::object::createBinary(binary_);
    if (!expected_file) {
        llvm::errs() << "Couldnt open " << binary_;
    }
    llvm::object::Binary  * bb =  expected_file.get().getBinary();
    //llvm::object::ELFObjectFileBase * elffile;
    if (llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(bb)) {
        llvm::object::ELF64LEObjectFile & elffile = *llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(bb);
        if(elffile.getELFFile()->getHeader()->e_type != llvm::ELF::ET_EXEC) {
            llvm::errs() << "Couldnt open " << binary_ << "excutable only ";
        } else {
            auto expected = elffile.getELFFile()->program_headers() ;
            if (expected) {
                std::cerr << "program headers" <<std::endl;
                std::cerr << "size" << expected.get().size() << std::endl ;
                if (expected.get().empty())
                    std::cerr << "is empty" << std::endl ;
            } else {
                std::cout << "Non elfile" << std::endl ;
            }
            for (auto & header : expected.get()) {
                std::cerr << "size" << expected.get().size() << std::endl ;
                if(header.p_type !=  llvm::ELF::PT_LOAD)
                    continue;

                base_addr_ = header.p_vaddr;
                break ;
            }

            for (auto & symb : elffile.symbols()) {
                auto name = symb.getName() ;
                auto address = symb.getAddress() ;
                auto section = symb.getSection();
                if (!name or ! address or !section) {
                  std::cerr << "Can't get symbol infos" << std::endl ;
                  exit(-1);
                }
                if (symb.getSize() == 0) {
                    continue ;
                }
                if (!section){
                    std::cerr << "cant get sections" << std::endl ;
                }
                auto sectionAddress = section.get()->getAddress() ;

                if (!sectionAddress){
                    std::cerr << "cant get sections address" << std::endl ;
                }
                auto symbolOffset = address.get() - sectionAddress +
                                    elffile.getSection(section.get()->getRawDataRefImpl())->sh_offset ;
                std::cout << std::hex << "Adding symbol " << name.get().str()
                          << ", at address " <<  address.get()
                          << ", in section starting at : " << sectionAddress
                          << ", section offset : "<< elffile.getSection(section.get()->getRawDataRefImpl())->sh_offset
                          << ", symbol offset : "<< symbolOffset << std::dec << std::endl;



                std::pair<AddressSymbolMap::iterator, bool> ret =
                        address_symbol_map_.insert(
                                std::make_pair(symbolOffset, std::make_pair(string(name.get()),
                                                                                       symb.getSize())));
                if (!ret.second) {
                    (name_alias_map_)[ret.first->second.first].insert(name.get());
                }
            }
        }
    } else if (llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(bb))  {
        llvm::object::ELF32LEObjectFile & elffile = *llvm::dyn_cast<llvm::object::ELF32LEObjectFile>(bb);
        if(elffile.getELFFile()->getHeader()->e_type != llvm::ELF::ET_EXEC) {
            llvm::errs() << "Couldnt open " << binary_ << "excutable only ";
        } else {
            for (auto & header : elffile.getELFFile()->program_headers().get()) {
                if(header.p_type !=  llvm::ELF::PT_LOAD)
                    continue;
                base_addr_ = header.p_vaddr;
            }

            for (auto & symb : elffile.symbols()) {

                if (symb.getSize() == 0) {
                    continue ;
                }
                std::pair<AddressSymbolMap::iterator, bool> ret =
                        address_symbol_map_.insert(
                                std::make_pair(symb.getAddress().get()  , std::make_pair(string(symb.getName().get()),
                                                                                       symb.getSize())));
                if (!ret.second) {
                    (name_alias_map_)[ret.first->second.first].insert(symb.getName().get());
                }
            }

        }
    } else {
        llvm::errs() << "Couldnt open " << binary_ << "Wrong file format ";
    }

    //ElfReader elf_reader(binary_);
    //base_addr_ = elf_reader.VaddrOfFirstLoadSegment();
    //SymbolReader symbol_reader(&name_alias_map_, &address_symbol_map_);

    //elf_reader.VisitSymbols(&symbol_reader);
}



string Symbol::ModuleName() const {
  // This is a special case in Google3, though tcmalloc.cc has a suffix of .cc,
  // it's actually no a module, but included by tcmalloc_or_debug.cc, which is
  // a pure wrapper. Thus when a function is found to belong to module
  // tcmalloc.cc, it should be reattributed to the wrapper module.
  if (info.RelativePath() == "./tcmalloc/tcmalloc.cc") {
    return "tcmalloc/tcmalloc_or_debug.cc";
  } else {
    return info.RelativePath();
  }
}

bool Symbol::IsFromHeader() const {
  if (HasSuffixString(ModuleName(), ".c") ||
      HasSuffixString(ModuleName(), ".cc") ||
      HasSuffixString(ModuleName(), ".C") ||
      HasSuffixString(ModuleName(), ".cpp")) {
    return false;
  } else if (HasSuffixString(ModuleName(), ".h")) {
    return true;
  } else {
    llvm::errs() << ModuleName() << " has unknown suffix.";
    // If suffix is unknown, we think it is from header so that the module
    // will not be considered in module grouping.
    return true;
  }
}

void SymbolMap::AddSymbolEntryCount(const string &symbol_name, uint64_t count) {
  Symbol *symbol = map_.find(symbol_name)->second;
  symbol->head_count += count;
}

Symbol *SymbolMap::TraverseInlineStack(const string &symbol_name,
                                       const SourceStack &src,
                                       uint64_t count) {
  Symbol *symbol = map_.find(symbol_name)->second;
  symbol->total_count += count;
  const SourceInfo &info = src[src.size() - 1];
  if (symbol->info.file_name.empty() && (!info.file_name.empty())) {
    symbol->info.file_name = info.file_name;
    symbol->info.dir_name = info.dir_name;
  }
  for (int i = src.size() - 1; i > 0; i--) {
    std::pair<CallsiteMap::iterator, bool> ret =
        symbol->callsites.insert(CallsiteMap::value_type(
            Callsite(src[i].Offset(use_discriminator_encoding_),
                     src[i - 1].func_name),
            NULL));
    if (ret.second) {
      ret.first->second = new Symbol(src[i - 1].func_name.c_str(),
                                     src[i - 1].dir_name.c_str(),
                                     src[i - 1].file_name.c_str(),
                                     src[i - 1].start_line);
    }
    symbol = ret.first->second;
    symbol->total_count += count;
  }
  return symbol;
}

void SymbolMap::AddSourceCount(const string &symbol_name,
                               const SourceStack &src,
                               uint64_t count, uint64_t num_inst,
                               Operation op) {
  if (src.size() == 0) {
    return;
  }
  uint32_t offset = src[0].Offset(use_discriminator_encoding_);
  Symbol *symbol = TraverseInlineStack(symbol_name, src, count);
  if (op == MAX) {
    if (count > symbol->pos_counts[offset].count) {
      symbol->pos_counts[offset].count = count;
    }
  } else if (op == SUM) {
    symbol->pos_counts[offset].count += count;
  } else {
    llvm::errs() << "op not supported.";
    exit(-1);
  }
  symbol->pos_counts[offset].num_inst += num_inst;
}

void SymbolMap::AddIndirectCallTarget(const string &symbol_name,
                                      const SourceStack &src,
                                      const string &target,
                                      uint64_t count) {
  if (src.size() == 0) {
    return;
  }
  Symbol *symbol = TraverseInlineStack(symbol_name, src, 0);
  symbol->pos_counts[src[0].Offset(use_discriminator_encoding_)].target_map[
      GetOriginalName(target.c_str())] = count;
}

struct CallsiteLessThan {
  bool operator()(const Callsite& c1, const Callsite& c2) const {
    if (c1.first != c2.first)
      return c1.first < c2.first;
    //if ((c1.second == NULL || c2.second == NULL))
    //  return c1.second == NULL;
    return c1.second <  c2.second;
  }
};

void Symbol::Dump(int ident) const {
  if (ident == 0) {
    printf("%s total:%llu head:%llu\n", info.func_name.c_str(),
           total_count, head_count);
  } else {
    printf("%s total:%llu\n", info.func_name.c_str(), total_count);
  }
  std::vector<uint32_t> positions;
  for (const auto &pos_count : pos_counts)
    positions.push_back(pos_count.first);
  std::sort(positions.begin(), positions.end());
  for (const auto &pos : positions) {
    PositionCountMap::const_iterator ret = pos_counts.find(pos);
    assert(ret != pos_counts.end());
    PrintSourceLocation(info.start_line, pos, ident + 2);
    printf("%llu", ret->second.count);
    TargetCountPairs target_count_pairs;
    GetSortedTargetCountPairs(ret->second.target_map,
                              &target_count_pairs);
    for (const auto &target_count : target_count_pairs) {
      printf("  %s:%llu", target_count.first.c_str(), target_count.second);
    }
    printf("\n");
  }
  std::vector<Callsite> calls;
  for (const auto &pos_symbol : callsites) {
    calls.push_back(pos_symbol.first);
  }
  std::sort(calls.begin(), calls.end(), CallsiteLessThan());
  for (const auto &callsite : calls) {
    PrintSourceLocation(info.start_line, callsite.first, ident + 2);
    callsites.find(callsite)->second->Dump(ident + 2);
  }
}

uint64_t Symbol::MaxPosCallsiteCount() const {
  uint64_t max_count = 0;

  for (const auto& pos_count : pos_counts) {
    max_count = std::max(max_count, pos_count.second.count);
  }

  for (const auto& callsite : callsites) {
    max_count = std::max(max_count, callsite.second->MaxPosCallsiteCount());
  }

  return max_count;
}

void SymbolMap::Dump() const {
  std::map<uint64_t, std::set<string> > count_names_map;
  for (const auto &name_symbol : map_) {
    if (name_symbol.second->total_count > 0) {
      count_names_map[~name_symbol.second->total_count].insert(
          name_symbol.first);
    }
  }
  for (const auto &count_names : count_names_map) {
    for (const auto &name : count_names.second) {
      Symbol *symbol = map_.find(name)->second;
      symbol->Dump(0);
    }
  }
}

float SymbolMap::Overlap(const SymbolMap &map) const {
  std::map<string, std::pair<uint64_t, uint64_t> > overlap_map;

  // Prepare for overlap_map
  uint64_t total_1 = 0;
  uint64_t total_2 = 0;
  for (const auto &name_symbol : map_) {
    total_1 += name_symbol.second->total_count;
    overlap_map[name_symbol.first].first = name_symbol.second->total_count;
    overlap_map[name_symbol.first].second = 0;
  }
  for (const auto &name_symbol : map.map()) {
    if (overlap_map.find(name_symbol.first) == overlap_map.end()) {
      overlap_map[name_symbol.first].first = 0;
    }
    total_2 += name_symbol.second->total_count;
    overlap_map[name_symbol.first].second = name_symbol.second->total_count;
  }

  if (total_1 == 0 || total_2 == 0) {
    return 0.0;
  }

  // Calculate the overlap
  float overlap = 0.0;
  for (const auto &name_counts : overlap_map) {
    overlap += std::min(
        static_cast<float>(name_counts.second.first) / total_1,
        static_cast<float>(name_counts.second.second) / total_2);
  }
  return overlap;
}

void SymbolMap::DumpFuncLevelProfileCompare(const SymbolMap &map) const {
  uint64_t max_1 = 0;
  uint64_t max_2 = 0;

  // Calculate the max of the two maps
  for (const auto &name_symbol : map_) {
    max_1 = std::max(name_symbol.second->total_count, max_1);
  }
  for (const auto &name_symbol : map.map()) {
    max_2 = std::max(name_symbol.second->total_count, max_2);
  }

  // Sort map_1
  std::map<uint64_t, std::vector<string> > count_names_map;
  for (const auto &name_symbol : map_) {
    if (name_symbol.second->total_count > 0) {
      count_names_map[name_symbol.second->total_count].push_back(
          name_symbol.first);
    }
  }
  // Dump hot functions in map_1
  for (auto count_names_iter = count_names_map.rbegin();
       count_names_iter != count_names_map.rend(); ++count_names_iter) {
    for (const auto &name : count_names_iter->second) {
      Symbol *symbol = map_.find(name)->second;
      if (symbol->total_count * 100 < max_1 * DumpCutoffPercent) {
        break;
      }

      const auto &iter = map.map().find(name);
      uint64_t compare_count = 0;
      if (iter != map.map().end()) {
        compare_count = iter->second->total_count;
      }
      printf("%3.4f%% %3.4f%% %s\n",
             100 * static_cast<double>(symbol->total_count) / max_1,
             100 * static_cast<double>(compare_count) / max_2,
             name.c_str());
    }
  }

  // Sort map_2
  count_names_map.clear();
  for (const auto &name_symbol : map.map()) {
    if (name_symbol.second->total_count > 0) {
      count_names_map[name_symbol.second->total_count].push_back(
          name_symbol.first);
    }
  }
  // Dump hot functions in map_2 that was not caught.
  for (auto count_names_iter = count_names_map.rbegin();
       count_names_iter != count_names_map.rend(); ++count_names_iter) {
    for (const auto &name : count_names_iter->second) {
      Symbol *symbol = map.map().find(name)->second;
      if (symbol->total_count * 100 < max_2 * DumpCutoffPercent) {
        break;
      }

      const auto &iter = map_.find(name);
      uint64_t compare_count = 0;
      if (iter != map.map().end()) {
        compare_count = iter->second->total_count;
        if (compare_count * 100 >= max_1 * DumpCutoffPercent) {
          continue;
        }
      }
      printf("%3.4f%% %3.4f%% %s\n",
             100 * static_cast<double>(compare_count) / max_1,
             100 * static_cast<double>(symbol->total_count) / max_2,
             name.c_str());
    }
  }
}





void SymbolMap::AddAlias(const string& sym, const string& alias) {
  name_alias_map_[sym].insert(alias);
}

// Consts for profile validation
static const int kMinNumSymbols = 10;
static const int kMinTotalCount = 1000000;
static const float kMinNonZeroSrcFrac = 0.8;

bool SymbolMap::Validate() const {
  if (size() < kMinNumSymbols) {
    llvm::errs() << "# of symbols (" << size() << ") too small.";
    return false;
  }
  uint64_t total_count = 0;
  uint64_t num_srcs = 0;
  uint64_t num_srcs_non_zero = 0;
  bool has_inline_stack = false;
  bool has_call = false;
  bool has_discriminator = false;
  std::vector<const Symbol *> symbols;
  for (const auto &name_symbol : map_) {
    total_count += name_symbol.second->total_count;
    symbols.push_back(name_symbol.second);
    if (name_symbol.second->callsites.size() > 0) {
      has_inline_stack = true;
    }
  }
  while (!symbols.empty()) {
    const Symbol *symbol = symbols.back();
    symbols.pop_back();
    for (const auto &pos_count : symbol->pos_counts) {
      if (pos_count.second.target_map.size() > 0) {
        has_call = true;
      }
      num_srcs++;
      if (pos_count.first != 0) {
        num_srcs_non_zero++;
      }
      if ((pos_count.first & 0xffff) != 0) {
        has_discriminator = true;
      }
    }
    for (const auto &pos_callsite : symbol->callsites) {
      symbols.push_back(pos_callsite.second);
    }
  }
  if (total_count < kMinTotalCount) {
    llvm::errs() << "Total count (" << total_count << ") too small.";
    return false;
  }
  if (!has_call) {
    llvm::errs() << "Do not have a single call.";
    return false;
  }
  if (!has_inline_stack) {
    llvm::errs() << "Do not have a single inline stack.";
    return false;
  }
  if (!has_discriminator) {
    llvm::errs() << "Do not have a single discriminator.";
    return false;
  }
  if (num_srcs_non_zero < num_srcs * kMinNonZeroSrcFrac) {
    llvm::errs() << "Do not have enough non-zero src locations."
               << " NonZero: " << num_srcs_non_zero
               << " Total: " << num_srcs;
    return false;
  }
  return true;
}
}  // namespace autofdo
