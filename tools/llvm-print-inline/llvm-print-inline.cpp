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
#include <set>
#include <map>
using namespace llvm;
using namespace std;

using DWARFLineTable = DWARFDebugLine::LineTable;
using FileLineInfoKind = DILineInfoSpecifier::FileLineInfoKind;
using FunctionNameKind = DILineInfoSpecifier::FunctionNameKind;



cl::opt<std::string> Binary("binary", llvm::cl::desc( "Binary file name"),llvm::cl::Required);
cl::list<std::string> FunctionNames(llvm::cl::desc( "functions to print"), cl::Positional,cl::ZeroOrMore);
cl::opt<bool> PrintAll("all",llvm::cl::desc("print all functions"), cl::Optional);



/*
 * Represent an inline tree
 *
 * */

static  const DILineInfoSpecifier infoSpec { FileLineInfoKind::Default, DINameKind::LinkageName};


class InlineTree {
public :
    enum class DiffState { SAME,ADDED,REMOVED};
    DiffState diff;
    const DWARFDie  FunctionDIE ;
    std::vector<InlineTree> children;
    unsigned int depth;

    InlineTree(const DWARFDie & rootDie) : FunctionDIE(rootDie), depth(0){
        for (auto const & child : FunctionDIE.children()){
            if (!child.isSubroutineDIE() or  (child.getSubroutineName(DINameKind::LinkageName) == nullptr))
                continue ;
            children.push_back({child});
        }
        if (!children.empty())
            depth = std::max_element(children.begin(),children.end(),
                    [](const InlineTree & a,
                            const InlineTree & b){return a.depth < b.depth;})->depth + 1;
    };


    /*
     * Return true if the head of both trees is the same;
     * We essentially compare the DWARFDie to make sure they represent the same inlined
     *  instance ie same function inlined at the same target location
     *
     * */
    //TODO: unify this with die_compare;
    static bool compare_head(const DWARFDie  &left,const DWARFDie & right) {

        assert(left.isSubroutineDIE());
        assert(right.isSubroutineDIE());
        uint32_t  callLineA,callLineB,discriminatorA,discriminatorB;
        uint32_t unused;
        left.getCallerFrame(unused,callLineA,unused,discriminatorA);
        right.getCallerFrame(unused,callLineB,unused,discriminatorB);

        return (left.getSubroutineName(DINameKind::LinkageName)
                  == right.getSubroutineName(DINameKind::LinkageName))
                  and std::tie(callLineA,discriminatorA) == std::tie(callLineB,discriminatorB);
    }

    static InlineTree TreeDifference(InlineTree & left, InlineTree & right) {
            assert(compare_head(left.FunctionDIE,right.FunctionDIE));

            // matching child nodes;
            auto compare_die = [](const DWARFDie  &left,const DWARFDie & right) { return compare_head(left,right) ;}
            std::set<DWARFDie,decltype(compare_die)> right_nodes ;
            std::map<DWARFDie,decltype(compare_die)> left_nodes ;
            for (auto & child : left.children){
                assert(left_nodes.count(child.FunctionDIE) == 0); //
                left_nodes.insert(child.FunctionDIE);
            }

            for (auto & child : right.children){
                assert(right_nodes.count(child.FunctionDIE) == 0); //
                right_nodes.insert(child.FunctionDIE);
            }

            auto common_nodes = std::set_interse

    }

    void get_code_location(uint32_t ) {

    }
static void printInlineTree(const InlineTree & tree,
                            const DWARFLineTable & LineTable,
                            std::ostream & out = std::cout,
                            const std::string & ident_char = "\t") {

        // Tree and level for printing
        // a std::stack would have been sufficient here, but since we also sort the element inplace
        // we need a container with random access.
        std::vector<std::pair<std::reference_wrapper<const InlineTree>, int> > trees ;
        std::ostream & ss  = std::cout;

        trees.push_back({std::cref(tree),0});

        std::map<int, std::string> ident_map;

        //ident_Char should be capture as const ref, but can't really before c++17
        auto get_indent_string = [&ident_map,ident_char](int level) -> std::string {
                auto insert_it = ident_map.find(level);
                if (insert_it == ident_map.end()) {
                    std::string & ident_string = ident_map[level];
                    for (int i =0; i < level;i++)
                        ident_string+=ident_char;
                }
                return ident_map[level] ;

        };

        // We sort the inlined subroutine by code location
        auto die_compare  = [](const  DWARFDie & a, const  DWARFDie & b) {
           assert(a.isSubroutineDIE());
            assert(b.isSubroutineDIE());
        uint32_t  callLineA,callLineB,discriminatorA,discriminatorB;
        uint32_t unused;
        a.getCallerFrame(unused,callLineA,unused,discriminatorA);
        b.getCallerFrame(unused,callLineB,unused,discriminatorB);
        return std::tie(callLineA,discriminatorA) < std::tie(callLineB,discriminatorB);
        };

        //Iterative depth traversal

        while (!trees.empty()) {
            auto  level = trees.back().second;
            auto  tree  = trees.back().first;
            out << get_indent_string(level);
            printSubroutineDie(tree.get().FunctionDIE,LineTable,out);
            out << "\n";
            trees.pop_back();

            auto child_it = trees.size();
            for (const auto &  subtree : tree.get().children) {
                //TODO investigate subroutine with missing linkageNames
                if (subtree.FunctionDIE.getSubroutineName(DINameKind::LinkageName) == nullptr) continue ;
                assert(subtree.FunctionDIE.isValid());
                trees.push_back({std::cref(subtree),level+1});
            }

            //Sort the newly inserted nodes, this makes the output more stable and easier to compare
            //accros different binaries
            std::sort(trees.begin() + child_it,trees.begin() + trees.size(),[&die_compare]
                    (std::pair<std::reference_wrapper<const InlineTree>, int> & a,
                     std::pair<std::reference_wrapper<const InlineTree>, int> & b) {
                assert(a.first.get().FunctionDIE.getSubroutineName(DINameKind::LinkageName) != nullptr);
                assert(b.first.get().FunctionDIE.getSubroutineName(DINameKind::LinkageName) != nullptr);
                return die_compare(a.first.get().FunctionDIE,b.first.get().FunctionDIE) ; });

        }
    }
    static void printSubroutineDie(const DWARFDie & FunctionDIE, const DWARFLineTable & LineTable, std::ostream &out) {
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
};




//get the corresponding debug context or fails terminally
static std::pair<std::unique_ptr<DWARFContext>,object::OwningBinary<object::Binary>> getDebugContext(std::string & path) {
    auto expected_file = object::createBinary(Binary);
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
        return make_pair(std::move(context), std::move(expected_file.get())) ;
}




static DILineInfo getLineInfo(const DWARFDie & die, const DWARFLineTable & LineTable) {

}

int main(int argc, char **argv)  {

    llvm::cl::ParseCommandLineOptions(argc,argv,"");

    // Getting a debug context
    auto context_  = getDebugContext(Binary);
    auto debugContext = std::move(context_.first) ;

    std::ostream & ss  = std::cout;

    std::set<std::string> functions(FunctionNames.begin(),FunctionNames.end());
    std::set<std::string> printed_functions;

    for (const auto & compilationUnit : debugContext->compile_units()) {
        const DWARFLineTable & LineTable = *debugContext->getLineTableForUnit(compilationUnit.get());

        //scan for all top level subroutine entries in the given compilation unit
        for (auto const & die : compilationUnit->getUnitDIE(false).children()){
            if(!die.isSubprogramDIE() or !die.hasChildren() or (die.getSubroutineName(DINameKind::LinkageName) == nullptr)) continue ;

            /*
             * Don't print if the functions has already been printed (happens when the functions is in multiple CUs,
             * or if the function hasn't been requested
             * */
            if( not (PrintAll) and
                ( (functions.count(die.getSubroutineName(DINameKind::LinkageName)) == 0) or
                 printed_functions.count(die.getSubroutineName(DINameKind::LinkageName)) != 0 )) continue ;

            //we should fail on non contiguous functions
            if (auto err = die.getAddressRanges())
                assert(err.get().size() < 2);

            InlineTree tree(die);
            //Avoid noisy output
            if (tree.depth == 0) continue;
            ss << "compilation unit " << compilationUnit->getUnitDIE(false).getName(DINameKind::ShortName) <<std::endl;
            printed_functions.insert(die.getSubroutineName(DINameKind::LinkageName));
            InlineTree::printInlineTree(die,LineTable);
        };


    }

}