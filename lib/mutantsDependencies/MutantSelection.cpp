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

void MutantSelection::buildDependenceGraphs(bool isFlowSensitive, dg::CD_ALG cd_alg = dg::CLASSIC/*or dg::CONTROL_EXPRESSION*/)
{
    //build IR DGraph
    dg::LLVMPointerAnalysis *PTA = new dg::LLVMPointerAnalysis(subjectModule);
    if (isFlowSensitive)
        PTA->run<dg::analysis::pta::PointsToFlowSensitive>();
    else
        PTA->run<dg::analysis::pta::PointsToFlowInSensitive>();
        
    assert (IRDGraph.build(subjectModule, PTA) && "Error: failed to build dg dependence graph");
    assert(PTA && "BUG: Need points-to analysis");
    
    dg::analysis::rd::LLVMReachingDefinitions RDA(subjectMobule, PTA);
    RDA.run();  // compute reaching definitions
    
    dg::LLVMDefUseAnalysis DUA(&IRDGraph, &RDA, PTA);
    DUA.run(); // add def-use edges according that
    
    delete PTA;
        
    IRDGraph.computeControlDependencies(cd_alg);
    
    //Build mutant DGraph
    mutantDGraph.build(subjectModule, IRDGraph, mutantInfos);
}

unsigned MutantSelection::findFurthestMutants (std::vector<MuLL::MutantIDType> const &fromMuts, std::vector<MuLL::MutantIDType> &results)
{
    
}

void MutantSelection::smartSelectMutants (std::vector<MuLL::MutantIDType> &selectedMutants)
{
    //Choose starting mutants (random for now: 1 mutant per dependency cluster)
    
}

void MutantSelection::randomMutants (std::vector<MuLL::MutantIDType> &spreadSelectedMutants, std::vector<MuLL::MutantIDType> &dummySelectedMutants, unsigned long number)
{  ///TODO TODO: test this function
    MuLL::MutantIDType mutants_number = mutantInfos.getMutantsNumber();
    
    assert (mutantDGraph.isBuilt() && "This function must be called after the mutantDGraph is build. We need mutants IRs");
    
    /// \brief Spread Random
    std::unordered_set<llvm::Value *> seenIRs, mut_notSeen;
    std::vector<MuLL::MutantIDType> no_unselected_IR;  //mutants whose IR have all been selected at least once but have not yet been selected
    std::unordered_map<unsigned, std::vector<MuLL::MutantIDType>> work_map;   //key: number of IRs seen, value: mutants with corresponding number of IRs seen
    
    for (MuLL::MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id)
    {
        work_map[0].push_back(mutant_id);
    }
    
    unsigned current_seen_index = 0
    unsigned num_seen_index = 0;
    
    std::vector<MuLL::MutantIDType> curr_notSelected;
    while (num_seen_index <= work_map.size())
    {
        while (work_map.count(current_seen_index) == 0) ++current_seen_index;
        
        std::random_shuffle (work_map[current_seen_index].begin(), work_map[current_seen_index].end());
        for (auto mutant_id: work_map[current_seen_index])
        {
            auto &mutant_irs = mutantDGraph.getIRsOfMut(mutant_id);
            mut_notSeen.clear();
            for (auto *ir: mutant_irs)
                if (seenIRs.count(ir) == 0) 
                    mut_notSeen.insert(ir);
            if (mutant_irs.size() - mut_notSeen.size() <= current_seen_index)
            {
                spreadSelectedMutants.push_back(mutant_id);
                seenIRs.insert(mut_notSeen.begin(), mut_notSeen.end());
            }
            else
            {
                curr_notSelected.push_back (mutant_id);
            }
        }
        
        for (auto mutant_id: curr_notSelected)
        {
            auto &mutant_irs = mutantDGraph.getIRsOfMut(mutant_id);
            unsigned seen = 0;
            for (auto *ir: mutant_irs)
                if (seenIRs.count(ir) > 0) 
                    ++seen;
            if (seen < mutant_irs.size())
                work_map[seen].push_back(mutant_id);
            else
                no_unselected_IR.push_back(mutant_id);
        }
        
        ++current_seen_index
        ++num_seen_index;
    }
    
    if (! no_unselected_IR.empty())
    {
        std::random_shuffle (no_unselected_IR.begin(), no_unselected_IR.end());
        spreadSelectedMutants.insert(spreadSelectedMutants.begin()+spreadSelectedMutants.size(), no_unselected_IR.begin(), no_unselected_IR.end()); //append
    }
    
    if (spreadSelectedMutants.size() > number)
        spreadSelectedMutants.resize(size);
        
    /******/
    
    /// \brief Dummy Random
    for (MuLL::MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id)
    {
        dummySelectedMutants.push_back(mutant_id);
    }
    
    //shuffle
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
        std::string nametype = mutantInfos.getMutantNameType(mutant_id);
        if (UserMaps::containsDeleteStmtConfName(nametype))
            selectedMutants.push_back(mutant_id);
    }
    
    //shuffle
    std::random_shuffle (selectedMutants.begin(), selectedMutants.end());
    
    //keep only the needed number
    if (selectedMutants.size() > number)
        selectedMutants.resize(number);
}

