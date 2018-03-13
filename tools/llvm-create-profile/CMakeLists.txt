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
set(CMAKE_CXX_STANDARD 11)
add_compile_options(-fno-rtti)

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