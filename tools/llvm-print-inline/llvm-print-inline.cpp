//
// Created by kaderkeita on 4/11/18.
//


#include "llvm/Support/CommandLine.h"
#include <memory>
#include <string>
#include <tuple>
#include <sstream>
#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include <iostream>
#include <iterator>
using namespace llvm;
using namespace std;

using DWARFLineTable = DWARFDebugLine::LineTable;
using FileLineInfoKind = DILineInfoSpecifier::FileLineInfoKind;
using FunctionNameKind = DILineInfoSpecifier::FunctionNameKind;

cl::opt<std::string> Binary("binary", llvm::cl::desc( "Binary file name"),llvm::cl::Required);




//get the corresponding debug context or fails terminally
static std::unique_ptr<DWARFContext> getDebugContext(std::string & path) {
    static auto expected_file = object::createBinary(Binary);
    if (!expected_file) {
        llvm::errs() << "Couldnt open " << Binary;
        exit(-1);

    // We don't stric;y need to retrict it here to elf64
    } else if (!llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(expected_file.get().getBinary())) {
        llvm::errs() << "Wrong file format  " << Binary;
        exit(-1);
    }

    //auto p  = expected_file.get().takeBinary().first.release();
        auto context = DWARFContext::create(*llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(expected_file.get().getBinary()));
        if (context == nullptr) {
            llvm::errs() << "Failed to load debug info for  " << Binary;
        }
        return context ;
}


const DILineInfoSpecifier infoSpec { FileLineInfoKind::Default, DINameKind::LinkageName};

static DILineInfo getLineInfo(const DWARFDie & die, const DWARFLineTable & LineTable) {

}

static void printSubroutineDie(const DWARFDie & FunctionDIE , const DWARFLineTable & LineTable, std::ostream &out) {
    assert(FunctionDIE.isSubroutineDIE());

    std::string name = FunctionDIE.getSubroutineName(infoSpec.FNKind);

    if (FunctionDIE.isSubprogramDIE()) {
        std::string fileName;
        LineTable.getFileNameByIndex(FunctionDIE.getDeclFile(),nullptr,infoSpec.FLIKind,fileName);
        out  <<  name << " : " << fileName << " : " << FunctionDIE.getDeclLine() ;
    } else {
        //this is an inlined subroutine
        uint32_t CallFile, CallLine, CallColumn, CallDiscriminator;
        FunctionDIE.getCallerFrame(CallFile,CallLine,CallColumn,CallDiscriminator);
        out <<  name  << " : " <<  CallLine << " : " << CallDiscriminator;
    }
}

int main(int argc, char **argv)  {

    llvm::cl::ParseCommandLineOptions(argc,argv,"");

    // Getting a debug context
    auto debugContext  = getDebugContext(Binary);

    // Die and depth level for printing
    // a std::stack would have been sufficient here, but since we also sort the element inplace
    // we need a container with random access.
    std::vector<std::pair<DWARFDie, int>> dies ;
    std::ostream & ss  = std::cout;

    //TODO:  this doesnt work for top level subprograms
    auto die_compare  = [](const  DWARFDie & a, const  DWARFDie & b) {
        assert(a.isSubroutineDIE());
        assert(b.isSubroutineDIE());
        uint32_t  callLineA,callLineB,discriminatorA,discriminatorB;
        uint32_t unused;
        a.getCallerFrame(unused,callLineA,unused,discriminatorA);
        b.getCallerFrame(unused,callLineB,unused,discriminatorB);
        return std::tie(callLineA,discriminatorA) < std::tie(callLineB,discriminatorB);
    };

    for (const auto & compilationUnit : debugContext->compile_units()) {
        const DWARFLineTable & LineTable = *debugContext->getLineTableForUnit(compilationUnit.get());
        //scan for all top level subroutine entries in the give compilation unit
        dies.clear();
        for (auto const & die : compilationUnit->getUnitDIE(false).children()){
            if(!die.isSubprogramDIE() or !die.hasChildren() or (die.getSubroutineName(DINameKind::LinkageName) == nullptr)) continue ;
            assert(die.isValid());
            dies.push_back(std::make_pair(die,0));
        };
        std::cout << "compilation unit " << compilationUnit->getUnitDIE(false).getName(DINameKind::ShortName)
                  << " "<< dies.size() << " function(s)" <<std::endl;
        std::sort(dies.begin(),dies.end(),[&die_compare]
                (const std::pair<DWARFDie, int> & a, const std::pair<DWARFDie, int> & b)
        {         assert(a.first.getSubroutineName(DINameKind::LinkageName) != nullptr);
                  assert(b.first.getSubroutineName(DINameKind::LinkageName) != nullptr);
                 return std::string(a.first.getSubroutineName(DINameKind::LinkageName)) <
                 std::string(b.first.getSubroutineName(DINameKind::LinkageName));});

        while (!dies.empty()){
            const auto & die =  dies.back().first;
            int depth = dies.back().second;
            dies.pop_back();

            size_t current_size = dies.size() ;

            for (auto const & child : die.children()){
                if (!child.isSubroutineDIE() or  (child.getSubroutineName(DINameKind::LinkageName) == nullptr))
                    continue ;
                    dies.push_back(std::make_pair(child,depth +1));
            }

            //We dont print top level functions with no inline call site
            if ( (depth !=0) or (dies.size() >  current_size)) {
                for (int i = 0; i < depth; ++i)
                    ss << ' ';
                printSubroutineDie(die, LineTable, ss);
                //sort the new added children die
                std::sort(dies.begin() + current_size, dies.end(), [&die_compare]
                        (const std::pair<DWARFDie, int> &a, const std::pair<DWARFDie, int> &b) {
                    return die_compare(b.first, a.first);
                });
                ss << std::endl;

            }
            }
    }

    //std::cout << ss.rdbuf() << std::endl;
}