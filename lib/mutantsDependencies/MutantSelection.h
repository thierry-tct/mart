/**
 * -==== MutantSelection.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Implementation of mutant selection based on dependency analysis and ... (this make use of DG - https://github.com/mchalupa/dg).
 */
 
////Uses graphviz for graph representation -- http://www.graphviz.org/pdf/Agraph.pdf  and  http://www.graphviz.org/pdf/libguide.pdf
//// Read/write dot files wit agread/agwrite
//// install 'libcgraph6'
//#include "cgraph.h"

#include "../typesops.h"

#include "third-parties/dg/src/llvm/LLVMDependenceGraph.h"      //https://github.com/mchalupa/dg
#include "third-parties/dg/src/llvm/analysis/DefUse.h"
#include "third-parties/dg/src/llvm/analysis/PointsTo/PointsTo.h"
#include "third-parties/dg/src/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"

#include "third-parties/dg/src/analysis/PointsTo/PointsToFlowSensitive.h"
#include "third-parties/dg/src/analysis/PointsTo/PointsToFlowInsensitive.h"

class MutantNode;
class MutantDependenceGraph;

class MutantNode : public Node<MutantDependenceGraph, MuLL::MutantIDType, MutantNode>
{
public:
    MutantNode(MuLL::MutantIDType id)
        :dg::Node<MutantDependenceGraph, MuLL::MutantIDType, MutantNode>(id){}

    ~MutantNode();

    llvm::Value *getID() const { return getKey(); }
};

class LLVMDependenceGraph : public DependenceGraph<LLVMNode>
{
  private:
    std::unordered_map<MuLL::MutantIDType, std::unordered_set<llvm::Value *>> mutant2irset;
  public:
    std::unordered_set<llvm::Value *> &getIRsOfMut (MuLL::MutantIDType id) {return mutant2irset.at(id);}
    bool build (llvm::Module const *mod, dg::LLVMDependenceGraph const &irDg, MutantInfoList const &mutInfos)
    {
        
    }
};


class MutantSelection
{
  private:
    
    llvm::Module const &inModule;
    MutantInfoList const &mutInfos;
    
    dg::LLVMDependenceGraph irDGraph;
    MutantDependenceGraph mutDGraph;
    
    std::unordered_map<llvm::Value *, std::unordered_set<MuLL::MutantIDType>> ir2mutantset;
    ////
    
  public:
    MutantSelection (llvm::Module const &inMod, MutantInfoList const &mInf): inModule(inMod), mutInfos(mInf) {}
    void buildDependenceGraph();
    void findFurthestMutants (std::vector<MuLL::MutantIDType> const &fromMuts, std::vector<MuLL::MutantIDType> &results);
    void selectMutants (std::vector<MuLL::MutantIDType> &selectedMutants);
    void randomMutants (std::vector<MuLL::MutantIDType> &selectedMutants, unsigned long number);
};

