/**
 * -==== MutantSelection.cpp
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Implementation of mutant selection based on dependency analysis and ... (this make use of DG - https://github.com/mchalupa/dg).
 */

#include <cstdlib>     /* srand, rand */
#include <ctime> 
#include <algorithm>


#include "MutantSelection.h"

#ifndef ENABLE_CFG
#define ENABLE_CFG
#endif
#include "../third-parties/dg/src/llvm/LLVMDependenceGraph.h"      //https://github.com/mchalupa/dg
#include "../third-parties/dg/src/llvm/analysis/DefUse.h"
#include "../third-parties/dg/src/llvm/analysis/PointsTo/PointsTo.h"
#include "../third-parties/dg/src/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"

#include "../third-parties/dg/src/analysis/PointsTo/PointsToFlowSensitive.h"
#include "../third-parties/dg/src/analysis/PointsTo/PointsToFlowInsensitive.h"


//class MutantDependenceGraph

void MutantDependenceGraph::addDataCtrlFor (dg::LLVMDependenceGraph const *subIRDg)
{
    for (auto nodeIt = subIRDg->begin(), ne = subIRDg->end(); nodeIt != ne; ++nodeIt)
    {
        auto *nodefrom = nodeIt->second;
        llvm::Value *irFrom = nodefrom->getKey();
        for (auto ndata = nodefrom->data_begin(), nde = nodefrom->data_end(); ndata != nde; ++ndata)
        {
            llvm::Value *irDataTo = (*ndata)->getKey();
            for (MuLL::MutantIDType m_id_from: IR2mutantset[irFrom])
                for (MuLL::MutantIDType m_id_datato: IR2mutantset[irDataTo])
                    if (m_id_from != m_id_datato)
                        addDataDependency (m_id_from, m_id_datato);
        }
        for (auto nctrl = nodefrom->control_begin(), nce = nodefrom->control_end(); nctrl != nce; ++nctrl)
        {
            llvm::Value *irCtrlTo = (*nctrl)->getKey();
            for (MuLL::MutantIDType m_id_from: IR2mutantset[irFrom])
                for (MuLL::MutantIDType m_id_ctrlto: IR2mutantset[irCtrlTo])
                    if (m_id_from != m_id_ctrlto)
                        addCtrlDependency (m_id_from, m_id_ctrlto);
        }
    }
}
    
bool MutantDependenceGraph::build (llvm::Module const &mod, dg::LLVMDependenceGraph const *irDg, MutantInfoList const &mutInfos, std::string mutant_depend_filename)
{
    std::unordered_map<std::string, std::unordered_map<unsigned, llvm::Value const *>> functionName_position2IR;
    
    //First compute IR2mutantset
    for (auto &Func: mod)
    {
        unsigned instPosition = 0;
        std::string funcName = Func.getName();
        //functionName_position2IR.clear();        //XXX: Since each mutant belong to only one function
        for (auto &BB: Func)
        {
            for (auto &Inst: BB)
            {
                if (! functionName_position2IR[funcName].insert(std::pair<unsigned, llvm::Value const *>(instPosition, &Inst)).second)
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
        auto &tmpPos2IRMap = functionName_position2IR.at(mutInfos.getMutantFunction(mutant_id));
        for (auto mutPos: mutInfos.getMutantIrPosInFunction(mutant_id))
        {
            llvm::Value const *ir_at_pos = tmpPos2IRMap.at(mutPos);
            mutant2IRset[mutant_id].insert(ir_at_pos);
            IR2mutantset[ir_at_pos].insert(mutant_id);
        }
    }

  // Build the mutant dependence graph
    //Add tie dependents
    std::vector<MuLL::MutantIDType> tmpIds;
    for (auto & pair_val: IR2mutantset)
    {
        llvm::Value const *inst = pair_val.first;
        tmpIds.clear();
        for (MuLL::MutantIDType mutant_id: IR2mutantset[inst])
            tmpIds.push_back(mutant_id);
        for (auto m_id_it = tmpIds.begin(), id_e = tmpIds.end(); m_id_it != id_e ; ++m_id_it)
            for (auto tie_id_it = m_id_it + 1; tie_id_it != id_e; ++tie_id_it)
                addTieDependency(*m_id_it, *tie_id_it);
    }
    
    //Add ctrl and data dependency
    if (irDg)
    {
        //compute by using the dependence graph of dg and dump resulting mutant dependencies
        const std::map<llvm::Value *, dg::LLVMDependenceGraph *>& CF = dg::getConstructedFunctions();
        for (auto& funcdg: CF)
            addDataCtrlFor (funcdg.second);
        
        //dump to file for consecutive runs maybe tuning (for experiments)
        if (!mutant_depend_filename.empty())
            dump(mutant_depend_filename);
    }
    else
    {
        assert (!mutant_depend_filename.empty() && "no mutnat dependency cache specified when dg is disabled");
        
        //load from file
        load(mutant_depend_filename, mutInfos);
    }
}

void MutantDependenceGraph::dump(std::string filename)
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
        
        outListJSON.push_back(JsonBox::Value(tmpobj));
    }
    JsonBox::Value vout(outListJSON);
    vout.writeToFile(filename, false, false);        
}

void MutantDependenceGraph::load(std::string filename, MutantInfoList const &mutInfos)
{
    JsonBox::Value value_in;
    value_in.loadFromFile(filename);
    assert (value_in.isArray() && "The JSON file data of mutants dependence must be a JSON Array");
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
        JsonBox::Value tmpv = array_in[mutant_id - 1];
        assert (tmpv.isObject() && "each mutant dependence data is a JSON object");
        JsonBox::Object tmpobj = tmpv.getObject();
        assert (tmpobj.count("outDataDependents")>0 && "'outDataDependents' missing from mutand dependence data");
        //assert (tmpobj.count("inDataDependents")>0 && "'inDataDependents' missing from mutand dependence data");
        assert (tmpobj.count("outCtrlDependents")>0 && "'outCtrlDependents' missing from mutand dependence data");
        //assert (tmpobj.count("inCtrlDependents")>0 && "'inCtrlDependents' missing from mutand dependence data");
        assert (tmpobj.count("tieDependents")>0 && "'tieDependents' missing from mutand dependence data");
        JsonBox::Array tmparr;
        assert (tmpobj["outDataDependents"].isArray() && "'outDataDependents' must be an array");
        tmparr = tmpobj["outDataDependents"].getArray();
        for (JsonBox::Value odid: tmparr)
        {
            assert (odid.isString() && "the mutant in each dependence set should be represented as an intger string (outDataDepends)");
            std::string tmpstr = odid.getString();
            addDataDependency(mutant_id, mutantid_str2int.at(tmpstr));      //out of bound if the value is not a mutant id as unsigned (Mull::MutantIDType)
        }
        assert (tmpobj["outCtrlDependents"].isArray() && "'outCtrlDependents' must be an Array");
        tmparr = tmpobj["outCtrlDependents"].getArray();
        for (JsonBox::Value ocid: tmparr)
        {
            assert (ocid.isString() && "the mutant in each dependence set should be represented as an intger string (outCtrlDepends)");
            std::string tmpstr = ocid.getString();
            addCtrlDependency(mutant_id, mutantid_str2int.at(tmpstr));      //out of bound if the value is not a mutant id as unsigned (Mull::MutantIDType)
        }
        assert (tmpobj["tieDependents"].isArray() && "'tieDependents' must be an Array");
        tmparr = tmpobj["tieDependents"].getArray();
        for (JsonBox::Value tid: tmparr)
        {
            assert (tid.isString() && "the mutant in each dependence set should be represented as an intger string (tieDependents)");
            std::string tmpstr = tid.getString();
            addTieDependency(mutant_id, mutantid_str2int.at(tmpstr));      //out of bound if the value is not a mutant id as unsigned (Mull::MutantIDType)
        }
    }
}
/////

//class MutantSelection

void MutantSelection::buildDependenceGraphs(std::string mutant_depend_filename, bool rerundg, bool isFlowSensitive, bool isClassicCtrlDepAlgo)
{
    if (rerundg)
    {
        dg::CD_ALG cd_alg;
        if (isClassicCtrlDepAlgo)
            cd_alg = dg::CLASSIC;
        else
            cd_alg = dg::CONTROL_EXPRESSION;
            
        //build IR DGraph
        dg::LLVMDependenceGraph IRDGraph;
        dg::LLVMPointerAnalysis *PTA = new dg::LLVMPointerAnalysis(&subjectModule);
        if (isFlowSensitive)
            PTA->run<dg::analysis::pta::PointsToFlowSensitive>();
        else
            PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();
            
        assert (IRDGraph.build(&subjectModule, PTA) && "Error: failed to build dg dependence graph");
        assert(PTA && "BUG: Need points-to analysis");
        
        dg::analysis::rd::LLVMReachingDefinitions RDA(&subjectModule, PTA);
        RDA.run();  // compute reaching definitions
        
        dg::LLVMDefUseAnalysis DUA(&IRDGraph, &RDA, PTA);
        DUA.run(); // add def-use edges according that
        
        delete PTA;
            
        IRDGraph.computeControlDependencies(cd_alg);
        
        //Build mutant DGraph
        mutantDGraph.build(subjectModule, &IRDGraph, mutantInfos, mutant_depend_filename);
    }
    else
    {
        //load from file
         mutantDGraph.build(subjectModule, nullptr, mutantInfos, mutant_depend_filename);
    }
}

MuLL::MutantIDType MutantSelection::pickMutant (std::unordered_set<MuLL::MutantIDType> const &candidates, std::vector<double> const &scores)
{
    std::vector<MuLL::MutantIDType> topScored;
    
    // First see which are having the maximum score
    double top_score = 0.0;
    for (auto mutant_id: candidates)
    {
        if (scores[mutant_id] >= top_score)
        {
            if (scores[mutant_id] > top_score)
            {
                top_score = scores[mutant_id];
                topScored.clear();
            }
            topScored.push_back(mutant_id);
        }
    }
    
    // Second see which among maximum score have smallest number of incoming control dependence
    unsigned minoutctrlnum = (unsigned) -1;
    unsigned maxinctrlnum = 0;
    MuLL::MutantIDType chosenMutant;
    for (auto mutant_id: topScored)
    {
        if (mutantDGraph.getOutCtrlDependents(mutant_id).size() < minoutctrlnum)
        {
            chosenMutant = mutant_id;
            minoutctrlnum = mutantDGraph.getOutCtrlDependents(mutant_id).size();
            maxinctrlnum = mutantDGraph.getInCtrlDependents(mutant_id).size();
        }
        else if (mutantDGraph.getOutCtrlDependents(mutant_id).size() == minoutctrlnum && mutantDGraph.getInCtrlDependents(mutant_id).size() > maxinctrlnum)
        {   
            chosenMutant = mutant_id;
            maxinctrlnum = mutantDGraph.getInCtrlDependents(mutant_id).size();
        }
    }
    return chosenMutant;
}

/**
 * \brief relaxes a mutant by affecting the score of its dependent mutants up to a certain hop
 */
void MutantSelection::relaxMutant (MuLL::MutantIDType mutant_id, std::vector<double> &scores)
{
    const double RELAX_STEP = 0.5;
    const double RELAX_THRESHOLD = 0.1;  // 5 hops
    //const MuLL::MutantIDType ORIGINAL_ID = 0; //no mutant have id 0, that's the original's
    
    // at data dependency hop n (in and out), and the corresponding node having score x, set its value to x'={x - x*RELAX_STEP/n}, if (RELAX_STEP/n) >= RELAX_THRESHOLD
    std::unordered_set<MuLL::MutantIDType> visited;
    std::queue<MuLL::MutantIDType> workQueue;
    MuLL::MutantIDType next_hop_first_mutant = mutant_id;
    unsigned curhop = 0;
    double cur_relax_factor = 0.0;
    workQueue.push(mutant_id);
    visited.insert(mutant_id);
    while (! workQueue.empty())
    {
        auto elem = workQueue.front();
        workQueue.pop();
        
        if (next_hop_first_mutant == elem)
        {
            ++curhop;
            cur_relax_factor = RELAX_STEP / curhop;
        }
            
        //Check if treshold reached. 
        //Since we use BFS, when an element reaches the threshold, all consecutive element will also reach so we can safely stop
        if (cur_relax_factor < RELAX_THRESHOLD)
            break;
        
        //expand
        for (auto inDatDepMut: mutantDGraph.getInDataDependents(elem))
        {
            //relax
            if (visited.insert(inDatDepMut).second)
            {
                if (next_hop_first_mutant == elem)
                    next_hop_first_mutant = inDatDepMut;
                workQueue.push(inDatDepMut);
            }
            
            //the score still decrease if there are many dependency path to this mutants (ex: a=1; b=a+3; c=a+b;). here inDatDepMut is at 'a=1'
            //explore also the case where we increase back if it was already visited(see out bellow)
            scores[inDatDepMut] -=  scores[inDatDepMut] * cur_relax_factor;           
        }
        for (auto outDatDepMut: mutantDGraph.getOutDataDependents(elem))
        {
            //relax
            if (visited.insert(outDatDepMut).second)
            {
                if (next_hop_first_mutant == elem)
                    next_hop_first_mutant = outDatDepMut;
                workQueue.push(outDatDepMut);
            }
            
            //the score still decrease if there are many dependency path to this mutants (ex: a=1; b=a+3; c=a+b;). here inDatDepMut is at 'c=a+b'
            scores[outDatDepMut] -=  scores[outDatDepMut] * cur_relax_factor; 
        }
    }
}

/**
 * \brief Stop when the best selected mutant's score is less to the score_threshold or there are no mutant left
 */
void MutantSelection::smartSelectMutants (std::vector<MuLL::MutantIDType> &selectedMutants, std::vector<double> &selectedScores)
{
    const double MAX_SCORE = 1.0;
    
    MuLL::MutantIDType mutants_number = mutantInfos.getMutantsNumber();

    selectedMutants.reserve(mutants_number);
    selectedScores.reserve(mutants_number);
    
    //Choose starting mutants (random for now: 1 mutant per dependency cluster)
    std::vector<double> mutant_scores(mutants_number + 1);
    std::unordered_set<MuLL::MutantIDType> visited_mutants;
    std::unordered_set<MuLL::MutantIDType> candidate_mutants;
    
    for (MuLL::MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id)
    {
        mutant_scores[mutant_id] = MAX_SCORE;
        candidate_mutants.insert(mutant_id);
    }
    
    std::unordered_set<MuLL::MutantIDType> tiesTemporal;   //For now put all ties here and append to list at the end
    
    while (! candidate_mutants.empty())
    {
        auto mutant_id = pickMutant(candidate_mutants, mutant_scores);
        //-----llvm::errs()<<candidate_mutants.size()<<" "<<mutant_scores[mutant_id]<<"\n";
        // Stop if the selected mutant has a score less than the threshold
        //if (mutant_scores[mutant_id] < score_threshold)
        //    break;
            
        selectedMutants.push_back(mutant_id);
        selectedScores.push_back(mutant_scores[mutant_id]);
        relaxMutant (mutant_id, mutant_scores);
        
        // insert the picked mutants and its tie-dependents into visited set
        visited_mutants.insert(mutant_id);
        visited_mutants.insert(mutantDGraph.getTieDependents(mutant_id).begin(), mutantDGraph.getTieDependents(mutant_id).end());
        
        tiesTemporal.insert(mutantDGraph.getTieDependents(mutant_id).begin(), mutantDGraph.getTieDependents(mutant_id).end());
        
        // Remove picked mutants and its tie dependents from candidates
        candidate_mutants.erase(mutant_id);
        for (auto tie_ids: mutantDGraph.getTieDependents(mutant_id))
            candidate_mutants.erase(tie_ids);
    }
    
    //append the ties temporal to selection
    selectedMutants.insert(selectedMutants.end(), tiesTemporal.begin(), tiesTemporal.end());
    for (auto mid: tiesTemporal)    
        selectedScores.push_back(-1e-100);   //default score for ties (very low value)
    
    if (selectedMutants.size() != mutants_number || selectedScores.size() != mutants_number)
    {
        llvm::errs() << "\nError: Bug in Selection: some mutants left out or added many times: Mutants number = ";
        llvm::errs() << mutants_number << ", Selected number = " << selectedMutants.size() << ", selected scores number = " << selectedScores.size() << "\n\n";
    }
}

void MutantSelection::randomMutants (std::vector<MuLL::MutantIDType> &spreadSelectedMutants, std::vector<MuLL::MutantIDType> &dummySelectedMutants, unsigned long number)
{
    MuLL::MutantIDType mutants_number = mutantInfos.getMutantsNumber();
    
    assert (mutantDGraph.isBuilt() && "This function must be called after the mutantDGraph is build. We need mutants IRs");
    
    spreadSelectedMutants.reserve(mutants_number);
    dummySelectedMutants.reserve(mutants_number);
    
    /// \brief Spread Random
    std::vector<llvm::Value const *> mutatedIRs;
    std::unordered_map<llvm::Value const *, std::unordered_set<MuLL::MutantIDType>> work_map(mutantDGraph.getIR2mutantsetMap());   
    
    mutatedIRs.reserve(work_map.size());
    for (auto &ir2mutset: work_map)
    {
        mutatedIRs.push_back(ir2mutset.first);
    }
    
    bool someselected = true;
    while (someselected)
    {
        someselected = false;
        
        //shuflle IRs
        std::srand (std::time(NULL) + clock());  //+ clock() because fast running program will generate same sequence with only time(NULL)
        std::random_shuffle (mutatedIRs.begin(), mutatedIRs.end());
        
        unsigned long numberofnulled = 0;
        std::srand (std::time(NULL) + clock());  //+ clock() because fast running program will generate same sequence with only time(NULL)
        for (unsigned long irPos=0, irE=mutatedIRs.size(); irPos < irE; ++irPos)
        {
            //randomly select one of its mutant
            auto & mutSet = work_map.at(mutatedIRs[irPos]);
            if (mutSet.empty())
            {
                mutatedIRs[irPos] = nullptr;
                ++numberofnulled;
                continue;
            }
            else
            {
                auto rnd = std::rand() % mutSet.size();
                auto sit = mutSet.begin();
                std::advance(sit, rnd);
                auto selMut = *sit;
                spreadSelectedMutants.push_back(selMut);
                someselected = true;
                
                for (auto *m_ir: mutantDGraph.getIRsOfMut(selMut))
                    work_map.at(m_ir).erase(selMut);
            }
        }
        // remove those that are null
        if (numberofnulled>0)
        {
            std::sort(mutatedIRs.rbegin(), mutatedIRs.rend());  //reverse sort (the null are at  the end: the last numberofnulled values)
            mutatedIRs.erase(mutatedIRs.begin()+(mutatedIRs.size() - numberofnulled), mutatedIRs.end());
        }
    }
    
    assert (spreadSelectedMutants.size() == mutants_number && "Bug in the selection program");   //DBG
    
    if (spreadSelectedMutants.size() > number)
        spreadSelectedMutants.resize(number);
        
    /******/
    
    /// \brief Dummy Random
    for (MuLL::MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id)
    {
        dummySelectedMutants.push_back(mutant_id);
    }
    
    //shuffle
    std::srand (std::time(NULL) + clock());  //+ clock() because fast running program will generate same sequence with only time(NULL)
    std::random_shuffle (dummySelectedMutants.begin(), dummySelectedMutants.end());
    
    //keep only the needed number
    if (dummySelectedMutants.size() > number)
        dummySelectedMutants.resize(number);
}

void MutantSelection::randomSDLMutants (std::vector<MuLL::MutantIDType> &selectedMutants, unsigned long number)
{
    /// Insert all SDL mutants here, note that any two SDL spawn different IRs
    MuLL::MutantIDType mutants_number = mutantInfos.getMutantsNumber();
    for (MuLL::MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id)
    {
        std::string nametype = mutantInfos.getMutantTypeName(mutant_id);
        if (UserMaps::containsDeleteStmtConfName(nametype))
            selectedMutants.push_back(mutant_id);
    }
    
    //shuffle
    std::srand (std::time(NULL) + clock());  //+ clock() because fast running program will generate same sequence with only time(NULL)
    std::random_shuffle (selectedMutants.begin(), selectedMutants.end());
    
    //keep only the needed number
    if (selectedMutants.size() > number)
        selectedMutants.resize(number);
}
