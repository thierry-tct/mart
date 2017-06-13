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

#ifndef __KLEE_SEMU_GENMU_mutantsSelection_MutantSelection__
#define __KLEE_SEMU_GENMU_mutantsSelection_MutantSelection__

#include "../usermaps.h"
#include "../typesops.h"    //JsonBox

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
    
    MuLL::MutantIDType getMutantsNumber() {return mutantDGraphData.size()-1;}
    
    const std_unordered_set<MuLL::MutantIDType> &getOutDataDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].outDataDependents;}
    const std_unordered_set<MuLL::MutantIDType> &getInDataDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].inDataDependents;}
    const std_unordered_set<MuLL::MutantIDType> &getOutCtrlDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].outCtrlDependents;}
    const std_unordered_set<MuLL::MutantIDType> &getInCtrlDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].inCtrlDependents;}
    
    const std_unordered_set<MuLL::MutantIDType> &getTieDependents(MuLL::MutantIDType mutant_id) {return mutantDGraphData[mutant_id].tieDependents;}
    
    bool build (llvm::Module const *mod, dg::LLVMDependenceGraph const *irDg, MutantInfoList const &mutInfos, std::string mutant_depend_filename)
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
        if (irDg)
        {
            //compute by using the dependence graph of dg and dump resulting mutant dependencies
            const std::map<llvm::Value *, LLVMDependenceGraph *>& CF = dg::getConstructedFunctions();
            for (auto& funcdg: CF)
                addDataCtrlFor (funcdg.second);
            
            //dump to file for consecutive runs maybe tuning (for experiments)
            dump(mutant_depend_filename);
        }
        else
        {
            //load from file
            load(mutant_depend_filename, mutInfos);
        }
    }
    
    void dump(std::string filename);
    {
        JsonBox::Array outListJSON;
        auto nummuts = getMutantsNumber();
        for (auto mutant_id = 1; mutant_id <= nummuts; ++mutant_id)
        {
            JsonBox::Object tmpobj;
            JsonBox::Array tmparr;
            for (auto odid: getOutDataDependents(mutant_id))
                tmparr.push_back(JsonBox::Value(std::to_string(odid)));
            tmpobj["outDataDependents"] = tmparr;
            /*tmparr.clear();
            for (auto idid: getInDataDependents(mutant_id))
                tmparr.push_back(JsonBox::Value(std::to_string(idid)));
            tmpobj["inDataDependents"] = tmparr;*/
            tmparr.clear();
            for (auto ocid: getOutCtrlDependents(mutant_id))
                tmparr.push_back(JsonBox::Value(std::to_string(ocid)));
            tmpobj["outCtrlDependents"] = tmparr;
            /*tmparr.clear();
            for (auto icid: getInCtrlDependents(mutant_id))
                tmparr.push_back(JsonBox::Value(std::to_string(icid)));
            tmpobj["inCtrlDependents"] = tmparr;*/
            tmparr.clear();
            for (auto tid: getTieDependents(mutant_id))
                tmparr.push_back(JsonBox::Value(std::to_string(tid)));
            tmpobj["tieDependents"] = tmparr;
            
            outListJSON.push_back(JsonBox::Value(tmpObj))
        }
        JsonBox::Value vout(outListJSON);
	    vout.writeToFile(filename, false, false);        
    }
    
    void load(std::string filename, MutantInfoList const &mutInfos);
    {
        JsonBox::Value value_in;
	    value_in.loadFromFile(filename);
	    assert (value_in.isArrayt() && "The JSON file data of mutants dependence must be a JSON Array");
	    JsonBox::Array array_in = value_in.getArray();
	    auto nummuts = mutInfos.getMutantsNumber();
	    assert (nummuts == array_in.size() && "the number of mutants mismach, did you choose the right mutants dependence or info file?");
	    
	    //initialize mutantid_str2int
	    std::unordered_map<std::string, MuLL::MutantIDType> mutantid_str2int;
	    for (auto mutant_id = 1; mutant_id <= nummuts; ++mutant_id)
	         mutantid_str2int[std::to_string(mutant_id)] = mutant_id;
	    
	    //Load
        for (auto mutant_id = 1; mutant_id <= nummuts; ++mutant_id)
        {
            //Skip type checking, assume that the file hasn't been modified
            JsonBox::Value tmpv = array_in[mutant_id];
            assert (tmpv.isObject() && "each mutant dependence data is a JSON object");
            JsonBox::Object tmpobj = tmpv.getObject();
            assert (tmpobj.count("outDataDependents")>0 && "'outDataDependents' missing from mutand dependence data");
            //assert (tmpobj.count("inDataDependents")>0 && "'inDataDependents' missing from mutand dependence data");
            assert (tmpobj.count("outCtrlDependents")>0 && "'outCtrlDependents' missing from mutand dependence data");
            //assert (tmpobj.count("inCtrlDependents")>0 && "'inCtrlDependents' missing from mutand dependence data");
            assert (tmpobj.count("tieDependents")>0 && "'tieDependents' missing from mutand dependence data");
            JsonBox::Array tmparr;
            tmparr = tmpobj["outDataDependents"];
            for (JsonBox::Value odid: tmparr)
            {
                assert (odid.isString() && "the mutant in each dependence set should be represented as an intger string (outDataDepends)")
                std::string tmpstr = odid.getString();
                addDataDependency(mutant_id, mutantid_str2int.at(tmpstr));      //out of bound if the value is not a mutant id as unsigned (Mull::MutantIDType)
            }
            tmparr = tmpobj["outCtrlDependents"];
            for (JsonBox::Value ocid: tmparr)
            {
                assert (ocid.isString() && "the mutant in each dependence set should be represented as an intger string (outCtrlDepends)")
                std::string tmpstr = ocid.getString();
                addCtrlDependency(mutant_id, mutantid_str2int.at(tmpstr));      //out of bound if the value is not a mutant id as unsigned (Mull::MutantIDType)
            }
            tmparr = tmpobj["tieDependents"];
            for (JsonBox::Value tid: tmparr)
            {
                assert (tid.isString() && "the mutant in each dependence set should be represented as an intger string (tieDependents)")
                std::string tmpstr = tid.getString();
                addTieDependency(mutant_id, mutantid_str2int.at(tmpstr));      //out of bound if the value is not a mutant id as unsigned (Mull::MutantIDType)
            }
        }
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
    void buildDependenceGraphs(std::string mutant_depend_filename, bool rerundg, bool isFlowSensitive = false, dg::CD_ALG cd_alg = dg::CLASSIC/*or dg::CONTROL_EXPRESSION*/);
    MuLL::MutantIDType pickMutant (std::unorderd_set<MuLL::MutantIDType> const &candidates, std::vector<MuLL::MutantIDType> const &scores);
    void relaxMutant (MuLL::MutantIDType mutant_id, std::vector<double> &scores);
    
  public:
    MutantSelection (llvm::Module const &inMod, MutantInfoList const &mInf, std::string mutant_depend_filename, bool rerundg, bool isFlowSensitive): subjectModule(inMod), mutantInfos(mInf) 
    { 
        buildDependenceGraphs(mutant_depend_filename, rerundg, isFlowSensitive); 
    }
    void smartSelectMutants (std::vector<MuLL::MutantIDType> &selectedMutants, double score_threshold=0.5);
    void randomMutants (std::vector<MuLL::MutantIDType> &spreadSelectedMutants, std::vector<MuLL::MutantIDType> &dummySelectedMutants, unsigned long number);
    void randomSDLMutants (std::vector<MuLL::MutantIDType> &selectedMutants, unsigned long number);   //only statement deletion mutants
};

#endif //__KLEE_SEMU_GENMU_mutantsSelection_MutantSelection__

