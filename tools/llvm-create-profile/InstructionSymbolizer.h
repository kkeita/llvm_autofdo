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
#include "llvm/DebugInfo/Symbolize/DIPrinter.h"
#define  DEBUG(x) {};
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
                symbolizer(symbolizerOption),Printer(llvm::errs()){
        };
       void print(const DILineInfo & info, std::ostream & out) {
            //ss << stack[i].dir_name << "/" << stack[i].file_name << ":"<< stack[i].start_line << "-" <<stack[i].line<<":"<< stack[i].discriminator <<", ";
            out << std::string(llvm::sys::path::filename(info.FileName)) << ":" << info.StartLine << "-" << info.Line << ":" << info.Discriminator ;
        }
        void print(DIInliningInfo & info,std::ostream & out) {
            out << "[ "  ;
            for (int i  = 0 ;  i < info.getNumberOfFrames();++i){
                print(info.getFrame(i),out);
                out << ", ";
            }
            out << "]";
        }
        Expected<DIInliningInfo> &symbolizeInstruction(const InstructionLocation &inst) {
            auto it = instructionMap.insert(decltype(instructionMap)::value_type{inst, nullptr});
            DEBUG(std::cerr << "Symbolizing Instruction " << std::hex << inst << std::dec << std::endl);
            if (it.second) {
                uint64_t vaddr = getVaddressFromFileOffset(inst);
                DEBUG(std::cerr << "vaddr : " << vaddr << endl) ;
                //Expected expect to be cheched before beeing moved-assigned
                it.first->second = std::unique_ptr<llvm::Expected<llvm::DIInliningInfo>>
                        (new llvm::Expected<llvm::DIInliningInfo>{std::move(symbolizer.symbolizeInlinedCode(inst.objectFile, vaddr))});
                /*
                if ( ( (*it.first->second.get()).get().getNumberOfFrames() > 0) and
                     ( (*it.first->second.get()).get().getFrame(0).FunctionName == "<invalid>") )
                     it.first->second = std::unique_ptr<llvm::Expected<llvm::DIInliningInfo>>(new llvm::Expected<llvm::DIInliningInfo>(DIInliningInfo()));
                */
                //Printer << (*it.first->second.get()).get()
                //std::cout << std::hex << vaddr << std::dec << " --> " ;
                //print((*it.first->second.get()).get(),std::cout);
                //std::cout << " --> " << count << std::endl ;
            }
            if (*it.first->second.get())
                DEBUG(Printer << (*it.first->second.get()).get());
            return *it.first->second.get();
        }

        static  uint32_t getDuplicationFactor(const DILineInfo & lineInfo) {
                    return llvm::DILocation::getDuplicationFactorFromDiscriminator(lineInfo.Discriminator);
        }

        //TODO: the offset encoding should probably be moved closer to the profile_writer
        static uint32_t Offset(const DILineInfo &lineInfo
                /*bool use_discriminator_encoding :we always use the encoding*/)  {
            //TODO should we assert that line - start_line < 2^16?
            return ((lineInfo.Line - lineInfo.StartLine) << 16) |
                    llvm::DILocation::getBaseDiscriminatorFromDiscriminator(lineInfo.Discriminator);
        }

    public:
        uint64_t getVaddressFromFileOffset(const InstructionLocation &loc) {
            auto it = objectFileCache.insert(decltype(objectFileCache)::value_type{loc.objectFile, nullptr});

            if (it.second) {
                auto expected_file = llvm::object::createBinary(loc.objectFile);
                if (!expected_file) {
                    llvm::errs() << "Couldn't open " << loc.objectFile;
                }
                llvm::object::Binary *bb = expected_file.get().getBinary();
                if (llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(bb)) {
                    llvm::errs() << "opened " << loc.objectFile;
                    it.first->second = std::unique_ptr<llvm::object::OwningBinary<llvm::object::Binary>>
                            {new llvm::object::OwningBinary<llvm::object::Binary>{std::move(expected_file.get())}};
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
        llvm::symbolize::DIPrinter Printer;
        std::map<InstructionLocation, std::unique_ptr<Expected<DIInliningInfo>>> instructionMap;
        mutable std::map<std::string, std::shared_ptr<llvm::object::OwningBinary<llvm::object::Binary>>> objectFileCache;
        llvm::symbolize::LLVMSymbolizer::Options symbolizerOption;
        mutable llvm::symbolize::LLVMSymbolizer symbolizer;
    };

}
#endif //LLVM_INSTRUCTIONSYMBOLIZER_H
