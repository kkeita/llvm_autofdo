#include "llvm/Support/CommandLine.h"
#include <memory>

#include "llvm/ProfileData/SampleProfReader.h"
#include "llvm/ProfileData/SampleProfWriter.h"
#include <iostream>

llvm::cl::opt<std::string> Profile("profile", llvm::cl::desc( "Input profile file name"),llvm::cl::Required);

using namespace llvm::sampleprof ;
using namespace llvm;

int main(int argc, char **argv){
  LLVMContext C;
  std::cout << Profile << std::endl ;
  llvm::cl::ParseCommandLineOptions(argc,argv,"Nonne");
  auto writter = SampleProfileWriter::create(Profile+"_out",SPF_Text);
  auto reader =  SampleProfileReader::create(Profile,C);
  reader.get()->read();
  writter.get()->write(reader.get()->getProfiles());
}
