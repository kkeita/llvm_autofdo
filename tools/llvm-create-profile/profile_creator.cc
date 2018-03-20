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

// Create AutoFDO Profile.

#include <memory>

#include "profile_creator.h"
#include "addr2line.h"
#include "profile.h"
#include "profile_writer.h"
#include "sample_reader.h"
#include "symbol_map.h"

namespace autofdo {
    using namespace std;
uint64_t ProfileCreator::GetTotalCountFromTextProfile(
    const string &input_profile_name) {
  ProfileCreator creator("");
  if (!creator.ReadSample(input_profile_name, "text")) {
    return 0;
  }
  return creator.TotalSamples();
}

bool ProfileCreator::CreateProfile(const string &input_profile_name,
                                   const string &profiler,
                                   ProfileWriter *writer,
                                   const string &output_profile_name) {
  if (!ReadSample(input_profile_name, profiler)) {
    return false;
  }
  if (!CreateProfileFromSample(writer, output_profile_name)) {
    return false;
  }
  return true;
}

bool ProfileCreator::ReadSample(const string &input_profile_name,
                                const string &profiler) {
  if (profiler == "perf") {

      llvm::errs() << "Unsupported profiler type: " << profiler;

  } else if (profiler == "text") {
    sample_reader_ = new TextSampleReaderWriter(input_profile_name,this->binary_);
  } else {
    llvm::errs() << "Unsupported profiler type: " << profiler;
    return false;
  }
  if (!sample_reader_->readProfile()) {
    llvm::errs() << "Error reading profile.";
    return false;
  }
  return true;
}

bool ProfileCreator::ComputeProfile(SymbolMap *symbol_map,
                                    Addr2line **addr2line) {
  Profile profile(sample_reader_, binary_, symbol_map);
  profile.ComputeProfile();

  return true;
}

bool ProfileCreator::CreateProfileFromSample(ProfileWriter *writer,
                                             const string &output_name) {
  SymbolMap symbol_map(binary_);
  symbol_map.set_use_discriminator_encoding(use_discriminator_encoding_);
  Addr2line *addr2line = nullptr;
  if (!ComputeProfile(&symbol_map, &addr2line)) return false;

  writer->setSymbolMap(&symbol_map);
  bool ret = writer->WriteToFile(output_name);

  delete addr2line;
  return ret;
}

uint64_t ProfileCreator::TotalSamples() {
  if (sample_reader_ == nullptr) {
    return 0;
  } else {
    return sample_reader_->GetTotalSampleCount();
  }
}

}  // namespace autofdo
