cmake_minimum_required(VERSION 3.9)
set(LLVM_LINK_COMPONENTS
        DebugInfoDWARF
        AllTargetsDescs
        AllTargetsInfos
        MC
        Object
        Support
        ProfileData
        Symbolize
        )
#set(CMAKE_CXX_COMPILER  gcc-7)
set(CMAKE_CXX_STANDARD 11)
#add_definitions(-DLLVM)
#link_libraries(-lLLVMLTO -lLLVMPasses -lLLVMObjCARCOpts -lLLVMSymbolize
#        -lLLVMDebugInfoPDB -lLLVMDebugInfoDWARF
#        -lLLVMFuzzMutate -lLLVMTableGen -lLLVMDlltoolDriver -lLLVMLineEditor -lLLVMOrcJIT -lLLVMCoverage
#-lLLVMMIRParser -lLLVMObjectYAML -lLLVMLibDriver -lLLVMOption -lLLVMWindowsManifest -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMGlobalISel
#-lLLVMSelectionDAG -lLLVMAsmPrinter -lLLVMDebugInfoCodeView -lLLVMDebugInfoMSF -lLLVMX86Desc -lLLVMMCDisassembler -lLLVMX86Info -lLLVMX86AsmPrinter -lLLVMX86Utils -lLLVMMCJIT -lLLVMInterpreter -lLLVMExecutionEngine -lLLVMRuntimeDyld -lLLVMCodeGen -lLLVMTarget -lLLVMCoroutines -lLLVMipo -lLLVMInstrumentation -lLLVMVectorize -lLLVMScalarOpts -lLLVMLinker -lLLVMIRReader -lLLVMAsmParser -lLLVMInstCombine -lLLVMBitWriter -lLLVMAggressiveInstCombine -lLLVMTransformUtils -lLLVMAnalysis -lLLVMProfileData -lLLVMObject -lLLVMMCParser -lLLVMMC -lLLVMBitReader -lLLVMCore -lLLVMBinaryFormat -lLLVMSupport -lLLVMDemangle)
add_compile_options(-fno-rtti)
#/home/kaderkeita/local/install/bin/llvm-config --system-libs
#link_libraries(-lz -lrt -ldl -ltinfo -lpthread -lm)
#link_directories(/home/kaderkeita/local/install/lib/)
#add_definitions(-DHAVE_LLVM)
add_llvm_tool(create-llvm-prof
        addr2line.cc
        instruction_map.cc
        profile_creator.cc
        profile_writer.cc
        sample_reader.cc
        source_info.cc
        symbol_map.cc
        profile.cc
        llvm_profile_writer.cc
        create_llvm_prof.cc)

#add_executable(create-test
#        PerfSampleReader.cpp)
#add_library(libsymbolize symbolize/addr2line_inlinestack.cc symbolize/bytereader.cc symbolize/dwarf2reader.cc symbolize/dwarf3ranges.cc symbolize/elf_reader.cc symbolize/functioninfo.cc)
#add_library(libgflags gflags/gflags.cc gflags/gflags_completions.cc gflags/gflags_reporting.cc)
