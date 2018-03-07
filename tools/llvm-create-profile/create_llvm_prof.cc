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

// This program creates an LLVM profile from an AutoFDO source.

#include "llvm/Support/CommandLine.h"
#include <memory>

#include "profile_creator.h"
#include "llvm_profile_writer.h"

llvm::cl::opt<std::string> Profile("profile", llvm::cl::desc( "Input profile file name"),llvm::cl::Required);
llvm::cl::opt<std::string> Profiler("profiler", llvm::cl::desc( "Input profile type"),llvm::cl::Required);
llvm::cl::opt<std::string> Out("out", llvm::cl::desc( "Output profile file name"),llvm::cl::Required);
llvm::cl::opt<std::string> Binary("binary", llvm::cl::desc( "Binary file name"),llvm::cl::Required);


//enum OutputFileFormat {
//    Text,
//    Binary,
//};

llvm::cl::opt<std::string> Format("format", llvm::cl::desc( "Output file format"),llvm::cl::Required);


#define PROG_USAGE                                           \
  "\nConverts a sample profile collected with Perf "         \
  "(https://perf.wiki.kernel.org/)\n"                        \
  "into an LLVM profile. The output file can be used with\n" \
  "Clang's -fprofile-sample-use flag."

int main(int argc, char **argv) {
    llvm::cl::ParseCommandLineOptions(argc,argv,PROG_USAGE);



  std::unique_ptr<autofdo::LLVMProfileWriter> writer(nullptr);
  if (Format.getValue()== "text") {
    writer.reset(new autofdo::LLVMProfileWriter(llvm::sampleprof::SPF_Text));
  } else if (Format.getValue() == "binary") {
    writer.reset(new autofdo::LLVMProfileWriter(llvm::sampleprof::SPF_Binary));
  } else {
    llvm::errs() << "--format must be one of 'text' or 'binary'";
    return 1;
  }

  autofdo::ProfileCreator creator(Binary);
  creator.set_use_discriminator_encoding(true);
  if (creator.CreateProfile(Profile, Profiler, writer.get(),
                            Out))
    return 0;
  else
    return -1;
}
