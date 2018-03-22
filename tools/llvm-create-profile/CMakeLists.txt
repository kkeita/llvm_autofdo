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
set(CMAKE_CXX_STANDARD 17)
add_compile_options(-fno-rtti)

add_llvm_tool(llvm-create-prof
        InstructionSymbolizer.cpp
        profile_creator.cc
        profile_writer.cc
        sample_reader.cc
        symbol_map.cpp
        profile.cc
        llvm_profile_writer.cc
        llvm-create-profile.cpp)