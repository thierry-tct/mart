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

#include "../usermaps.h"
#include "../typesops.h"

#include "third-parties/dg/src/llvm/LLVMDependenceGraph.h"      //https://github.com/mchalupa/dg
#include "third-parties/dg/src/llvm/analysis/DefUse.h"
#include "third-parties/dg/src/llvm/analysis/PointsTo/PointsTo.h"
#include "third-parties/dg/src/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"

#include "third-parties/dg/src/analysis/PointsTo/PointsToFlowSensitive.h"
#include "third-parties/dg/src/analysis/PointsTo/PointsToFlowInsensitive.h"

class MutantNode;
class MutantDependenceGraph;

/*class MutantNode : public Node<MutantDependenceGraph, MuLL::MutantIDType, MutantNode>
{
public:
    MutantNode(MuLL::MutantIDType id)
        :dg::Node<MutantDependenceGraph, MuLL::MutantIDType, MutantNode>(id){}

    ~MutantNode();

    llvm::Value *getID() const { return getKey(); }
};*/

class MutantDependenceGraph //: public DependenceGraph<MutantNode>
{
    struct MutantDepends
    {
        std_unordered_set<MuLL::MutantIDType> outDataDependents;   // x ---> x
        std_unordered_set<MuLL::MutantIDType> inDataDependents;   // this ---> x
        std_unordered_set<MuLL::MutantIDType> outCtrlDependents;   // x ---> x
        std_unordered_set<MuLL::MutantIDType> inCtrlDependents;   // this ---> x
        
        std_unordered_set<MuLL::MutantIDType> tieDependents;   // Share IR (mutants on same statement)
    }
  private:
    std::vector<MutantDepends> mutantDGraphData;  //Adjacent List
    std::unordered_map<MuLL::MutantIDType, std::unordered_set<llvm::Value *>> mutant2IRset;
    std::unordered_map<llvm::Value *, std::unordered_set<MuLL::MutantIDType>> IR2mutantset;
    
    void addDataDependency (MuLL::MutantIDType fromID, MuLL::MutantIDType toID)
    {
        bool isNew = mutantDGraphData[fromID].outDataDependents.insert(toID).second;
        //assert (isNew && "Error: mutant inserted twice as another's out Data.");
        isNew = mutantDGraphData[toID].inDataDependents.insert(fromID).second;
        //assert (isNew && "Error: mutant inserted twice as another's in Data.");
    }
    
    void addCtrlDependency (MuLL::MutantIDType fromID, MuLL::MutantIDType toID)
    {
        bool isNew = mutantDGraphData[fromID].outCtrlDependents.insert(toID).second;
        //assert (isNew && "Error: mutant inserted twice as another's out Ctrl.");
        isNem = mutantDGraphData[toID].inCtrlDependents.insert(fromID).second;
        //assert (isNemw && "Error: mutant inserted twice as another's in CTRL.");
    }
    
    void addTieDependency (MuLL::MutantIDType id1, MuLL::MutantIDType id2)
    {
        bool isNew = mutantDGraphData[id1].tieDependents.insert(id2).second;
        //assert (isNew && "Error: mutant inserted twice as another's tie.");
        isNew = mutantDGraphData[id2].tieDependents.insert(id1).second;
        //assert (isNew && "Error: mutant inserted twice as another's tie.");
    }
    
    void addDataCtrlFor (dg::LLVMDependenceGraph const *subIRDg)
    {
        for (auto nodeIt = subIRDg->begin(), ne = subIRDg->end(); nodeIt != ne; ++nodeIt)
        {
            auto *nodefrom = nodeIt.second;
            llvm::Value *irFrom = nodefrom->getKey();
            for (auto ndata = nodefrom->data_begin(), nde = nodefrom->data_end(); ndata != nde; ++ndata)
            {
                llvm::Value *irDataTo = ndata->getKey();
                for (auto MuLL::MutantIDType m_id_from: IR2mutantset[irFrom])
                    for (auto MuLL::MutantIDType m_id_datato: IR2mutantset[irDataTo])
                        if (m_id_from != m_id_datato)
                            addDataDependency (m_id_from, m_id_datato);
            }
            for (auto nctrl = nodefrom->control_begin(), nce = nodefrom->control_end(); nctrl != nce; ++nctrl)
            {
                llvm::Value *irCtrlTo = nctrl->getKey();
                for (auto MuLL::MutantIDType m_id_from: IR2mutantset[irFrom])
                    for (auto MuLL::MutantIDType m_id_ctrlto: IR2mutantset[irCtrlTo])
                        if (m_id_from != m_id_ctrlto)
                            addCtrlDependency (m_id_from, m_id_ctrlto);
            }
        }
    }
    
  public:
    std::unordered_set<llvm::Value *> &getIRsOfMut (MuLL::MutantIDType id) {return mutant2irset.at(id);}
    bool isBuilt () {return !mutant2irset.empty();}
    
    const std_unordered_set<MuLL::MutantIDType> &getOutDataDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].outDataDependents;}
    const std_unordered_set<MuLL::MutantIDType> &getInDataDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].inDataDependents;}
    const std_unordered_set<MuLL::MutantIDType> &getOutCtrlDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].outCtrlDependents;}
    const std_unordered_set<MuLL::MutantIDType> &getInCtrlDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].inCtrlDependents;}
    
    const std_unordered_set<MuLL::MutantIDType> &getTieDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].tieDependents;}
    
    bool build (llvm::Module const *mod, dg::LLVMDependenceGraph const &irDg, MutantInfoList const &mutInfos)
    {
        std::unordered_map<std::string, std::unordered_map<unsigned, llvm::Value *>> functionName_position2IR;
        
        //First compute IR2mutantset
        for (auto &Func: *mod)
        {
            unsigned instPosition = 0;
            std::string funcName = Func.getName();
            position2IR.clear();        //XXX: Since each mutant belong to only one function
            for (auto &BB: Func)
            {
                for (auto &Inst: BB)
                {
                    if (! functionName_position2IR[funcName].insert(std::pair<unsigned, llvm::Value *>(instPosition, &Inst)).second)
                    {
                        llvm::errs() << "\nError: Each position must correspond to a single IR Instruction\n\n";
                        assert (false);
                    }
                    ++instPosition;
                }
            }
        }
        
      // Compute mutant2IRset and IR2mutantset
        MuLL::MutantIDType mutants_number = mutInfos.getMutantsNumber();
        for (MuLL::MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id)
        {
            auto &tmpPos2IRMap = functionName_position2IR[mutInfos.getMutantFunction(mutant_id)];
            for (auto mutPos: mutInfos.getMutantIrPosInFunction(mutant_id))
            {
                llvm::Value *ir_at_pos = tmpPos2IRMap.at(mutPos);
                mutant2IRset[mutant_id].insert(ir_at_pos);
                IR2mutantset[ir_at_pos].insert(mutant_id);
            }
        }
    
      // Build the mutant dependence graph
        //Add tie dependents
        std::vector<MuLL::MutantIDType> tmpIds;
        for (llvm::Value *inst: IR2mutantset)
        {
            tmpIds.clear();
            for (MuLL::MutantIDType mutant_id: IR2mutantset[inst])
                tmpIds.push_back(mutant_id);
            for (auto m_id_it = tmpIds.begin(), id_e = tmpIds.end(); m_id_it != id_e ; ++m_id_it)
                for (auto tie_id_it = m_id_it + 1; tie_id_it != id_e; ++tie_id)
                    addTieDependency(*m_id_it, *tie_id_it);
        }
        
        //Add ctrl and data dependency
        const std::map<llvm::Value *, LLVMDependenceGraph *>& CF = dg::getConstructedFunctions();
        for (auto& funcdg: CF)
            addDataCtrlFor (funcdg.second);
    }
    
    void dump();
    {
        assert (false && "To be implemented: store as different maps as JSON");
    }
};


class MutantSelection
{
  private:
    
    llvm::Module const &subjectModule;
    MutantInfoList const &mutantInfos;
    
    dg::LLVMDependenceGraph IRDGraph;
    MutantDependenceGraph mutantDGraph;
    
    ////
    void buildDependenceGraphs();
    void findFurthestMutants (std::vector<MuLL::MutantIDType> const &fromMuts, std::vector<MuLL::MutantIDType> &results);
    
  public:
    MutantSelection (llvm::Module const &inMod, MutantInfoList const &mInf): subjectModule(inMod), mutantInfos(mInf) { buildDependenceGraphs(); }
    void smartSelectMutants (std::vector<MuLL::MutantIDType> &selectedMutants);
    void randomMutants (std::vector<MuLL::MutantIDType> &spreadSelectedMutants, std::vector<MuLL::MutantIDType> &dummySelectedMutants, unsigned long number);
    void randomSDLMutants (std::vector<MuLL::MutantIDType> &selectedMutants, unsigned long number);   //only statement deletion mutants
};

