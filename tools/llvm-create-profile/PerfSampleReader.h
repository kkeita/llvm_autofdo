#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <set>
#include <algorithm>
#include <map>
#include<regex>
#include "assert.h"
#include <optional>
#include "sample_reader.h"

#pragma once
namespace autofdo {
    namespace experimental {

        struct MemoryMapping {
            std::string objectFile;
            uint64_t length;
            uint64_t startAddress;
            uint64_t offset;

            //TODO: use std::tie or something
            static bool intersects(const MemoryMapping &a, const MemoryMapping &b) {
                auto &lowest_mapping = a.startAddress < b.startAddress ? a : b;
                auto &highest_mapping = a.startAddress < b.startAddress ? b : a;
                return (lowest_mapping.startAddress + lowest_mapping.length) >= highest_mapping.startAddress;
            }

            bool operator<(const MemoryMapping &rhs) const {
                return startAddress < rhs.startAddress;
            }

            bool operator>(const MemoryMapping &rhs) const {
                return rhs < *this;
            }

            bool operator<=(const MemoryMapping &rhs) const {
                return !(rhs < *this);
            }

            bool operator>=(const MemoryMapping &rhs) const {
                return !(*this < rhs);
            }

            friend std::ostream &operator<<(std::ostream &os, const MemoryMapping &mapping) {
                os << "objectFile: " << mapping.objectFile << " length: " << mapping.length << " startAddress: "
                   << mapping.startAddress << " offset: " << mapping.offset;
                return os;
            }
        };


        struct lbrElement {
            friend std::ostream &operator<<(std::ostream &os, const lbrElement &element) {
                os << "to: " << element.to << " from: " << element.from;
                return os;
            }

            uint64_t from;
            uint64_t to;
        };

//TODO: use a templated size with std::array<32> to avoid allocations on std::vector;
        struct perfEvent {
            uint64_t ip;
            std::vector<lbrElement> brstack;
        };

/*
 * load samples from perf script --no-demangle --show-mmap-events -F ip,brstack output
 * TODO: use string_view vs string for more efficient parsing
 * TODO: add support for unmapping events
 * TODO: we generally assume that the event period is constant, this might not be the case
 * TODO: switch to named groups in the regexes
 * */


            class PerfDataSampleReader : AbstractSampleReader {

                std::vector<Branch> brstack; //lbr content;
                std::vector<Range> ranges;
                std::set<MemoryMapping> mappedAddressSpace;
            public:
                std::ostream &log = std::cerr;
                std::map<Branch, uint64_t> branchCountMap;
                std::map<Range, uint64_t> rangeCountMap;
                std::map<InstructionLocation, uint64_t> ip_count_map;
            private:

                std::optional<InstructionLocation> resolveAddress(uint64_t address) {
                    auto mapping = std::lower_bound(mappedAddressSpace.begin(), mappedAddressSpace.end(), address,
                                                    [&address](const auto &mapped, uint64_t addr) {
                                                        return mapped.startAddress < addr;
                                                    });

                    // if mapping == begin, means that address is lower than the lowest start address of any mapped region.

                    if (mapping == mappedAddressSpace.begin()) {
                        log << "absolute address : " << address << ", could not be resolved " << std::endl;
                        return std::optional < InstructionLocation > {};
                    }
                    mapping--;
                    if (address > ((mapping)->startAddress + (mapping)->length)) {
                        log << "absolute address : " << address << ", could not be resolved " << std::endl;
                        return std::optional < InstructionLocation > {};
                    }

                    {
                        auto offset =  (address - mapping->startAddress) + mapping->offset;
                        InstructionLocation ret{mapping->objectFile,offset};
                        log << "absolute address : " << address << ", resolved to " << ret << std::endl;
                        return std::make_optional(ret);
                    };
                };

                std::optional<MemoryMapping> parseMMAP2(const std::string &line) {
                    //PERF_RECORD_MMAP2 16781/16781: [0x563f9f223000(0x237000) @ 0 08:06 38535520 522742584]: r-xp /usr/bin/find
                    std::string mmap2 = "PERF_RECORD_MMAP2 .*: \\[(.*)\\((.*)\\) @ (\\S*) .*\\]: .* (\\/.*)";
                    std::regex reg(mmap2, std::regex_constants::ECMAScript);
                    std::smatch results;
                    bool matched = std::regex_match(line, results, reg);

                    if (!matched)
                        return std::optional < MemoryMapping > {};
                    if (matched) {
                        uint64_t loadAddress = std::stoull(results[1], nullptr, 16);
                        uint64_t length = std::stoull(results[2], nullptr, 16);
                        uint64_t offset = std::stoull(results[3], nullptr, 16);
                        std::string objectFile(results[4]);
                        return std::make_optional<MemoryMapping>(MemoryMapping{objectFile, length, loadAddress, offset});
                    }
                };

                std::optional<MemoryMapping> parseMMAP(const std::string &line) {
                    std::string mmap = "PERF_RECORD_MMAP .*: \\[(.*)\\((.*)\\) @ (\\S*).*\\]: .* (\\/.*)";
                    std::regex reg(mmap, std::regex_constants::ECMAScript);
                    std::smatch results;
                    bool matched = std::regex_match(line, results, reg);
                    if (!matched)
                        return std::optional < MemoryMapping > {};
                    uint64_t loadAddress = std::stoull(results[1], nullptr, 16);
                    uint64_t length = std::stoull(results[2], nullptr, 16);
                    uint64_t offset = std::stoull(results[3], nullptr, 16);
                    std::string objectFile(results[4]);
                    return std::make_optional<MemoryMapping>(MemoryMapping{objectFile, length, loadAddress, offset});


                };

                std::optional<perfEvent> parsePerfEvent(const std::string &line) {
                    //log << "event parsing" << std::endl;
                    std::string event = "\\"
                            "s+([a-f0-9]+) (((0x[a-f0-9]*)\\/(0x[a-f0-9]*)\\/([P M])\\/([P \\-])\\/([P \\-])\\/([a-f0-9]*)\\s*)+)";
                    std::string lbrContent = "(0x[a-f0-9]*)\\/(0x[a-f0-9]*)\\/([P M])\\/([P \\-])\\/([P \\-])\\/([a-f0-9]*)";
                    std::regex lbr(lbrContent);
                    std::regex reg(event, std::regex_constants::ECMAScript);
                    std::smatch results, results2;
                    bool matched = std::regex_match(line, results, reg);
                    if (!matched)
                        return std::optional < perfEvent > {};
                    //log << "Matched" << std::endl ;
                    perfEvent ret;
                    ret.ip = std::stoull(std::string(results[1]), nullptr, 16);
                    //log << "IP : " << ret.ip <<" : " << std::string("0x") + std::string(results[1]) <<  std::endl ;
                    std::regex_search(line, results2, lbr);
                    std::sregex_iterator it(line.begin(), line.end(), lbr);

                    for (; it != std::sregex_iterator(); it++) {
                        //log << "LBR ITEM : "<< std::string(it->str()) << std::endl;
                        for (auto sub : *it) {
                            //log << "\tLBR ITEM component: "<< std::string(sub) << std::endl;
                        }
                        uint64_t from = std::stoull((*it)[1], nullptr, 16);
                        uint64_t to = std::stoull((*it)[2], nullptr, 16);
                        ret.brstack.emplace_back(lbrElement{from, to});
                    }
                    return ret;
                };

                //Does the given mapping conflicts with the current address space ;
                bool conflictingMemoryMapping(const MemoryMapping &memoryMap) {

                    //TODO: use find_first of for abetter error message
                    auto conflict = std::any_of(mappedAddressSpace.begin(),
                                                mappedAddressSpace.end(),
                                                [&memoryMap](auto &mapping) {
                                                    return MemoryMapping::intersects(mapping, memoryMap);
                                                });
                    if (conflict) {
                        log << "New mapping is conflicting with existing mapping " << std::endl;
                        return true;
                    }
                    return false;
                };


                //Debug functions
                void printAddressSpace() {
                    log << "Address space : " << std::endl;
                    for (const auto &e : mappedAddressSpace) {
                        log << e << std::endl;
                    }
                }

                void parseSingleLine(const std::string &line) {
                    log << "parsing line1 : " << line << std::endl;
                    if (std::optional<MemoryMapping> memoryMap = parseMMAP2(line)) {
                        mappedAddressSpace.insert(memoryMap.value());
                    } else if (auto memoryMap = parseMMAP(line)) {
                        mappedAddressSpace.insert(memoryMap.value());
                    } else if (auto event = parsePerfEvent(line)) {
                        if (auto resolved_ip = resolveAddress(event.value().ip)) {
                            ip_count_map[resolved_ip.value()] += 1;
                        } else {
                            log << "on event : " << line << std::endl;
                            log << "dropping address count because could not resolve : "
                                << event.value().ip << std::endl;
                        }
                        std::vector<std::pair<std::optional<InstructionLocation>, std::optional<InstructionLocation>>> lbr;
                        for (const auto &e : event.value().brstack) {
                            auto from = resolveAddress(e.from);
                            auto to = resolveAddress(e.to);
                            lbr.push_back(std::make_pair(from, to));
                        }

                        for (int i = 0; i < lbr.size() - 1; i++) {
                            auto &from = lbr[i].first;
                            auto &to = lbr[i].second;

                            if ((to) and (from)) {
                                Branch br{from.value(),to.value()};
                                branchCountMap[br] += 1;
                                log << "New LBR element : " << br << std::endl;
                            } else {
                                log << "Dropping lbr element : " << event.value().brstack[i] << std::endl;
                            }

                            auto &next_from = lbr[i + 1].first;
                            if (to and next_from) {
                                Range range{to.value(),next_from.value()};
                                rangeCountMap[range] += 1;
                                log << "New Range : " << range << std::endl;
                            }
                        }
                    } else {
                        log << "parsing failed " << std::endl;
                    }
                }

                const AddressCountMap &address_count_map() const override {
                    return ip_count_map;
                }

                const RangeCountMap &range_count_map() const override {
                    return rangeCountMap;
                }

                const BranchCountMap &branch_count_map() const override {
                    return branchCountMap;
                }


                uint64_t GetTotalSampleCount() const override {
                    return 0;
                }

                uint64_t GetTotalCount() const override {
                    return 0;
                }

            public:

                bool readProfile(std::ifstream &input) override {
                    std::string line;
                    log << std::hex;
                    int count = 0;
                    for (; !input.eof(); std::getline(input, line), count++) {
                        parseSingleLine(line);
                    };
                    return true ;
                };

            };

            static int main2() {
                PerfDataSampleReader loader{};
                std::ifstream samples("/home/kaderkeita/CLionProjects/perfdataParser/perf.txt");
                loader.readProfile(samples);
                for (auto e : loader.rangeCountMap) {
                    loader.log << "Range : " << e.first << ", count = " << e.second << std::endl;
                }
                for (auto e: loader.branchCountMap) {
                    loader.log << "Branch : " << e.first << ", count = " << e.second << std::endl;
                }
                for (auto e : loader.ip_count_map) {
                    loader.log << "Ip: " << e.first << ", count = " << e.second << std::endl;
                }
                std::cout << "Hello, World!" << std::endl;
                return 0;
            }

    }

}


