//
// Created by kaderkeita on 3/20/18.
//

#ifndef LLVM_INSTRUCTIONSYMBOLIZER_H
#define LLVM_INSTRUCTIONSYMBOLIZER_H

#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/Support/Path.h"
#include "PerfSampleReader.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"

class InstructionSymbolizer {
using llvm::object::Binary;
    using llvm::symbolize::LLVMSymbolizer::Options ;
    using llvm::symbolize::LLVMSymbolizer;
public:
    explicit InstructionSymbolizer() :
            symbolizerOption(
                    /* FunctionNameKind PrintFunctions =*/ llvm::symbolize::FunctionNameKind::LinkageName,
                    /* bool UseSymbolTable =*/ false,
                    /* bool Demangle = */false,
                    /* bool RelativeAddresses =*/ false,
                    /* std::string DefaultArch =*/ ""),
            symbolizer(symbolizerOption) {};



    Expected<DIInliningInfo> symbolizeInstruction(const InstructionLocation & inst) {
        uint64_t vaddr = getVaddressFromFileOffset(loc);
        return symbolizer.symbolizeInlinedCode(loc.objectFile, vaddr);
    }
private:
    uint64_t getVaddressFromFileOffset(const InstructionLocation & loc){
        auto it = objectFileCache.insert(decltype(objectFileCache)::value_type{loc.objectFile,nullptr});

        if (it.second){
            auto expected_file = llvm::object::createBinary(loc.objectFile);
            if (!expected_file) {
                llvm::errs() << "Couldn't open " << loc.objectFile;
            }
            llvm::object::Binary  * bb =  expected_file.get().getBinary();
            if (llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(bb)) {
                it.first->second = std::make_shared<llvm::object::OwningBinary<llvm::object::Binary>>
                        (std::move(expected_file.get()));
            }
        }

        llvm::object::ELF64LEObjectFile &elffile = *llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(it.first->second->getBinary());
        auto sections  = elffile.getELFFile()->program_headers();
        if(!sections)
            return 0;
        for (auto & section : sections.get()){
            if ( ( section.p_offset <= loc.offset) and ( loc.offset < section.p_offset + section.p_filesz)) {
                return section.p_vaddr + loc.offset ;
            }
        }

        return 0;
    }
    std::map<InstructionLocation,std::shared_ptr<DIInliningInfo>>
    mutable std::map<std::string, std::shared_ptr<llvm::object::OwningBinary<llvm::object::Binary>> > objectFileCache;
    llvm::symbolize::LLVMSymbolizer::Options symbolizerOption;
    mutable llvm::symbolize::LLVMSymbolizer symbolizer;
};


#endif //LLVM_INSTRUCTIONSYMBOLIZER_H
