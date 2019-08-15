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

#ifndef __MART_GENMU_mutantsSelection_MutantSelection__
#define __MART_GENMU_mutantsSelection_MutantSelection__

#include <unordered_set>

namespace dg {
class LLVMDependenceGraph;
}

#include "../typesops.h" //JsonBox
#include "../usermaps.h"

namespace mart {
namespace selection {

class PredictionModule {
  std::string modelFilename;
  void fastBDTPredict(std::vector<std::vector<float>> const &X_matrix,
                      std::fstream &in_stream,
                      std::vector<float> &prediction);
  std::map<unsigned long, double> fastBDTTrain(std::fstream &out_stream,
                    std::vector<std::vector<float>> const &X_matrix,
                    std::vector<bool> const &isCoupled, std::vector<float> const &weights,
                    unsigned treeNumber = 5000, unsigned treeDepth = 5);
  void randomForestPredict(std::vector<std::vector<float>> const &X_matrix,
                           std::fstream &in_stream,
                           std::vector<float> &prediction);
  void randomForestTrain(std::fstream &out_stream,
                         std::vector<std::vector<float>> const &X_matrix,
                         std::vector<bool> const &isCoupled,
                         unsigned treeNumber = 10);

public:
  PredictionModule(std::string modelfile) : modelFilename(modelfile) {}
  /// make prediction for data in @param X_matrix and put the results into
  /// prediction
  /// Each contained vector correspond to a feature
  void predict(std::vector<std::vector<float>> const &X_matrix, std::vector<std::string> const &featuresnames, std::vector<float> &prediction);

  /// Train model and write model into predictionModelFilename
  /// Each contained vector correspond to a feature
  std::map<unsigned long, double> train(std::vector<std::vector<float>> const &X_matrix, std::vector<std::string> const &modelFeaturesnames, std::vector<bool> const &isCoupled, std::vector<float> const &weights, unsigned treeNumber = 1000, unsigned treeDepth=3);
  
}; // PredictionModule

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

    // Children Context
    unsigned ccHasLiteralChild = 0;
    unsigned ccHasIdentifierChild = 0;
    unsigned ccHasOperatorChild = 0;
    
    // Data type context: vector is operands
    std::vector <std::string> dtcOperands;
    std::string dtcReturn;

    std::string stmtBBTypename;
    std::string mutantTypename;
    std::unordered_set<std::string> astParentsOpcodeNames;
    std::unordered_set<MutantIDType> astParentsMutants;

    // Higerer hop deps
    std::vector<std::unordered_set<MutantIDType>> higherHopsOutDataDependents;
    std::vector<std::unordered_set<MutantIDType>> higherHopsInDataDependents;
    std::vector<std::unordered_set<MutantIDType>> higherHopsOutCtrlDependents;
    std::vector<std::unordered_set<MutantIDType>> higherHopsInCtrlDependents;

    // containg for out dependent reachable mutants, their ID and the strength
    // of thier relationship (proba of reaching them from this mutant theough DD)
    // this do not include mutants tie dependent to this
    std::unordered_map<MutantIDType, double> relationOutgoing;
    std::unordered_map<MutantIDType, double> relationIncoming;
  };

private:
  llvm::Module const *usedModule;
  std::vector<MutantDepends> mutantDGraphData; // Adjacent List
  std::vector<std::unordered_set<MutantIDType>> DDClusters;
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

  std::vector<std::string> &getOperandDataTypeContextRef(MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].dtcOperands;
  }

  void setReturnDataTypeContext(MutantIDType mutant_id, std::string str) {
    mutantDGraphData[mutant_id].dtcReturn.assign(str);
  }

  void setChildContext(MutantIDType id, unsigned nLiteral, unsigned nIdentifier, unsigned nOperator) {
    mutantDGraphData[id].ccHasLiteralChild = nLiteral;
    mutantDGraphData[id].ccHasIdentifierChild = nIdentifier;
    mutantDGraphData[id].ccHasOperatorChild = nOperator;
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

  std::unordered_set<MutantIDType> &
  addHigherHopsOutCtrlDependents(MutantIDType id) {
    mutantDGraphData[id].higherHopsOutCtrlDependents.emplace_back();
    return mutantDGraphData[id].higherHopsOutCtrlDependents.back();
  }

  std::unordered_set<MutantIDType> &
  addHigherHopsInCtrlDependents(MutantIDType id) {
    mutantDGraphData[id].higherHopsInCtrlDependents.emplace_back();
    return mutantDGraphData[id].higherHopsInCtrlDependents.back();
  }

  /*void addInDataRelationStrength (MutantIDType midSrc, MutantIDType midTarget, double val) {
    mutantDGraphData[midSrc].relationIncoming.emplace(midTarget, val);
  }

  void addOutDataRelationStrength (MutantIDType midSrc, MutantIDType midTarget, double val) {
    mutantDGraphData[midSrc].relationOutgoing.emplace(midTarget, val);
  }*/

  inline std::unordered_map<MutantIDType, double> &getRelationIncomingRef (MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].relationIncoming;
  }

  inline std::unordered_map<MutantIDType, double> &getRelationOutgoingRef (MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].relationOutgoing;
  }

  inline std::unordered_map<MutantIDType, double> const &getRelationIncomingConstRef (MutantIDType mutant_id) const {
    return mutantDGraphData[mutant_id].relationIncoming;
  }

  inline std::unordered_map<MutantIDType, double> const &getRelationOutgoingConstRef (MutantIDType mutant_id) const  {
    return mutantDGraphData[mutant_id].relationOutgoing;
  }

  void getInOutDataRelationStrengthMutantsOfInto (MutantIDType mid, std::unordered_set<MutantIDType> &copyTo) {
    for (auto &inMPair :mutantDGraphData[mid].relationIncoming)
      copyTo.insert(inMPair.first);
    for (auto &outMPair :mutantDGraphData[mid].relationOutgoing)
      copyTo.insert(outMPair.first);
  }

  // return the cluster id (position in vector)
  unsigned long createNewDDCluster() {
    DDClusters.emplace_back();
    return DDClusters.size() - 1;
  }

  std::unordered_set<MutantIDType> *getDDClusterAt(unsigned long cid) {
    return &(DDClusters.at(cid));
  }

  /// Make MCL expansions for a adjacency matrix
  // void graphMCL (std::vector<std::vector<float>> &adjacencyMatrix);

  /// make a graph using DD, CD as edges (higher edge weight to DD). Each node
  /// on the graph correspond to tie dependent mutants
  // void computeMarkovCLusters (std::vector<std::vector<MutantIDType>>
  // &clusters);

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

  template <typename T>
  void getTSetOfMutSet(std::unordered_set<MutantIDType> const &mutset,
                          std::unordered_map<MutantIDType, std::unordered_set<T>> mutant2Tset,
                          std::unordered_set<T> &Tset) {
    for (auto mid: mutset) {
      auto &s = mutant2Tset[mid];
      Tset.insert(s.begin(), s.end());
    }
  }

  // if @param featuresnames is not null put the ordered feature names in it
  void computeMutantFeatures(std::vector<std::vector<float>> &featuresmatrix,
                             std::vector<std::string> &featuresnames);
  // if @param featuresnames is not null put the ordered feature names in it
  void computeStatementFeatures(std::vector<std::vector<float>> &featuresmatrix,
                             std::vector<std::string> &featuresnames,
                             std::vector<std::unordered_set<MutantIDType>> &stmt2muts);

  bool build(llvm::Module const &mod, dg::LLVMDependenceGraph const *irDg,
             MutantInfoList const &mutInfos,
             std::string mutant_depend_filename,
             bool disable_selection);

  void dump(std::string filename);

  void load(std::string filename, MutantInfoList const &mutInfos);

  void exportMutantFeaturesCSV(std::string filenameCSV, 
                               MutantInfoList const &mutInfos, 
                               bool isDefectPrediction);

  // Others
public:
  // void getMutantsOfASTParentOf (MutantIDType id,
  //                         std::vector<MutantIDType> &parentmutants);
  // void getMutantsOfASTChildrenOf (MutantIDType id,
  //                         std::vector<MutantIDType> &childrenmutants);
  inline std::string const &getMutantTypename(MutantIDType id) const {
    assert(mutantDGraphData[id].mutantTypename.length() > 0 &&
           "Mutant Typename must not be empty");
    return mutantDGraphData[id].mutantTypename;
  }
  inline std::string const &getStmtBBTypename(MutantIDType id) const {
    return mutantDGraphData[id].stmtBBTypename;
  }

  void getSplittedMutantTypename(MutantIDType id, std::vector<std::string> &mrNames) const {
    assert(mutantDGraphData[id].mutantTypename.length() > 0 &&
           "Mutant Typename must not be empty");
    static const char sep = '!';
    const std::string &mname = mutantDGraphData[id].mutantTypename;
    auto sepPos = mname.find(sep);
    assert (sepPos != std::string::npos && "Mutant Typename have noe separator");
    mrNames.push_back(mname.substr(0, sepPos)+"-Matcher");
    mrNames.push_back(mname.substr(sepPos+1)+"-Replacer");
  }
  void getSplittedStmtBBTypename(MutantIDType id, std::vector<std::string> &sbNames) const {
    static const char  sep = '.';
    const std::string &sbbname = mutantDGraphData[id].stmtBBTypename;
    auto sepPos = sbbname.find(sep);
    sbNames.push_back(sbbname.substr(0, sepPos)+"-BBType");
    if (sepPos != std::string::npos)
      sbNames.push_back(sbbname.substr(sepPos+1)+"-BBType");
  }

  std::unordered_set<MutantIDType> const &
  getAstParentsMutants(MutantIDType id) const {
    return mutantDGraphData[id].astParentsMutants;
  }

  std::unordered_set<std::string> const &
  getAstParentsOpcodeNames(MutantIDType id) const {
    return mutantDGraphData[id].astParentsOpcodeNames;
  }

  std::vector<std::string> const & getOperandDataTypeContext(MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].dtcOperands;
  }

  std::string const getReturnDataTypeContext(MutantIDType mutant_id) {
    return mutantDGraphData[mutant_id].dtcReturn;
  }

  unsigned getHasLiteralChild(MutantIDType id) const {
    return mutantDGraphData[id].ccHasLiteralChild;
  }

  unsigned getHasIdentifierChild(MutantIDType id) const {
    return mutantDGraphData[id].ccHasIdentifierChild;
  }

  unsigned getHasOperatorChild(MutantIDType id) const {
    return mutantDGraphData[id].ccHasOperatorChild;
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

  const std::unordered_set<MutantIDType> &
  getHopsOutCtrlDependents(MutantIDType mutant_id, unsigned hop) {
    if (hop == 1)
      return getOutCtrlDependents(mutant_id);
    return mutantDGraphData[mutant_id].higherHopsOutCtrlDependents[hop - 2];
  }

  const std::unordered_set<MutantIDType> &
  getHopsInCtrlDependents(MutantIDType mutant_id, unsigned hop) {
    if (hop == 1)
      return getInCtrlDependents(mutant_id);
    return mutantDGraphData[mutant_id].higherHopsInCtrlDependents[hop - 2];
  }

  double getInRelationStrength(MutantIDType fromMid, MutantIDType toMid) {
    return mutantDGraphData[fromMid].relationIncoming.at(toMid);
  }

  double getOutRelationStrength(MutantIDType fromMid, MutantIDType toMid) {
    return mutantDGraphData[fromMid].relationOutgoing.at(toMid);
  }

  std::unordered_map<MutantIDType, double> &getInMutantRelationStrength(MutantIDType fromMid) {
    return mutantDGraphData[fromMid].relationIncoming;
  }

  std::unordered_map<MutantIDType, double> &getOutMutantRelationStrength(MutantIDType fromMid) {
    return mutantDGraphData[fromMid].relationOutgoing;
  }

  std::vector<std::unordered_set<MutantIDType>> &getDDClusters() {
    return DDClusters;
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
                             bool isClassicCtrlDepAlgo = true,
                             bool disable_selection = false);
  MutantIDType pickMutant(std::unordered_set<MutantIDType> const &candidates,
                          std::vector<double> const &scores);
  void relaxMutant(MutantIDType mutant_id, std::vector<double> &scores);
  void
  getMachineLearningPrediction(std::vector<float> &couplingProbabilitiesOut,
                               std::string modelFilename, bool isDefectPrediction);

public:
  MutantSelection(llvm::Module &inMod, MutantInfoList const &mInf,
                  std::string mutant_depend_filename, bool rerundg,
                  bool isFlowSensitive, bool disable_selection=false)
      : subjectModule(inMod), mutantInfos(mInf),
        mutantDGraph(mInf.getMutantsNumber()) {
    buildDependenceGraphs(mutant_depend_filename, rerundg, isFlowSensitive, true, disable_selection);
  }
  void dumpMutantsFeaturesToCSV(std::string csvFilename) {
    mutantDGraph.exportMutantFeaturesCSV(csvFilename, mutantInfos, false /*isDefectPrediction*/);
  }
  void dumpStmtsFeaturesToCSV(std::string csvFilename) {
    mutantDGraph.exportMutantFeaturesCSV(csvFilename, mutantInfos, true /*isDefectPrediction*/);
  }
  void smartSelectMutants(std::vector<MutantIDType> &selectedMutants,
                          // std::vector<double> &selectedScores,
                          std::vector<float> &cachedPrediction,
                          std::string trainedModelFilename, bool mlOn, bool mclOn, 
                          bool defectPredOn);
  void randomMutants(std::vector<MutantIDType> &spreadSelectedMutants,
                     std::vector<MutantIDType> &dummySelectedMutants,
                     unsigned long number);
  void
  randomSDLMutants(std::vector<MutantIDType> &selectedMutants,
                   unsigned long number); // only statement deletion mutants
};                                        // class MutantSelection

} // namespace selection
} // namespace mart

#endif //__MART_GENMU_mutantsSelection_MutantSelection__
