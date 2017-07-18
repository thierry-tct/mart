/**
 * -==== MutantSelection.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Implementation of mutant selection based on dependency analysis
 * and ... (this make use of DG - https://github.com/mchalupa/dg).
 */

////Uses graphviz for graph representation --
/// http://www.graphviz.org/pdf/Agraph.pdf  and
/// http://www.graphviz.org/pdf/libguide.pdf
//// Read/write dot files wit agread/agwrite
//// install 'libcgraph6'
//#include "cgraph.h"

#ifndef __MART_SEMU_GENMU_mutantsSelection_MutantSelection__
#define __MART_SEMU_GENMU_mutantsSelection_MutantSelection__

#include <unordered_set>

namespace dg {
class LLVMDependenceGraph;
}

#include "../typesops.h" //JsonBox
#include "../usermaps.h"

namespace mart {
namespace selection {

class MutantDependenceGraph //: public DependenceGraph<MutantNode>
{
  enum ASTType {
    IF_COND = 1,
    LOOP_COND = 2,
    MEM_PTR_READ = 4,
    WRITE_MEM = 8,
    CALL_FUNC = 16,
    RETURN = 32
  };

  struct MutantDepends {
    std::unordered_set<MutantIDType> outDataDependents; // this ---> x
    std::unordered_set<MutantIDType> inDataDependents;  // x ---> this
    std::unordered_set<MutantIDType> outCtrlDependents; // this ---> x
    std::unordered_set<MutantIDType> inCtrlDependents;  // x ---> this

    // Shared IR (mutants on same statement)
    std::unordered_set<MutantIDType> tieDependents;

    unsigned cfgDepth;   // number of BBs until its BBs following CFG
    unsigned cfgPredNum; // number of predecessors BBs
    unsigned cfgSuccNum; // number of successors BBs
    unsigned complexity; // number of IRs in its expression

    unsigned stmtASTType = 0; // ASTType of this mutant's statement
    std::string mutantTypename;
  };

private:
  std::vector<MutantDepends> mutantDGraphData; // Adjacent List
  std::unordered_map<MutantIDType, std::unordered_set<llvm::Value const *>>
      mutant2IRset;
  std::unordered_map<llvm::Value const *, std::unordered_set<MutantIDType>>
      IR2mutantset;

  void addDataDependency(MutantIDType fromID, MutantIDType toID) {
    bool isNew = mutantDGraphData[fromID].outDataDependents.insert(toID).second;
    // assert (isNew && "Error: mutant inserted twice as another's out Data.");
    isNew = mutantDGraphData[toID].inDataDependents.insert(fromID).second;
    // assert (isNew && "Error: mutant inserted twice as another's in Data.");
  }

  void addCtrlDependency(MutantIDType fromID, MutantIDType toID) {
    bool isNew = mutantDGraphData[fromID].outCtrlDependents.insert(toID).second;
    // assert (isNew && "Error: mutant inserted twice as another's out Ctrl.");
    isNew = mutantDGraphData[toID].inCtrlDependents.insert(fromID).second;
    // assert (isNew && "Error: mutant inserted twice as another's in CTRL.");
  }

  void addTieDependency(MutantIDType id1, MutantIDType id2) {
    bool isNew = mutantDGraphData[id1].tieDependents.insert(id2).second;
    // assert (isNew && "Error: mutant inserted twice as another's tie.");
    isNew = mutantDGraphData[id2].tieDependents.insert(id1).second;
    // assert (isNew && "Error: mutant inserted twice as another's tie.");
  }

  void addDataCtrlFor(dg::LLVMDependenceGraph const *subIRDg);

  // Others
  void setCFGDepthPredSucNum(MutantIDType id, unsigned depth, unsigned prednum,
                             unsigned succnum) {
    mutantDGraphData[id].cfgDepth = depth;
    mutantDGraphData[id].cfgPredNum = prednum;
    mutantDGraphData[id].cfgSuccNum = succnum;
  }

  void setComplexity(MutantIDType id, unsigned cplx) {
    mutantDGraphData[id].complexity = cplx;
  }

  void addstmtASTType(MutantIDType id, enum ASTType asttype) {
    mutantDGraphData[id].stmtASTType |= asttype;
  }

  void setMutantTypename(MutantIDType id, std::string const &tname) {
    mutantDGraphData[id].mutantTypename = tname;
  }

public:
  MutantDependenceGraph(MutantIDType nMuts) {
    mutantDGraphData.resize(nMuts + 1);
  }

  std::unordered_map<llvm::Value const *,
                     std::unordered_set<MutantIDType>> const &
  getIR2mutantsetMap() const {
    return IR2mutantset;
  }
  std::unordered_set<llvm::Value const *> &getIRsOfMut(MutantIDType id) {
    return mutant2IRset.at(id);
  }
  bool isBuilt() { return (!mutant2IRset.empty()); }

  MutantIDType getMutantsNumber() { return mutantDGraphData.size() - 1; }

  const std::unordered_set<MutantIDType> &
  getOutDataDependents(MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].outDataDependents;
  }
  const std::unordered_set<MutantIDType> &
  getInDataDependents(MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].inDataDependents;
  }
  const std::unordered_set<MutantIDType> &
  getOutCtrlDependents(MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].outCtrlDependents;
  }
  const std::unordered_set<MutantIDType> &
  getInCtrlDependents(MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].inCtrlDependents;
  }

  const std::unordered_set<MutantIDType> &
  getTieDependents(MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].tieDependents;
  }

  bool build(llvm::Module const &mod, dg::LLVMDependenceGraph const *irDg,
             MutantInfoList const &mutInfos,
             std::string mutant_depend_filename);

  void dump(std::string filename);

  void load(std::string filename, MutantInfoList const &mutInfos);

  // Others
public:
  // void getMutantsOfASTParentOf (MutantIDType id,
  //                         std::vector<MutantIDType> &parentmutants);
  // void getMutantsOfASTChildrenOf (MutantIDType id,
  //                         std::vector<MutantIDType> &childrenmutants);
  std::string const &getMutantName(MutantIDType id) const {
    return mutantDGraphData[id].mutantTypename;
  }

}; // class MutantDependenceGraph

class MutantSelection {
private:
  llvm::Module &subjectModule;
  MutantInfoList const &mutantInfos;

  // dg::LLVMDependenceGraph *IRDGraph;
  MutantDependenceGraph mutantDGraph;

  ////
  void buildDependenceGraphs(std::string mutant_depend_filename, bool rerundg,
                             bool isFlowSensitive = false,
                             bool isClassicCtrlDepAlgo = true);
  MutantIDType pickMutant(std::unordered_set<MutantIDType> const &candidates,
                          std::vector<double> const &scores);
  void relaxMutant(MutantIDType mutant_id, std::vector<double> &scores);

public:
  MutantSelection(llvm::Module &inMod, MutantInfoList const &mInf,
                  std::string mutant_depend_filename, bool rerundg,
                  bool isFlowSensitive)
      : subjectModule(inMod), mutantInfos(mInf),
        mutantDGraph(mInf.getMutantsNumber()) {
    buildDependenceGraphs(mutant_depend_filename, rerundg, isFlowSensitive);
  }
  void smartSelectMutants(std::vector<MutantIDType> &selectedMutants,
                          std::vector<double> &selectedScores);
  void randomMutants(std::vector<MutantIDType> &spreadSelectedMutants,
                     std::vector<MutantIDType> &dummySelectedMutants,
                     unsigned long number);
  void
  randomSDLMutants(std::vector<MutantIDType> &selectedMutants,
                   unsigned long number); // only statement deletion mutants
};                                        // class MutantSelection

} // namespace selection
} // namespace mart

#endif //__MART_SEMU_GENMU_mutantsSelection_MutantSelection__
