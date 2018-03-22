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
#include "llvm/IR/DebugInfoMetadata.h"


namespace autofdo {
    using llvm::DIInliningInfo;
    using llvm::DILineInfo;
    using experimental::InstructionLocation;
    using llvm::symbolize::LLVMSymbolizer;
    using llvm::symbolize::LLVMSymbolizer;
    using  llvm::DILocation;
    using llvm::object::Binary;
    using llvm::Expected;




    class InstructionSymbolizer {

    public:
        explicit InstructionSymbolizer() :
                symbolizerOption(
                        /* FunctionNameKind PrintFunctions =*/ llvm::symbolize::FunctionNameKind::LinkageName,
                        /* bool UseSymbolTable =*/ false,
                        /* bool Demangle = */false,
                        /* bool RelativeAddresses =*/ false,
                        /* std::string DefaultArch =*/ ""),
                symbolizer(symbolizerOption),sentinelValue(DIInliningInfo()) {
            if(sentinelValue) {};
        };

        Expected<DIInliningInfo> &symbolizeInstruction(const InstructionLocation &inst) {
            auto it = instructionMap.insert(decltype(instructionMap)::value_type{inst, nullptr});
            if (it.second) {
                uint64_t vaddr = getVaddressFromFileOffset(inst);
                //Expected expect to be cheched before beeing moved-assigned
                it.first->second = std::make_unique<llvm::Expected<llvm::DIInliningInfo>>
                            (symbolizer.symbolizeInlinedCode(inst.objectFile, vaddr));
            }
            return *it.first->second.get();
        }

        static  uint32_t getDuplicationFactor(const DILineInfo & lineInfo) {
                    return llvm::DILocation::getDuplicationFactorFromDiscriminator(lineInfo.Discriminator);
        }

        //TODO: the offset encoding should be probably be moved closer to the profile_writter
        static uint32_t Offset(const DILineInfo &lineInfo
                /*bool use_discriminator_encoding we always use the encoding*/)  {
            //TODO should we assert that line - start_line < 2^16?
            return ((lineInfo.Line - lineInfo.StartLine) << 16) |
                    llvm::DILocation::getBaseDiscriminatorFromDiscriminator(lineInfo.Discriminator);
        }

    private:
        uint64_t getVaddressFromFileOffset(const InstructionLocation &loc) {
            auto it = objectFileCache.insert(decltype(objectFileCache)::value_type{loc.objectFile, nullptr});

            if (it.second) {
                auto expected_file = llvm::object::createBinary(loc.objectFile);
                if (!expected_file) {
                    llvm::errs() << "Couldn't open " << loc.objectFile;
                }
                llvm::object::Binary *bb = expected_file.get().getBinary();
                if (llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(bb)) {
                    it.first->second = std::make_shared<llvm::object::OwningBinary<llvm::object::Binary>>
                            (std::move(expected_file.get()));
                }
            }

            llvm::object::ELF64LEObjectFile &elffile = *llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(
                    it.first->second->getBinary());
            auto sections = elffile.getELFFile()->program_headers();
            if (!sections)
                return 0;
            for (auto &section : sections.get()) {
                if ((section.p_offset <= loc.offset) and (loc.offset < section.p_offset + section.p_filesz)) {
                    return section.p_vaddr + loc.offset;
                }
            }

            return 0;
        }

        Expected<DIInliningInfo> sentinelValue ;
        std::map<InstructionLocation, std::unique_ptr<Expected<DIInliningInfo>>> instructionMap;
        mutable std::map<std::string, std::shared_ptr<llvm::object::OwningBinary<llvm::object::Binary>>> objectFileCache;
        llvm::symbolize::LLVMSymbolizer::Options symbolizerOption;
        mutable llvm::symbolize::LLVMSymbolizer symbolizer;
    };

}
#endif //LLVM_INSTRUCTIONSYMBOLIZER_H
