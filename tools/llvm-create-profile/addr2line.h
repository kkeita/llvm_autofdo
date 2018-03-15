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

// Class to derive inline stack.

#ifndef AUTOFDO_ADDR2LINE_H_
#define AUTOFDO_ADDR2LINE_H_

#include <string>
#include <map>

#include "source_info.h"
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Support/Path.h"
#include "PerfSampleReader.h"
namespace autofdo {
    using namespace std ;
    using experimental::InstructionLocation;
    class Addr2line {
    public:
        Addr2line() {};
        explicit Addr2line(const string &binary_name) : binary_name_(binary_name) {}

        virtual ~Addr2line() {}

        static Addr2line *Create(const string &binary_name);

        static Addr2line *CreateWithSampledFunctions();


        // Stores the inline stack of ADDR in STACK.
        virtual void GetInlineStack(InstructionLocation, SourceStack *stack) =0 ;

        string binary_name_;

    private:
    };

    class AddressQuery;

    class InlineStackHandler;

    class ElfReader;

    class LineIdentifier;

    class AddressToLineMap;


    class LLVMAddr2line : public Addr2line {
    public:
        explicit LLVMAddr2line() :
                Addr2line(),
                symbolizerOption(
                        /*FunctionNameKind PrintFunctions =*/ llvm::symbolize::FunctionNameKind::LinkageName,
                        /*bool UseSymbolTable =*/ false,
                        /* bool Demangle = */false,
                        /*bool RelativeAddresses =*/ true,
                        /* std::string DefaultArch =*/ ""),
                symbolizer(symbolizerOption) {

        };

        virtual ~LLVMAddr2line() {};

        void GetInlineStack(InstructionLocation loc, SourceStack *stack) override {
            llvm::Expected<llvm::DIInliningInfo> st = std::move(symbolizer.symbolizeInlinedCode(loc.objectFile, loc.offset));
            if (st) {
                auto ss = st.get();
                SourceStack &mystack = *stack; //;(ss.getNumberOfFrames());
                mystack.resize(ss.getNumberOfFrames());
                for (int i = 0; i < ss.getNumberOfFrames(); i++) {
                    mystack[i].file_name =
                            llvm::sys::path::filename(ss.getFrame(i).FileName.c_str());
                    mystack[i].dir_name; //= ss.getFrame(i).FileName.c_str();
                    mystack[i].discriminator = ss.getFrame(i).Discriminator;
                    mystack[i].start_line = ss.getFrame(i).StartLine;
                    mystack[i].func_name = ss.getFrame(i).FunctionName.c_str();
                    mystack[i].line = ss.getFrame(i).Line;

                }
            } else {
                std::cerr << "failled to get stack object for location \n" << std::hex << loc << std::dec;
                st.takeError();
            }

        };
    private:
        llvm::symbolize::LLVMSymbolizer::Options symbolizerOption;
        mutable llvm::symbolize::LLVMSymbolizer symbolizer;
    };



}
#endif  // AUTOFDO_ADDR2LINE_H_
