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
  struct MutantDepends {
    std::unordered_set<MutantIDType> outDataDependents; // this ---> x
    std::unordered_set<MutantIDType> inDataDependents;  // x ---> this
    std::unordered_set<MutantIDType> outCtrlDependents; // this ---> x
    std::unordered_set<MutantIDType> inCtrlDependents;  // x ---> this

    // Shared IR (mutants on same statement)
    std::unordered_set<MutantIDType> tieDependents;

    unsigned cfgDepth = 0;   // number of BBs until its BBs following CFG
    unsigned cfgPredNum = 0; // number of predecessors BBs
    unsigned cfgSuccNum = 0; // number of successors BBs
    unsigned complexity = 0; // number of IRs in its expression

    std::string stmtBBTypename;
    std::string mutantTypename;
    std::unordered_set<std::string> astParentsOpcodeNames;
    std::unordered_set<MutantIDType> astParentsMutants;

    // Higerer hop deps
    std::vector<std::unordered_set<MutantIDType>> higherHopsOutDataDependents;
    std::vector<std::unordered_set<MutantIDType>> higherHopsInDataDependents;
    // std::vector<std::unordered_set<MutantIDType>>
    // higherHopsOutCtrlDependents;
    // std::vector<std::unordered_set<MutantIDType>> higherHopsInCtrlDependents;
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
  void setCFGDepthPredSuccNum(MutantIDType id, unsigned depth, unsigned prednum,
                              unsigned succnum) {
    mutantDGraphData[id].cfgDepth = depth;
    mutantDGraphData[id].cfgPredNum = prednum;
    mutantDGraphData[id].cfgSuccNum = succnum;
  }

  void setComplexity(MutantIDType id, unsigned cplx) {
    mutantDGraphData[id].complexity = cplx;
  }

  void setStmtBBTypename(MutantIDType id, std::string bbtypename) {
    mutantDGraphData[id].stmtBBTypename.assign(bbtypename);
  }

  void setMutantTypename(MutantIDType id, std::string const &tname) {
    mutantDGraphData[id].mutantTypename.assign(tname);
    assert(mutantDGraphData[id].mutantTypename.length() > 0 &&
           "Empty mutant TypeName");
  }

  void addAstParentsMutants(MutantIDType id, MutantIDType pId) {
    mutantDGraphData[id].astParentsMutants.insert(pId);
  }

  void addAstParentsOpcodeNames(MutantIDType id, std::string opcode) {
    mutantDGraphData[id].astParentsOpcodeNames.insert(opcode);
  }

  void addAstParents(MutantIDType id, llvm::Instruction const *astparent) {
    mutantDGraphData[id].astParentsOpcodeNames.insert(
        astparent->getOpcodeName());
    if (IR2mutantset.count(astparent) > 0)
      mutantDGraphData[id].astParentsMutants.insert(
          IR2mutantset.at(astparent).begin(), IR2mutantset.at(astparent).end());
  }

  std::unordered_set<MutantIDType> &
  addHigherHopsOutDataDependents(MutantIDType id) {
    mutantDGraphData[id].higherHopsOutDataDependents.emplace_back();
    return mutantDGraphData[id].higherHopsOutDataDependents.back();
  }

  std::unordered_set<MutantIDType> &
  addHigherHopsInDataDependents(MutantIDType id) {
    mutantDGraphData[id].higherHopsInDataDependents.emplace_back();
    return mutantDGraphData[id].higherHopsInDataDependents.back();
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
  std::string const &getMutantTypename(MutantIDType id) const {
    assert(mutantDGraphData[id].mutantTypename.length() > 0 &&
           "Mutant Typename must not be empty");
    return mutantDGraphData[id].mutantTypename;
  }
  std::string const &getStmtBBTypename(MutantIDType id) const {
    return mutantDGraphData[id].stmtBBTypename;
  }

  std::unordered_set<MutantIDType> const &
  getAstParentsMutants(MutantIDType id) const {
    return mutantDGraphData[id].astParentsMutants;
  }

  std::unordered_set<std::string> const &
  getAstParentsOpcodeNames(MutantIDType id) const {
    return mutantDGraphData[id].astParentsOpcodeNames;
  }

  unsigned getComplexity(MutantIDType id) const {
    return mutantDGraphData[id].complexity;
  }

  unsigned getCfgDepth(MutantIDType id) const {
    return mutantDGraphData[id].cfgDepth;
  }

  unsigned getCfgPredNum(MutantIDType id) const {
    return mutantDGraphData[id].cfgPredNum;
  }

  unsigned getCfgSuccNum(MutantIDType id) const {
    return mutantDGraphData[id].cfgSuccNum;
  }

  const std::unordered_set<MutantIDType> &
  getHopsOutDataDependents(MutantIDType mutant_id, unsigned hop) {
    if (hop == 1)
      return getOutDataDependents(mutant_id);
    return mutantDGraphData[mutant_id].higherHopsOutDataDependents[hop - 2];
  }

  const std::unordered_set<MutantIDType> &
  getHopsInDataDependents(MutantIDType mutant_id, unsigned hop) {
    if (hop == 1)
      return getInDataDependents(mutant_id);
    return mutantDGraphData[mutant_id].higherHopsInDataDependents[hop - 2];
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
