//===-- mart/lib/MutantSelection.cpp - Implements FaRM Mutant Selection. --===//
//
//                MART Multi-Language LLVM Mutation Framework
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of FaRM, a mutant selection, 
/// based on machine learning with program elements as features 
/// (e.g: statement complexity, number of data dependencies, depth in CFG,...)
/// This make use of DG - https://github.com/mchalupa/dg
///
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cstdlib> /* srand, rand */
#include <ctime>
#include <fstream>

#include "llvm/Analysis/CFG.h"

#include "MutantSelection.h"

// https://github.com/thomaskeck/FastBDT
#include "../third-parties/FastBDT/include/Classifier.h" //FastBDT

// https://github.com/bjoern-andres/random-forest
#include "../third-parties/random-forest/include/andres/marray.hxx"
#include "../third-parties/random-forest/include/andres/ml/decision-trees.hxx"

#ifndef ENABLE_CFG
#define ENABLE_CFG // needed by dg
#endif
// https://github.com/mchalupa/dg
//#include "../third-parties/dg/src/llvm/LLVMDependenceGraph.h"
#include "../third-parties/dg/include/dg/llvm/LLVMDependenceGraph.h"
//#include "../third-parties/dg/src/llvm/analysis/DefUse.h"
#include "../third-parties/dg/lib/llvm/analysis/DefUse/DefUse.h"
//#include "../third-parties/dg/src/llvm/analysis/PointsTo/PointsTo.h"
#include "../third-parties/dg/include/dg/llvm/analysis/PointsTo/PointerAnalysis.h"
//#include "../third-parties/dg/src/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"
#include "../third-parties/dg/include/dg/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"

//#include "../third-parties/dg/src/analysis/PointsTo/PointsToFlowInsensitive.h"
#include "../third-parties/dg/include/dg/analysis/PointsTo/PointerAnalysisFI.h"
//#include "../third-parties/dg/src/analysis/PointsTo/PointsToFlowSensitive.h"
#include "../third-parties/dg/include/dg/analysis/PointsTo/PointerAnalysisFS.h"

#include "../third-parties/dg/include/dg/llvm/LLVMDependenceGraphBuilder.h"
#include "../third-parties/dg/include/dg/llvm/analysis/PointsTo/LLVMPointerAnalysisOptions.h"

using namespace mart;
using namespace mart::selection;

// Amplifier help reduce the different factors...
#define AMPLIFIER 10000000

namespace {
const double MAX_SCORE = 1.0;
const double RELAX_STEP = 0.05 / AMPLIFIER;
//const double RELAX_THRESHOLD = 0.01 / AMPLIFIER; // 5 hops
const double RELAX_THRESHOLD = 0.05 / AMPLIFIER; // 0 hops
const double TIE_REDUCTION_DIFF = 0.03 / AMPLIFIER;
}

void PredictionModule::fastBDTPredict(
    std::vector<std::vector<float>> const &X_matrix,
    std::fstream &in_stream,
    std::vector<float> &prediction) {
  FastBDT::Classifier classifier(in_stream);
  std::vector<float> event;
  event.reserve(X_matrix.size());
  unsigned long long eventNumber = X_matrix.back().size();
  for (unsigned long long eIndex = 0; eIndex < eventNumber; ++eIndex) {
    event.clear();
    for (auto &feature : X_matrix)
      event.push_back(feature[eIndex]);
    prediction.push_back(classifier.predict(event));
  }
}

std::map<unsigned long, double> PredictionModule::fastBDTTrain(
    std::fstream &out_stream,
    std::vector<std::vector<float>> const &X_matrix,
    std::vector<bool> const &isCoupled, std::vector<float> const &weights, unsigned treeNumber, unsigned treeDepth) {
  assert(!X_matrix.empty() && !isCoupled.empty() &&
         "Error: calling train with empty data");
  //std::vector<float> weights(X_matrix.back().size(), 1.0);
  FastBDT::Classifier classifier;
  classifier.SetNTrees(treeNumber);
  classifier.SetDepth(treeDepth);
  classifier.fit(X_matrix, isCoupled, weights);
  // std::cout << "Score " << GetIrisScore(classifier) << std::endl;
  out_stream << classifier << std::endl;
  
  return classifier.GetVariableRanking();
}

void PredictionModule::randomForestPredict(
    std::vector<std::vector<float>> const &X_matrix,
    std::fstream &in_stream,
    std::vector<float> &prediction) {
  andres::ml::DecisionForest<double, unsigned char, double> decisionForest;
  decisionForest.deserialize(in_stream);
  auto nFeatures = X_matrix.size();
  auto nEvents = X_matrix.back().size();
  andres::Matrix<double> features(nEvents, nFeatures);
  andres::Vector<double> probabilities;
  for (unsigned col = 0; col < nFeatures; ++col)
    for (MutantIDType row = 0, lastr = nEvents; row < lastr; ++row)
      features(row, col) = X_matrix[col][row];
  decisionForest.predict(features, probabilities);

  for (auto p_val : probabilities)
    prediction.push_back(p_val);
}

void PredictionModule::randomForestTrain(
    std::fstream &out_stream,
    std::vector<std::vector<float>> const &X_matrix,
    std::vector<bool> const &isCoupled, unsigned treeNumber) {
  assert(!X_matrix.empty() && !isCoupled.empty() &&
         "Error: calling train with empty data");
  std::vector<float> weights(X_matrix.back().size(), 1.0);
  andres::ml::DecisionForest<double, unsigned char, double> decisionForest;
  const size_t numberOfDecisionTrees = treeNumber; /// 10
  auto nFeatures = X_matrix.size();
  auto nEvents = X_matrix.back().size();
  andres::Matrix<double> features(nEvents, nFeatures);
  andres::Vector<unsigned char> labels(isCoupled.size());
  for (MutantIDType row = 0, lastr = nEvents; row < lastr; ++row)
    labels[row] = (char)isCoupled[row];
  for (unsigned col = 0; col < nFeatures; ++col)
    for (MutantIDType row = 0, lastr = nEvents; row < lastr; ++row)
      features(row, col) = X_matrix[col][row];
  decisionForest.learn(features, labels, numberOfDecisionTrees);
  decisionForest.serialize(out_stream);
}

/// make prediction for data in @param X_matrix and put the results into
/// prediction
/// Each contained vector correspond to a feature
void PredictionModule::predict(std::vector<std::vector<float>> const &X_matrix,
                               std::vector<std::string> const &featuresnames,
                               std::vector<float> &prediction) {
  std::vector<std::string> modelFeaturesnames;
  std::string line;
  std::fstream in_stream(modelFilename, std::ios_base::in);
  //get list of feature in the model
  std::getline(in_stream, line);
  std::istringstream ss(line);
  while (ss.good()) {
    std::string fstr;
    ss >> fstr;
    modelFeaturesnames.push_back(fstr);
  }
  // rearrange the data to map model (warning about feature not in model)
  std::vector<std::vector<float>> finalFeatures;
  finalFeatures.reserve(modelFeaturesnames.size());
  std::unordered_map<std::string, size_t> fname2pos;
  for (size_t pos=0; pos<featuresnames.size(); ++pos)
    fname2pos[featuresnames[pos]] = pos;
  auto nEvents = X_matrix.back().size();
  std::unordered_set<std::string> seeninmodel;
  for (auto fname: modelFeaturesnames) {
    if (fname2pos.count(fname)) {
      finalFeatures.push_back(X_matrix[fname2pos[fname]]);
      seeninmodel.insert(fname);
    } else {
      finalFeatures.push_back(std::vector<float>(nEvents, 0.0));
    }
  }
  for (auto fname: featuresnames)
    if (seeninmodel.count(fname) == 0)
      llvm::errs() << "Warning: feature not in model: " << fname << "\n";

  fastBDTPredict(finalFeatures, in_stream, prediction);
  // randomForestPredict(finalFeatures, in_stream, prediction);
}

/// Train model and write model into predictionModelFilename
/// Each contained vector correspond to a feature
std::map<unsigned long, double> PredictionModule::train(std::vector<std::vector<float>> const &X_matrix,
                             std::vector<std::string> const &modelFeaturesnames,
                             std::vector<bool> const &isCoupled,
                             std::vector<float> const  &weights,
                             unsigned treeNumber, unsigned treeDepth) {
  std::fstream out_stream(modelFilename,
                          std::ios_base::out | std::ios_base::trunc);
  // dump feature list (to match feature during prediction)                        
  for (auto &fstr: modelFeaturesnames)
    out_stream << " " << fstr ;  //istringstrem will skip first space
  out_stream << "\n";
  
  std::map<unsigned long, double> featuremap = fastBDTTrain(out_stream, X_matrix, isCoupled, weights, treeNumber, treeDepth);
  // randomForestTrain(out_stream, X_matrix, isCoupled. treeNumber);
  out_stream.close();
  return featuremap;
}

// class MutantDependenceGraph
/// This Function is written using dg's DG2Dot.h... as reference
void MutantDependenceGraph::addDataCtrlFor(
    dg::LLVMDependenceGraph const *subIRDg) {
  for (auto nodeIt = subIRDg->begin(), ne = subIRDg->end(); nodeIt != ne;
       ++nodeIt) {
    auto *nodefrom = nodeIt->second;
    llvm::Value *irFrom = nodefrom->getKey();
    if (IR2mutantset.count(irFrom) == 0) 
      continue;
    for (auto ndata = nodefrom->data_begin(), nde = nodefrom->data_end();
         ndata != nde; ++ndata) {
      llvm::Value *irDataTo = (*ndata)->getKey();
      if (IR2mutantset.count(irDataTo) == 0) 
        continue;
      for (MutantIDType m_id_from : IR2mutantset[irFrom])
        for (MutantIDType m_id_datato : IR2mutantset[irDataTo])
          if (m_id_from != m_id_datato)
            addDataDependency(m_id_from, m_id_datato);
    }
    for (auto nctrl = nodefrom->control_begin(), nce = nodefrom->control_end();
         nctrl != nce; ++nctrl) {
      llvm::Value *irCtrlTo = (*nctrl)->getKey();
      if (IR2mutantset.count(irCtrlTo) == 0) 
        continue;
      for (MutantIDType m_id_from : IR2mutantset[irFrom])
        for (MutantIDType m_id_ctrlto : IR2mutantset[irCtrlTo])
          if (m_id_from != m_id_ctrlto)
            addCtrlDependency(m_id_from, m_id_ctrlto);
    }
  }
  /// dg mainly store Control dependencies here (between BBs)
  for (auto it : subIRDg->getBlocks()) {
    auto *dgbbFrom = it.second;
    for (auto dgbbTo : dgbbFrom->controlDependence()) {
      llvm::Value *irFrom = dgbbFrom->getLastNode()->getKey();
      llvm::Value *irCtrlTo = dgbbTo->getFirstNode()->getKey();

      // XXX as irFrom is a terminator instruction, and do not really have value
      // we make it's operands as ctrl dependency source as well
      llvm::SmallVector<llvm::Value *, 2> fromIRs;
      fromIRs.push_back(irFrom); // add terminator
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
      for (auto II = llvm::dyn_cast<llvm::User>(irFrom)->op_begin(), IE = llvm::dyn_cast<llvm::User>(irFrom)->op_end(); II != IE; ++II) {

        if (llvm::isa<llvm::Instruction>(*II))
          fromIRs.push_back(*II);
      }
#else
      for (llvm::Value *oprd :
           llvm::dyn_cast<llvm::User>(irFrom)->operand_values())
        if (llvm::isa<llvm::Instruction>(oprd))
          fromIRs.push_back(oprd);
#endif

      for (llvm::Value *cvFrom : fromIRs) {
        if (IR2mutantset.count(cvFrom) == 0) 
          continue;
        // if it donesn't have parent it wont be mutated anyway 
        // (seems dg add ret void to the code and that have no parent...)
        if (auto *ictBB = llvm::dyn_cast<llvm::Instruction>(irCtrlTo)->getParent()) {
          for (auto &cinstTo : *ictBB) {
            if (IR2mutantset.count(&cinstTo) == 0) 
              continue;
            for (MutantIDType m_id_from : IR2mutantset[cvFrom])
              for (MutantIDType m_id_ctrlto : IR2mutantset[&cinstTo])
                if (m_id_from != m_id_ctrlto)
                  addCtrlDependency(m_id_from, m_id_ctrlto);
          }
        }
      }
    }

    // XXX Should we consider Pos-Dominators as dependencies??
    /*for (auto *dgbbTo : dgbbFrom->getPostDomFrontiers()) {
      llvm::Value *irFrom = dgbbFrom->getFirstNode()->getKey();
      llvm::Value *irCtrlTo = dgbbTo->getLastNode()->getKey();
      if (IR2mutantset.count(irFrom) == 0 || IR2mutantset.count(irCtrlTo) == 0) 
        continue;
      for (MutantIDType m_id_from : IR2mutantset[irFrom])
        for (MutantIDType m_id_ctrlto : IR2mutantset[irCtrlTo])
          if (m_id_from != m_id_ctrlto)
            addCtrlDependency(m_id_from, m_id_ctrlto);
    }*/
  }
}

bool MutantDependenceGraph::build(llvm::Module const &mod,
                                  dg::LLVMDependenceGraph const *irDg,
                                  MutantInfoList const &mutInfos,
                                  std::string mutant_depend_filename,
                                  bool disable_selection) {
  usedModule = &mod;
  std::unordered_map<std::string,
                     std::unordered_map<unsigned, llvm::Value const *>>
      functionName_position2IR;

  // First compute IR2mutantset
  for (auto &Func : *usedModule) {
    unsigned instPosition = 0;
    std::string funcName = Func.getName();
    // functionName_position2IR.clear();        //XXX: Since each mutant belong
    // to only one function
    for (auto &BB : Func) {
      for (auto &Inst : BB) {
        if (!functionName_position2IR[funcName]
                 .insert(std::pair<unsigned, llvm::Value const *>(instPosition,
                                                                  &Inst))
                 .second) {
          llvm::errs() << "\nError: Each position must correspond to a single "
                          "IR Instruction\n\n";
          assert(false);
        }
        ++instPosition;
      }
    }
  }

  // Compute mutant2IRset and IR2mutantset
  MutantIDType mutants_number = mutInfos.getMutantsNumber();
  for (MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id) {
    auto &tmpPos2IRMap =
        functionName_position2IR.at(mutInfos.getMutantFunction(mutant_id));
    for (auto mutPos : mutInfos.getMutantIrPosInFunction(mutant_id)) {
      llvm::Value const *ir_at_pos = tmpPos2IRMap.at(mutPos);
      mutant2IRset[mutant_id].insert(ir_at_pos);
      IR2mutantset[ir_at_pos].insert(mutant_id);
    }
  }

  // Build the mutant dependence graph

  // Add ctrl and data dependency
  if (irDg) {
    // Add tie dependents
    std::vector<MutantIDType> tmpIds;
    for (auto &pair_val : IR2mutantset) {
      llvm::Value const *inst = pair_val.first;
      tmpIds.clear();
      for (MutantIDType mutant_id : IR2mutantset[inst])
        tmpIds.push_back(mutant_id);
      for (auto m_id_it = tmpIds.begin(), id_e = tmpIds.end(); m_id_it != id_e;
           ++m_id_it)
        for (auto tie_id_it = m_id_it + 1; tie_id_it != id_e; ++tie_id_it)
          addTieDependency(*m_id_it, *tie_id_it);
    }

    // compute by using the dependence graph of dg and dump resulting mutant
    // dependencies
    const std::map<llvm::Value *, dg::LLVMDependenceGraph *> &CF =
        dg::getConstructedFunctions();
    for (auto &funcdg : CF)
      addDataCtrlFor(funcdg.second);

    // Add others (typename, complexity, cfgdepth, ast type,...)
    // cfgDepth, prednum, succnum
    std::unordered_map<const llvm::BasicBlock *, unsigned> block_depth_map;
    std::queue<const llvm::BasicBlock *> workQ;
    /// XXX: it could be possible that some mutants are not visted bellow
    /// because their Basic block is unreachable, but all those are removed by
    /// the compiler front-end, so it is actually impossible
    for (auto &func : *usedModule) {
      if (func.isDeclaration())
        continue;
      // workQ.clear();
      block_depth_map.clear();
      auto &entryBB = func.getEntryBlock();
      workQ.push(&entryBB);
      block_depth_map.emplace(&entryBB, 1);
      while (!workQ.empty()) { // BFS
        auto *bb = workQ.front();
        auto depth = block_depth_map.at(bb);
        workQ.pop();
        unsigned nSuccs = 0;
        unsigned nPreds = 0;
        for (auto succIt = llvm::succ_begin(bb), succE = llvm::succ_end(bb);
             succIt != succE; ++succIt) {
          if (block_depth_map.count(*succIt) == 0) {
            block_depth_map.emplace(*succIt, depth + 1);
            workQ.push(*succIt);
          }
          ++nSuccs;
        }

        for (auto predIt = llvm::pred_begin(bb), predE = llvm::pred_end(bb);
             predIt != predE; ++predIt)
          ++nPreds;

        std::string bbTypename;
        if (bb->getName().size() > 0) {
          // remove count numbering if existing (after last dot)
          auto bbtypenameend = llvm::StringRef::npos; // Last dot
          if (bb->getName().back() == '.' || bb->getName().back() >= '0' ||
              bb->getName().back() <= '9')
            bbtypenameend = bb->getName().rfind('.');
          bbTypename = bb->getName().substr(0, bbtypenameend).str();
        }

        // Update the info for all the mutants for this popped basic block
        // XXX \ref CONSTRUCT SRC level STMTS
        std::unordered_set<MutantIDType> mutantsofstmt;
        std::unordered_set<const llvm::Instruction *> visitedI;
        for (auto &inst : *bb) {
          if (visitedI.count(&inst) || IR2mutantset.count(&inst) == 0)
            continue;
          mutantsofstmt.clear();
          std::unordered_set<MutantIDType> tmpmuts(IR2mutantset.at(&inst));
          std::unordered_set<MutantIDType> tmpnewmuts;
          while (true) {
            tmpnewmuts.clear();
            for (auto mId : tmpmuts) {
              if (mutantsofstmt.insert(mId).second) {
                tmpnewmuts.insert(mId);
              }
            }
            if (tmpnewmuts.empty())
              break;
            for (auto mId : tmpnewmuts) {
              for (auto *minst : mutant2IRset.at(mId)) {
                assert(
                    llvm::dyn_cast<llvm::Instruction>(minst)->getParent() ==
                        bb &&
                    "mutants spawning multiple BBs not yet implemented. TODO");
                visitedI.insert(llvm::dyn_cast<llvm::Instruction>(minst));
              }
              tmpmuts.insert(getTieDependents(mId).begin(),
                             getTieDependents(mId).end());
            }
          }

          // set the info for each mutant here
          for (auto mutant_id : mutantsofstmt) {
            setMutantTypename(mutant_id, mutInfos.getMutantTypeName(mutant_id));
            setStmtBBTypename(mutant_id, bbTypename);
            setComplexity(mutant_id, mutantsofstmt.size());
            setCFGDepthPredSuccNum(mutant_id, depth, nPreds, nSuccs);
            auto &mutInsts = mutant2IRset.at(mutant_id);
            std::vector<std::string> &dataTypeContext = getOperandDataTypeContextRef(mutant_id);
            for (auto *minst : mutInsts) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
              for (auto UI = minst->use_begin(), UE = minst->use_end(); UI != UE; ++UI) {
                const llvm::User *par = *UI;
#else
              for (auto *par : minst->users()) {
#endif
                // Skip existing in mutInsts
                if (mutInsts.count(par) != 0)
                  continue;

                if (auto *astPar = llvm::dyn_cast<llvm::Instruction>(par)) {
                  addAstParents(mutant_id, astPar);
                  if (dataTypeContext.size() == 0)
                    setReturnDataTypeContext(mutant_id, std::to_string(astPar->getType()->getTypeID()));
                }
              }

              unsigned nLiteral = 0, nIdentifier = 0, nOperator = 0;
              if (auto *minstU = llvm::dyn_cast<llvm::User>(minst)) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                for (auto II = minstU->op_begin(), IE = minstU->op_end(); II != IE; ++II) {
                  auto *child = II->get();
#else
                for (const llvm::Use &childUse : minstU->operands()) {
                  auto *child = childUse.get();
#endif
                  if (mutInsts.count(child) == 0) {
                    nLiteral += (unsigned)llvm::isa<llvm::Constant>(child); 
                    if (auto *isLoad = llvm::dyn_cast<llvm::LoadInst>(child)) {
                      if (llvm::isa<llvm::AllocaInst>(isLoad->getPointerOperand()) || llvm::isa<llvm::GlobalVariable>(isLoad->getPointerOperand()) || llvm::isa<llvm::Argument>(isLoad->getPointerOperand()))
                        ++nIdentifier;
                    }
                    else if (llvm::isa<llvm::AllocaInst>(child) || llvm::isa<llvm::GlobalVariable>(child) || llvm::isa<llvm::Argument>(child))
                      ++nIdentifier;
                    else if (llvm::isa<llvm::Instruction>(child))
                      ++nOperator; 
                    dataTypeContext.push_back(std::to_string(child->getType()->getTypeID()));
                  }
                }
              }
              setChildContext(mutant_id, nLiteral, nIdentifier, nOperator);
            }
          }
        }
      }
    }

    // dump to file for consecutive runs maybe tuning (for experiments)
    if (!mutant_depend_filename.empty())
      dump(mutant_depend_filename);
  } else {
    assert(!mutant_depend_filename.empty() &&
           "no mutant dependency cache specified when dg is disabled");

    // load from file
    load(mutant_depend_filename, mutInfos);
  }

  if (! disable_selection) {
  
    //std::vector<std::vector<std::pair<double, double>>> matrixInOut(mutants_number+1);

    for (MutantIDType mid = 1; mid <= mutants_number; ++mid) {
      //matrixInOut[mid].resize(mutants_number+1, std::pair<double, double>(0.0, 0.0));
      double inproba, outproba;
      inproba = 1.0 / (1 + getTieDependents(mid).size() + getInDataDependents(mid).size());
      outproba = 1.0 / (1 + getTieDependents(mid).size() + getOutDataDependents(mid).size());
      
      for (auto did: getInDataDependents(mid)) {
        //matrixInOut[mid][did].first = inproba;
        getRelationIncomingRef(mid)[did] = inproba;
      }
      
      for (auto did: getOutDataDependents(mid)) {
        //matrixInOut[mid][did].second = outproba;
        getRelationOutgoingRef(mid)[did] = outproba;
      }

      //Tie dependent are added as self loop including This mutant
      // they are added both for In and Out
      //matrixInOut[mid][mid].first = inproba;
      getRelationIncomingRef(mid)[mid] = inproba;
      //matrixInOut[mid][mid].second = outproba;
      getRelationOutgoingRef(mid)[mid] = outproba;
      for (auto did: getTieDependents(mid)) {
        //matrixInOut[mid][did].first = inproba;
        getRelationIncomingRef(mid)[did] = inproba;
        //matrixInOut[mid][did].second = outproba;
        getRelationOutgoingRef(mid)[did] = outproba;
      }
    }

    std::vector<std::unordered_set<MutantIDType> *> vstmtTies(mutants_number + 1, nullptr);

    for (MutantIDType mid = 1; mid <= mutants_number; ++mid) {
      if (vstmtTies[mid] == nullptr) {
        vstmtTies[mid] = new std::unordered_set<MutantIDType>;
        vstmtTies[mid]->insert(mid);
        //If they have exactely the same IRs
        for (auto mtid: getTieDependents(mid)) { 
          if(mutant2IRset[mid].size() == mutant2IRset[mtid].size()){
            bool same = true;
            for (auto *ir: mutant2IRset[mid]) {
              if (mutant2IRset[mtid].count(ir) == 0) {
                same = false;
                break;
              }
            }
            if (same) {
              vstmtTies[mid]->insert(mtid);
              vstmtTies[mtid] = vstmtTies[mid];
            }
          }
        }
      }
    }

    std::unordered_set<std::unordered_set<MutantIDType> *> stmtTies(vstmtTies.begin()+1, vstmtTies.end());
    std::vector<std::pair<std::unordered_map<MutantIDType,double>, std::unordered_map<MutantIDType,double>>> update(mutants_number+1);
    std::vector<std::pair<std::unordered_map<MutantIDType,double>, std::unordered_map<MutantIDType,double>>> prevupdate(mutants_number+1);

    // update and prevupdate take only first elem of each Tieset
    for (MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id) {
      if (mutant_id == *(vstmtTies[mutant_id]->begin())) {
        for (auto &vp: getRelationIncomingConstRef(mutant_id)) {
          if (vp.first == *(vstmtTies[vp.first]->begin())) {
            update[mutant_id].first[vp.first] = vp.second;
            prevupdate[mutant_id].first[vp.first] = vp.second;
          }
        }
        for (auto &vp: getRelationOutgoingConstRef(mutant_id)) {
          if (vp.first == *(vstmtTies[vp.first]->begin())) {
            update[mutant_id].second[vp.first] = vp.second;
            prevupdate[mutant_id].second[vp.first] = vp.second;
          }
        }
      }
    }

    unsigned curhop = 2;
    double cur_relax_factor = RELAX_STEP / curhop;
    while (cur_relax_factor >= RELAX_THRESHOLD) {
      for (auto *stTie: stmtTies) {
        MutantIDType midSrc = *(stTie->begin());

        // In relation
        for (auto &vd: prevupdate[midSrc].first) {
          MutantIDType midDest = vd.first;
          for (auto &vt: prevupdate[midDest].first) {
            MutantIDType midFar = vt.first;
            update[midSrc].first[midFar] +=   prevupdate[midSrc].first[midDest] * prevupdate[midDest].first[midFar];
          }
        }

        // Out Relation
        for (auto &vd: prevupdate[midSrc].second) {
          MutantIDType midDest = vd.first;
          for (auto &vt: prevupdate[midDest].second) {
            MutantIDType midFar = vt.first;
            update[midSrc].second[midFar] +=  prevupdate[midSrc].second[midDest] * prevupdate[midDest].second[midFar];
          }
        }
      }

      // Update the whole cumule
      for (MutantIDType mid = 1; mid <= mutants_number; ++mid) {
        // Readjust the probabilities to 0 -- 1
        // Then update the cumule
        if(mid == *(vstmtTies[mid]->begin())) {
          double sumIn = 0.0;
          for (auto &vin: update[mid].first) { 
            vin.second *= vin.second; //inflate
            sumIn += vin.second * vstmtTies[mid]->size();
          }
          if (sumIn > 0.0)
            for (auto &vin: update[mid].first) 
              vin.second /= sumIn;
          for (auto &inIt: update[mid].first) {
            //absent keys are inserted with value zero in maps
            if (inIt.second > 0.0)
              for (auto mid_i: *(vstmtTies[mid]))
                for (auto mid_j: *(vstmtTies[inIt.first]))
                  //can insert
                  getRelationIncomingRef(mid_i)[mid_j] += inIt.second;
            prevupdate[mid].first[inIt.first] = inIt.second;
            //matrixInOut[mid][inIt.first].first = inIt.second;
          }

          double sumOut = 0.0;
          for (auto &vout: update[mid].second) {
            vout.second *= vout.second;  //inflate
            sumOut += vout.second * vstmtTies[mid]->size();
          }
          if (sumOut > 0.0)
            for (auto &vout: update[mid].second) 
              vout.second /= sumOut;
          for (auto &outIt: update[mid].second) {
            //absent keys are inserted with value zero in maps
            if (outIt.second > 0.0)
              for (auto mid_i: *(vstmtTies[mid]))
                for (auto mid_j: *(vstmtTies[outIt.first]))
                  //can insert
                  getRelationOutgoingRef(mid_i)[mid_j] += outIt.second;
            prevupdate[mid].second[outIt.first] = outIt.second;
            //matrixInOut[mid][outIt.first].second = outIt.second;
          }
        }
      }

      llvm::errs() << "hop " << curhop << "... "; //perf DBG

      // next hop
      ++curhop;
      cur_relax_factor = RELAX_STEP / curhop;
    }
    
    llvm::errs() << "\n";

    // Clear unused
    vstmtTies.clear();
    for (auto *t: stmtTies)
      delete t;
    //matrixInOut.clear();
    update.clear();
    prevupdate.clear();

    // project to 0 -- 1
    for (MutantIDType mid = 1; mid <= mutants_number; ++mid) {
      // Readjust the probabilities to 0 -- 1
      // Then update the cumule
      double sumIn = 0.0;
      for (auto &vin: getRelationIncomingRef(mid)) { 
        //vin.second *= vin.second; //inflate
        sumIn += vin.second;
      }
      if (sumIn > 0.0)
        for (auto &vin: getRelationIncomingRef(mid)) 
          vin.second /= sumIn;

      double sumOut = 0.0;
      for (auto &vout: getRelationOutgoingRef(mid)) {
        //vout.second *= vout.second;  //inflate
        sumOut += vout.second;
      }
      if (sumOut > 0.0)
        for (auto &vout: getRelationOutgoingRef(mid)) 
          vout.second /= sumOut;
    }

    // Verify
    for (MutantIDType m_id = 1; m_id <= mutants_number; ++m_id) {
      for (MutantIDType dm_id = 1; dm_id <= mutants_number; ++dm_id) {
        if (getRelationIncomingRef(m_id).count(dm_id)) { //[dm_id] > 0.0) {
          assert (getRelationOutgoingConstRef(dm_id).count(m_id)); //[m_id] > 0.0);
        }
        if (getRelationOutgoingRef(m_id).count(dm_id)) { //[dm_id] > 0.0) {
          assert (getRelationIncomingConstRef(dm_id).count(m_id)); //[m_id] > 0.0);
        }
      }
    }
    
    //create clusters
    // FIXME: there can't be more cluster than mutant, so better use MutantIDType 
    // instead of unsigned long
    unsigned long initialval = (unsigned long)-1;  
    std::unordered_set<MutantIDType> mutantsVisited;
    std::vector<unsigned long> cluster_ids(mutants_number+1, initialval);
    for (MutantIDType m_id = 1; m_id <= mutants_number; ++m_id) {
      std::unordered_set<MutantIDType> *curDDCluster;
      unsigned long c_id;
      if (cluster_ids[m_id] == initialval) {
        c_id = createNewDDCluster();
        cluster_ids[m_id] = c_id;
        curDDCluster = getDDClusterAt(c_id);
        curDDCluster->insert(m_id);
        mutantsVisited.insert(m_id);
      } else {
        c_id = cluster_ids[m_id];
        curDDCluster = getDDClusterAt(c_id);
        assert (curDDCluster->count(m_id) > 0 && "The mutant must be in the cluster here");
        mutantsVisited.insert(m_id);
    }
      std::stack<MutantIDType> wStack;
      wStack.push(m_id);
      while(!wStack.empty()) {
        auto m_elem = wStack.top();
        wStack.pop();
        for (MutantIDType rel_id = 1; rel_id <= mutants_number; ++rel_id) {
          if (getRelationIncomingConstRef(m_elem).count(rel_id) || getRelationOutgoingConstRef(m_elem).count(rel_id)) {
            if (mutantsVisited.count(rel_id) == 0) {
              curDDCluster->insert(rel_id);
              cluster_ids[rel_id] = c_id;
              mutantsVisited.insert(rel_id);
              wStack.push(rel_id);
            } else {
              assert (curDDCluster->count(rel_id) && cluster_ids[rel_id] == c_id && "absent but not seen here");
            }
          }
        }
      }
    }

  /*
    // get Higher hops data deps (XXX This is not stored in the cache)
    // Skip hop 1
    curhop = 2;
    cur_relax_factor = RELAX_STEP / curhop;

    // compute hop 2 and above
    while (cur_relax_factor >= RELAX_THRESHOLD) {
      for (MutantIDType m_id = 1; m_id <= mutants_number; ++m_id) {
        std::unordered_set<MutantIDType> &hhodDep =
            addHigherHopsOutDataDependents(m_id);
        std::unordered_set<MutantIDType> &hhidDep =
            addHigherHopsInDataDependents(m_id);
        std::unordered_set<MutantIDType> &hhocDep =
            addHigherHopsOutCtrlDependents(m_id);
        std::unordered_set<MutantIDType> &hhicDep =
            addHigherHopsInCtrlDependents(m_id);

        for (auto oddM : getHopsOutDataDependents(m_id, curhop))
          hhodDep.insert(getHopsOutDataDependents(oddM, curhop - 1).begin(),
                         getHopsOutDataDependents(oddM, curhop - 1).end());
        for (auto iddM : getHopsInDataDependents(m_id, curhop))
          hhidDep.insert(getHopsInDataDependents(iddM, curhop - 1).begin(),
                         getHopsInDataDependents(iddM, curhop - 1).end());
        for (auto ocdM : getHopsOutCtrlDependents(m_id, curhop))
          hhocDep.insert(getHopsOutCtrlDependents(ocdM, curhop - 1).begin(),
                         getHopsOutCtrlDependents(ocdM, curhop - 1).end());
        for (auto icdM : getHopsInCtrlDependents(m_id, curhop))
          hhicDep.insert(getHopsInCtrlDependents(icdM, curhop - 1).begin(),
                         getHopsInCtrlDependents(icdM, curhop - 1).end());
      }

      // next hop
      ++curhop;
      cur_relax_factor = RELAX_STEP / curhop;
    }
  */
  
  }  //if (! disable_selection)
  
  llvm::errs() << "\t... graph Builded/Loaded\n";
}

void MutantDependenceGraph::computeMutantFeatures(
    std::vector<std::vector<float>> &features_matrix,
    std::vector<std::string> &features_names) {
  auto nummuts = getMutantsNumber();

  // Features requiring One Hot Encoding (more like python pandas' get_dummies)
  std::unordered_set<std::string> allMutantTypenames; // all mutantTypeNames
  std::unordered_set<std::string> allStmtBBTypenames; // all stmtBBTypeNames
  std::unordered_set<std::string> allASTParentOpcodenames;
  std::unordered_set<std::string> allDataTypes;
  std::vector<std::string> mutTypenameTmp;
  std::vector<std::string> stmtTypenameTmp;
  const std::string nsuff_astparent("-astparent");
  const std::string nsuff_outdatadep("-outdatadep");
  const std::string nsuff_indatadep("-indatadep");
  const std::string nsuff_outctrldep("-outctrldep");
  const std::string nsuff_inctrldep("-inctrldep");
  const std::string astParentTypeSig("-ASTp");
  const std::string childContextSig("-ChildContext");
  const std::string dataTypeContextOperandSig("-Operand-DataTypeContext");
  const std::string dataTypeContextReturnSig("-Return-DataTypeContext");
  
  for (auto mutant_id = 1; mutant_id <= nummuts; ++mutant_id) {
    mutTypenameTmp.clear();
    getSplittedMutantTypename(mutant_id, mutTypenameTmp);
    allMutantTypenames.insert(mutTypenameTmp.begin(), mutTypenameTmp.end());
    stmtTypenameTmp.clear();
    getSplittedStmtBBTypename(mutant_id, stmtTypenameTmp);
    allStmtBBTypenames.insert(stmtTypenameTmp.begin(), stmtTypenameTmp.end());
    for (auto const &str: getAstParentsOpcodeNames(mutant_id))
      allASTParentOpcodenames.insert(str+astParentTypeSig);
    if (!getReturnDataTypeContext(mutant_id).empty())
      allDataTypes.insert(getReturnDataTypeContext(mutant_id));
    for (auto const &str: getOperandDataTypeContext(mutant_id))
      allDataTypes.insert(str);
  }

  std::unordered_map<std::string, unsigned long long> mutantFeatures;
  // insert features names
  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("Complexity");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("CfgDepth");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("CfgPredNum");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("CfgSuccNum");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("AstNumParents");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("HasLiteralChild"+childContextSig);
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("HasIdentifierChild"+childContextSig);
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("HasOperatorChild"+childContextSig);
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("NumOutDataDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("NumInDataDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("NumOutCtrlDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("NumInCtrlDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("NumTieDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("AstParentsNumOutDataDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("AstParentsNumInDataDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("AstParentsNumOutCtrlDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("AstParentsNumInCtrlDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(nummuts);
  features_names.emplace_back("AstParentsNumTieDeps");
  mutantFeatures[features_names.back()] = features_matrix.size() - 1;

  // mutant typename as one hot form
  for (auto &mtname : allMutantTypenames) {
    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(mtname);
    mutantFeatures[features_names.back()] =
        features_matrix.size() - 1; // mutant's XXX

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(mtname + nsuff_astparent);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(mtname + nsuff_outdatadep);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(mtname + nsuff_indatadep);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(mtname + nsuff_outctrldep);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(mtname + nsuff_inctrldep);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;
  }

  // stmt BB typename as one hot form
  for (auto &sbbtname : allStmtBBTypenames) {
    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(sbbtname);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(sbbtname + nsuff_astparent);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(sbbtname + nsuff_outdatadep);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(sbbtname + nsuff_indatadep);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(sbbtname + nsuff_outctrldep);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(sbbtname + nsuff_inctrldep);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;
  }

  // AST parent type as one hot
  for (auto &pasttype : allASTParentOpcodenames) {
    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(pasttype);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;
  }

  // Data type as one hot
  for (auto &dattype : allDataTypes) {
    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(dattype+dataTypeContextOperandSig);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(nummuts);
    features_names.emplace_back(dattype+dataTypeContextReturnSig);
    mutantFeatures[features_names.back()] = features_matrix.size() - 1;
  }

  /// Create feature values for all mutants
  std::unordered_map<std::string, float> featureValuesPost1Hot;
  for (MutantIDType mutant_id = 1; mutant_id <= nummuts; ++mutant_id) {
    featureValuesPost1Hot.clear();
    featureValuesPost1Hot["Complexity"] = (getComplexity(mutant_id));
    featureValuesPost1Hot["CfgDepth"] = (getCfgDepth(mutant_id));
    featureValuesPost1Hot["CfgPredNum"] = (getCfgPredNum(mutant_id));
    featureValuesPost1Hot["CfgSuccNum"] = (getCfgSuccNum(mutant_id));
    featureValuesPost1Hot["AstNumParents"] =
        (getAstParentsOpcodeNames(mutant_id).size());
    featureValuesPost1Hot["HasLiteralChild"+childContextSig] =
        getHasLiteralChild(mutant_id) > 0;
    featureValuesPost1Hot["HasIdentifierChild"+childContextSig] =
        getHasIdentifierChild(mutant_id) > 0;
    featureValuesPost1Hot["HasOperatorChild"+childContextSig] =
        getHasOperatorChild(mutant_id) > 0;
    featureValuesPost1Hot["NumOutDataDeps"] =
        (getOutDataDependents(mutant_id).size());
    featureValuesPost1Hot["NumInDataDeps"] =
        (getInDataDependents(mutant_id).size());
    featureValuesPost1Hot["NumOutCtrlDeps"] =
        (getOutCtrlDependents(mutant_id).size());
    featureValuesPost1Hot["NumInCtrlDeps"] =
        (getInCtrlDependents(mutant_id).size());
    featureValuesPost1Hot["NumTieDeps"] = (getTieDependents(mutant_id).size());

    std::unordered_set<MutantIDType> poutDataDeps;
    std::unordered_set<MutantIDType> pinDataDeps;
    std::unordered_set<MutantIDType> poutCtrlDeps;
    std::unordered_set<MutantIDType> pinCtrlDeps;
    std::unordered_set<MutantIDType> pTieDeps;
    for (auto parent_id : getAstParentsMutants(mutant_id)) {
      poutDataDeps.insert(getOutDataDependents(parent_id).begin(),
                          getOutDataDependents(parent_id).end());
      pinDataDeps.insert(getInDataDependents(parent_id).begin(),
                         getInDataDependents(parent_id).end());
      poutCtrlDeps.insert(getOutCtrlDependents(parent_id).begin(),
                          getOutCtrlDependents(parent_id).end());
      pinCtrlDeps.insert(getInCtrlDependents(parent_id).begin(),
                         getInCtrlDependents(parent_id).end());
      pTieDeps.insert(getTieDependents(parent_id).begin(),
                      getTieDependents(parent_id).end());
    }

    featureValuesPost1Hot["AstParentsNumOutDataDeps"] = poutDataDeps.size();
    featureValuesPost1Hot["AstParentsNumInDataDeps"] = pinDataDeps.size();
    featureValuesPost1Hot["AstParentsNumOutCtrlDeps"] = poutCtrlDeps.size();
    featureValuesPost1Hot["AstParentsNumInCtrlDeps"] = pinCtrlDeps.size();
    featureValuesPost1Hot["AstParentsNumTieDeps"] = pTieDeps.size();

    /// One Hot Features: first initialize all to 0 then increment as they are
    /// found
    for (auto &mtname : allMutantTypenames) {
      featureValuesPost1Hot[mtname] = 0;
      featureValuesPost1Hot[mtname + nsuff_astparent] = 0;
      featureValuesPost1Hot[mtname + nsuff_outdatadep] = 0;
      featureValuesPost1Hot[mtname + nsuff_indatadep] = 0;
      featureValuesPost1Hot[mtname + nsuff_outctrldep] = 0;
      featureValuesPost1Hot[mtname + nsuff_inctrldep] = 0;
    }
    for (auto &sbbtname : allStmtBBTypenames) {
      featureValuesPost1Hot[sbbtname] = 0;
      featureValuesPost1Hot[sbbtname + nsuff_astparent] = 0;
      featureValuesPost1Hot[sbbtname + nsuff_outdatadep] = 0;
      featureValuesPost1Hot[sbbtname + nsuff_indatadep] = 0;
      featureValuesPost1Hot[sbbtname + nsuff_outctrldep] = 0;
      featureValuesPost1Hot[sbbtname + nsuff_inctrldep] = 0;
    }
    for (auto &pasttype : allASTParentOpcodenames) {
      featureValuesPost1Hot[pasttype] = 0;
    }

    for (auto &dattype : allDataTypes) {
      featureValuesPost1Hot[dattype+dataTypeContextOperandSig] = 0;
      featureValuesPost1Hot[dattype+dataTypeContextReturnSig] = 0;
    }

    mutTypenameTmp.clear();
    getSplittedMutantTypename(mutant_id, mutTypenameTmp);
    for (auto &str: mutTypenameTmp)
      featureValuesPost1Hot.at(str) = 1;
    stmtTypenameTmp.clear();
    getSplittedStmtBBTypename(mutant_id, stmtTypenameTmp);
    for (auto &str: stmtTypenameTmp)
      featureValuesPost1Hot.at(str) = 1;
    for (auto &str: getAstParentsOpcodeNames(mutant_id))
      ++(featureValuesPost1Hot.at(str+astParentTypeSig));

    if (!getReturnDataTypeContext(mutant_id).empty()) 
      ++(featureValuesPost1Hot.at(getReturnDataTypeContext(mutant_id)+dataTypeContextReturnSig));
    for (auto strt: getOperandDataTypeContext(mutant_id))
      ++(featureValuesPost1Hot.at(strt+dataTypeContextOperandSig));

    for (auto pid : getAstParentsMutants(mutant_id)) {
      mutTypenameTmp.clear();
      getSplittedMutantTypename(pid, mutTypenameTmp);
      for (auto &str: mutTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_astparent) += 1;
      stmtTypenameTmp.clear();
      getSplittedStmtBBTypename(pid, stmtTypenameTmp);
      for (auto &str: stmtTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_astparent) += 1;
    }
    for (auto odid : getOutDataDependents(mutant_id)) {
      mutTypenameTmp.clear();
      getSplittedMutantTypename(odid, mutTypenameTmp);
      for (auto &str: mutTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_outdatadep) += 1;
      stmtTypenameTmp.clear();
      getSplittedStmtBBTypename(odid, stmtTypenameTmp);
      for (auto &str: stmtTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_outdatadep) += 1;
    }
    for (auto idid : getInDataDependents(mutant_id)) {
      mutTypenameTmp.clear();
      getSplittedMutantTypename(idid, mutTypenameTmp);
      for (auto &str: mutTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_indatadep) += 1;
      stmtTypenameTmp.clear();
      getSplittedStmtBBTypename(idid, stmtTypenameTmp);
      for (auto &str: stmtTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_indatadep) += 1;
    }
    for (auto ocid : getOutCtrlDependents(mutant_id)) {
      mutTypenameTmp.clear();
      getSplittedMutantTypename(ocid, mutTypenameTmp);
      for (auto &str: mutTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_outctrldep) += 1;
      stmtTypenameTmp.clear();
      getSplittedStmtBBTypename(ocid, stmtTypenameTmp);
      for (auto &str: stmtTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_outctrldep) += 1;
    }
    for (auto icid : getInCtrlDependents(mutant_id)) {
      mutTypenameTmp.clear();
      getSplittedMutantTypename(icid, mutTypenameTmp);
      for (auto &str: mutTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_inctrldep) += 1;
      stmtTypenameTmp.clear();
      getSplittedStmtBBTypename(icid, stmtTypenameTmp);
      for (auto &str: stmtTypenameTmp) 
        featureValuesPost1Hot.at(str + nsuff_inctrldep) += 1;
    }

    /// Append this mutant feature values to the mutant features table
    // llvm::errs() << mutantFeatures.size()<< " " <<
    // featureValuesPost1Hot.size()<< "\n";
    assert(mutantFeatures.size() == featureValuesPost1Hot.size() &&
           "must have same size");
    for (auto &mfIt : featureValuesPost1Hot) {
      features_matrix.at(mutantFeatures.at(mfIt.first)).push_back(mfIt.second);
    }
  }

  /// Normalize each feature value between 0 and 1 to have a normalization
  /// accross programs
  for (auto &feature_val : mutantFeatures) {
    auto mM =
        std::minmax_element(features_matrix.at(feature_val.second).begin(),
                            features_matrix.at(feature_val.second).end());
    auto min = *(mM.first);
    auto max = *(mM.second);
    auto max_min = max - min;

    // normalize
    if (max > min)
      for (auto It = features_matrix.at(feature_val.second).begin(),
                Ie = features_matrix.at(feature_val.second).end();
           It != Ie; ++It)
        *It = (*It - min) / max_min;
  }
  assert(mutantFeatures.size() == features_matrix.size() &&
         mutantFeatures.size() == features_names.size() &&
         "@MutantSelection: Features size mismatch, Please report Bug!");
}

void MutantDependenceGraph::computeStatementFeatures(
    std::vector<std::vector<float>> &features_matrix,
    std::vector<std::string> &features_names, 
    std::vector<std::unordered_set<MutantIDType>> &stmt2muts) {

  typedef size_t StmtIDType;
  std::unordered_map<MutantIDType, std::unordered_set<StmtIDType>> mutant2stmtid;

  // XXX \ref CONSTRUCT SRC level STMTS
  std::unordered_set<const llvm::Value *> visitedI;
  for (auto &kv_pair : IR2mutantset) {
    auto const *inst_val = kv_pair.first;
    auto *bb = llvm::dyn_cast<llvm::Instruction>(inst_val)->getParent();
    if (visitedI.count(inst_val) || IR2mutantset.count(inst_val) == 0)
      continue;
    stmt2muts.emplace_back();
    std::unordered_set<MutantIDType> tmpmuts(IR2mutantset.at(inst_val));
    std::unordered_set<MutantIDType> tmpnewmuts;
    while (true) {
      tmpnewmuts.clear();
      for (auto mId : tmpmuts) {
        if (stmt2muts.back().insert(mId).second) {
          tmpnewmuts.insert(mId);
        }
      }
      if (tmpnewmuts.empty())
        break;
      for (auto mId : tmpnewmuts) {
        for (auto *minst : mutant2IRset.at(mId)) {
          assert(
              llvm::dyn_cast<llvm::Instruction>(minst)->getParent() ==
                  bb &&
              "mutants spawning multiple BBs not yet implemented. TODO");
          visitedI.insert(llvm::dyn_cast<llvm::Instruction>(minst));
        }
        tmpmuts.insert(getTieDependents(mId).begin(),
                       getTieDependents(mId).end());
      }
    }
    for (auto mid: stmt2muts.back()) {
      assert (mutant2stmtid.count(mid) == 0 && "Mutant spawning 2 stmts, impossible(BUG)");
      mutant2stmtid[mid] = std::unordered_set<StmtIDType> {stmt2muts.size()-1};
    }
  }

  auto numstmts = stmt2muts.size();
  std::vector<std::unordered_set<std::string>> stmt2BBTypenames(numstmts); 

  // Features requiring One Hot Encoding (more like python pandas' get_dummies)
  std::unordered_set<std::string> allStmtBBTypenames; // all stmtBBTypeNames
  std::vector<std::string> stmtTypenameTmp;
  const std::string nsuff_outdatadep("-outdatadep");
  const std::string nsuff_indatadep("-indatadep");
  const std::string nsuff_outctrldep("-outctrldep");
  const std::string nsuff_inctrldep("-inctrldep");
  
  for (auto stmt_id = 0; stmt_id < numstmts; ++stmt_id) {
    if (stmt2muts[stmt_id].empty()) 
      continue;
    for (auto a_mut_id: stmt2muts[stmt_id]) {
      stmtTypenameTmp.clear();
      getSplittedStmtBBTypename(a_mut_id, stmtTypenameTmp);
      allStmtBBTypenames.insert(stmtTypenameTmp.begin(), stmtTypenameTmp.end());
      stmt2BBTypenames[stmt_id].insert(stmtTypenameTmp.begin(), stmtTypenameTmp.end());
    }
  }

  std::unordered_map<std::string, unsigned long long> stmtFeatures;
  // insert features names
  features_matrix.emplace_back();
  features_matrix.back().reserve(numstmts);
  features_names.emplace_back("Complexity");
  stmtFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(numstmts);
  features_names.emplace_back("CfgDepth");
  stmtFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(numstmts);
  features_names.emplace_back("CfgPredNum");
  stmtFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(numstmts);
  features_names.emplace_back("CfgSuccNum");
  stmtFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(numstmts);
  features_names.emplace_back("NumOutDataDeps");
  stmtFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(numstmts);
  features_names.emplace_back("NumInDataDeps");
  stmtFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(numstmts);
  features_names.emplace_back("NumOutCtrlDeps");
  stmtFeatures[features_names.back()] = features_matrix.size() - 1;

  features_matrix.emplace_back();
  features_matrix.back().reserve(numstmts);
  features_names.emplace_back("NumInCtrlDeps");
  stmtFeatures[features_names.back()] = features_matrix.size() - 1;



  // stmt BB typename as one hot form
  for (auto &sbbtname : allStmtBBTypenames) {
    features_matrix.emplace_back();
    features_matrix.back().reserve(numstmts);
    features_names.emplace_back(sbbtname);
    stmtFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(numstmts);
    features_names.emplace_back(sbbtname + nsuff_outdatadep);
    stmtFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(numstmts);
    features_names.emplace_back(sbbtname + nsuff_indatadep);
    stmtFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(numstmts);
    features_names.emplace_back(sbbtname + nsuff_outctrldep);
    stmtFeatures[features_names.back()] = features_matrix.size() - 1;

    features_matrix.emplace_back();
    features_matrix.back().reserve(numstmts);
    features_names.emplace_back(sbbtname + nsuff_inctrldep);
    stmtFeatures[features_names.back()] = features_matrix.size() - 1;
  }

  /// Create feature values for all mutants
  std::unordered_set<llvm::Value const *> irset_tmp;
  std::unordered_set<StmtIDType> stmtset_tmp;
  std::unordered_map<std::string, float> featureValuesPost1Hot;
  for (auto stmt_id = 0; stmt_id < numstmts; ++stmt_id) {
    if (stmt2muts[stmt_id].empty()) 
      continue;
    auto a_mut_id = *(stmt2muts[stmt_id].begin());
  
    featureValuesPost1Hot.clear();
    irset_tmp.clear();
    getTSetOfMutSet<llvm::Value const *>(stmt2muts[stmt_id], mutant2IRset, irset_tmp);
    featureValuesPost1Hot["Complexity"] = (irset_tmp.size());
    featureValuesPost1Hot["CfgDepth"] = (getCfgDepth(a_mut_id));
    featureValuesPost1Hot["CfgPredNum"] = (getCfgPredNum(a_mut_id));
    featureValuesPost1Hot["CfgSuccNum"] = (getCfgSuccNum(a_mut_id));
    stmtset_tmp.clear();
    getTSetOfMutSet<StmtIDType>(getOutDataDependents(a_mut_id), mutant2stmtid, stmtset_tmp);
    stmtset_tmp.erase(stmt_id);
    featureValuesPost1Hot["NumOutDataDeps"] =
        (stmtset_tmp.size()); 
    stmtset_tmp.clear();
    getTSetOfMutSet<StmtIDType>(getInDataDependents(a_mut_id), mutant2stmtid, stmtset_tmp);
    stmtset_tmp.erase(stmt_id);
    featureValuesPost1Hot["NumInDataDeps"] =
        (stmtset_tmp.size());
    stmtset_tmp.clear();
    getTSetOfMutSet<StmtIDType>(getOutCtrlDependents(a_mut_id), mutant2stmtid, stmtset_tmp);
    stmtset_tmp.erase(stmt_id);
    featureValuesPost1Hot["NumOutCtrlDeps"] =
        (stmtset_tmp.size());
    stmtset_tmp.clear();
    getTSetOfMutSet<StmtIDType>(getInCtrlDependents(a_mut_id), mutant2stmtid, stmtset_tmp);
    stmtset_tmp.erase(stmt_id);
    featureValuesPost1Hot["NumInCtrlDeps"] =
        (stmtset_tmp.size());

    for (auto &sbbtname : allStmtBBTypenames) {
      featureValuesPost1Hot[sbbtname] = 0;
      featureValuesPost1Hot[sbbtname + nsuff_outdatadep] = 0;
      featureValuesPost1Hot[sbbtname + nsuff_indatadep] = 0;
      featureValuesPost1Hot[sbbtname + nsuff_outctrldep] = 0;
      featureValuesPost1Hot[sbbtname + nsuff_inctrldep] = 0;
    }

    for (auto &str: stmt2BBTypenames[stmt_id])
      featureValuesPost1Hot.at(str) = 1;

    stmtset_tmp.clear();
    getTSetOfMutSet<StmtIDType>(getOutDataDependents(a_mut_id), mutant2stmtid, stmtset_tmp);
    stmtset_tmp.erase(stmt_id);
    for (auto odid : stmtset_tmp) {
      for (auto &str: stmt2BBTypenames[odid]) 
        featureValuesPost1Hot.at(str + nsuff_outdatadep) += 1;
    }
    stmtset_tmp.clear();
    getTSetOfMutSet<StmtIDType>(getInDataDependents(a_mut_id), mutant2stmtid, stmtset_tmp);
    stmtset_tmp.erase(stmt_id);
    for (auto idid : stmtset_tmp) { 
      for (auto &str: stmt2BBTypenames[idid]) 
        featureValuesPost1Hot.at(str + nsuff_indatadep) += 1;
    }
    stmtset_tmp.clear();
    getTSetOfMutSet<StmtIDType>(getOutCtrlDependents(a_mut_id), mutant2stmtid, stmtset_tmp);
    stmtset_tmp.erase(stmt_id);
    for (auto ocid : stmtset_tmp) { 
      for (auto &str: stmt2BBTypenames[ocid]) 
        featureValuesPost1Hot.at(str + nsuff_outctrldep) += 1;
    }
    stmtset_tmp.clear();
    getTSetOfMutSet<StmtIDType>(getInCtrlDependents(a_mut_id), mutant2stmtid, stmtset_tmp);
    stmtset_tmp.erase(stmt_id);
    for (auto icid : stmtset_tmp) { 
      for (auto &str: stmt2BBTypenames[icid]) 
        featureValuesPost1Hot.at(str + nsuff_inctrldep) += 1;
    }

    /// Append this mutant feature values to the mutant features table
    // llvm::errs() << mutantFeatures.size()<< " " <<
    // featureValuesPost1Hot.size()<< "\n";
    assert(stmtFeatures.size() == featureValuesPost1Hot.size() &&
           "must have same size");
    for (auto &mfIt : featureValuesPost1Hot) {
      features_matrix.at(stmtFeatures.at(mfIt.first)).push_back(mfIt.second);
    }
  }


  /// Normalize each feature value between 0 and 1 to have a normalization
  /// accross programs
  for (auto &feature_val : stmtFeatures) {
    auto mM =
        std::minmax_element(features_matrix.at(feature_val.second).begin(),
                            features_matrix.at(feature_val.second).end());
    auto min = *(mM.first);
    auto max = *(mM.second);
    auto max_min = max - min;

    // normalize
    if (max > min)
      for (auto It = features_matrix.at(feature_val.second).begin(),
                Ie = features_matrix.at(feature_val.second).end();
           It != Ie; ++It)
        *It = (*It - min) / max_min;
  }
  assert(stmtFeatures.size() == features_matrix.size() &&
         stmtFeatures.size() == features_names.size() &&
         "@MutantSelection: (Stmt) Features size mismatch, Please report Bug!");
}

///
void MutantDependenceGraph::dump(std::string filename) {
  JsonBox::Array outListJSON;
  auto nummuts = getMutantsNumber();
  for (auto mutant_id = 1; mutant_id <= nummuts; ++mutant_id) {
    JsonBox::Object tmpobj;
    JsonBox::Array tmparr;
    for (auto odid : getOutDataDependents(mutant_id))
      tmparr.push_back(JsonBox::Value(std::to_string(odid)));
    tmpobj["outDataDependents"] = tmparr;
    /*tmparr.clear();
    for (auto idid: getInDataDependents(mutant_id))
        tmparr.push_back(JsonBox::Value(std::to_string(idid)));
    tmpobj["inDataDependents"] = tmparr;*/
    tmparr.clear();
    for (auto ocid : getOutCtrlDependents(mutant_id))
      tmparr.push_back(JsonBox::Value(std::to_string(ocid)));
    tmpobj["outCtrlDependents"] = tmparr;
    /*tmparr.clear();
    for (auto icid: getInCtrlDependents(mutant_id))
        tmparr.push_back(JsonBox::Value(std::to_string(icid)));
    tmpobj["inCtrlDependents"] = tmparr;*/
    tmparr.clear();
    for (auto tid : getTieDependents(mutant_id))
      tmparr.push_back(JsonBox::Value(std::to_string(tid)));
    tmpobj["tieDependents"] = tmparr;

    // Others
    tmparr.clear();
    for (auto pmid : getAstParentsMutants(mutant_id))
      tmparr.push_back(JsonBox::Value(std::to_string(pmid)));
    tmpobj["astParentsMutants"] = tmparr;

    tmparr.clear();
    for (auto pocn : getAstParentsOpcodeNames(mutant_id))
      tmparr.push_back(JsonBox::Value(pocn));
    tmpobj["astParentsOpcodeNames"] = tmparr;

    tmpobj["mutantTypename"] = JsonBox::Value(getMutantTypename(mutant_id));
    tmpobj["stmtBBTypename"] = JsonBox::Value(getStmtBBTypename(mutant_id));
    tmpobj["cfgDepth"] = JsonBox::Value(std::to_string(getCfgDepth(mutant_id)));
    tmpobj["cfgPredNum"] =
        JsonBox::Value(std::to_string(getCfgPredNum(mutant_id)));
    tmpobj["cfgSuccNum"] =
        JsonBox::Value(std::to_string(getCfgSuccNum(mutant_id)));
    tmpobj["complexity"] =
        JsonBox::Value(std::to_string(getComplexity(mutant_id)));

    tmpobj["hasLiteralChild"] = JsonBox::Value(std::to_string(getHasLiteralChild(mutant_id)));
    tmpobj["hasIdentifierChild"] = JsonBox::Value(std::to_string(getHasIdentifierChild(mutant_id)));
    tmpobj["hasOperatorChild"] = JsonBox::Value(std::to_string(getHasOperatorChild(mutant_id)));

    tmpobj["dataTypeContextReturn"] = JsonBox::Value(getReturnDataTypeContext(mutant_id));
    tmparr.clear();
    for (auto str : getOperandDataTypeContext(mutant_id))
      tmparr.push_back(JsonBox::Value(str));
    tmpobj["dataTypeContextOperand"] = tmparr;

    outListJSON.push_back(JsonBox::Value(tmpobj));
  }
  JsonBox::Value vout(outListJSON);
  vout.writeToFile(filename, false, false);
}

void MutantDependenceGraph::load(std::string filename,
                                 MutantInfoList const &mutInfos) {
  JsonBox::Value value_in;
  value_in.loadFromFile(filename);
  assert(value_in.isArray() &&
         "The JSON file data of mutants dependence must be a JSON Array");
  JsonBox::Array array_in = value_in.getArray();
  auto nummuts = mutInfos.getMutantsNumber();
  assert(nummuts == array_in.size() && "the number of mutants mismach, did you "
                                       "choose the right mutants dependence or "
                                       "info file?");

  // initialize mutantid_str2int
  std::unordered_map<std::string, MutantIDType> mutantid_str2int;
  for (auto mutant_id = 1; mutant_id <= nummuts; ++mutant_id)
    mutantid_str2int[std::to_string(mutant_id)] = mutant_id;

  // Load
  for (auto mutant_id = 1; mutant_id <= nummuts; ++mutant_id) {
    // Skip type checking, assume that the file hasn't been modified
    JsonBox::Value tmpv = array_in[mutant_id - 1];
    assert(tmpv.isObject() && "each mutant dependence data is a JSON object");
    JsonBox::Object tmpobj = tmpv.getObject();
    assert(tmpobj.count("outDataDependents") > 0 &&
           "'outDataDependents' missing from mutand dependence data");
    // assert (tmpobj.count("inDataDependents")>0 && "'inDataDependents' missing
    // from mutand dependence data");
    assert(tmpobj.count("outCtrlDependents") > 0 &&
           "'outCtrlDependents' missing from mutand dependence data");
    // assert (tmpobj.count("inCtrlDependents")>0 && "'inCtrlDependents' missing
    // from mutand dependence data");
    assert(tmpobj.count("tieDependents") > 0 &&
           "'tieDependents' missing from mutand dependence data");
    JsonBox::Array tmparr;
    assert(tmpobj["outDataDependents"].isArray() &&
           "'outDataDependents' must be an array");
    tmparr = tmpobj["outDataDependents"].getArray();
    for (JsonBox::Value odid : tmparr) {
      assert(odid.isString() && "the mutant in each dependence set should be "
                                "represented as an intger string "
                                "(outDataDepends)");
      std::string tmpstr = odid.getString();
      // out of bound if the value is not a mutant id as unsigned (MutantIDType)
      addDataDependency(mutant_id, mutantid_str2int.at(tmpstr));
    }
    assert(tmpobj["outCtrlDependents"].isArray() &&
           "'outCtrlDependents' must be an Array");
    tmparr = tmpobj["outCtrlDependents"].getArray();
    for (JsonBox::Value ocid : tmparr) {
      assert(ocid.isString() && "the mutant in each dependence set should be "
                                "represented as an intger string "
                                "(outCtrlDepends)");
      std::string tmpstr = ocid.getString();
      // out of bound if the value is not a mutant id as unsigned (MutantIDType)
      addCtrlDependency(mutant_id, mutantid_str2int.at(tmpstr));
    }
    assert(tmpobj["tieDependents"].isArray() &&
           "'tieDependents' must be an Array");
    tmparr = tmpobj["tieDependents"].getArray();
    for (JsonBox::Value tid : tmparr) {
      assert(tid.isString() && "the mutant in each dependence set should be "
                               "represented as an intger string "
                               "(tieDependents)");
      std::string tmpstr = tid.getString();
      // out of bound if the value is not a mutant id as unsigned (MutantIDType)
      addTieDependency(mutant_id, mutantid_str2int.at(tmpstr));
    }

    // Others
    assert(tmpobj["astParentsMutants"].isArray() &&
           "'astParentsMutants' must be an Array");
    tmparr = tmpobj["astParentsMutants"].getArray();
    for (JsonBox::Value pm : tmparr) {
      assert(pm.isString() && "the mutants should be "
                              "represented as an intger string "
                              "(astParentsMutants)");
      std::string tmpstr = pm.getString();
      // out of bound if the value is not a mutant id as unsigned (MutantIDType)
      addAstParentsMutants(mutant_id, mutantid_str2int.at(tmpstr));
    }

    assert(tmpobj["astParentsOpcodeNames"].isArray() &&
           "'astParentsopcodeNames' must be an Array");
    tmparr = tmpobj["astParentsOpcodeNames"].getArray();
    for (JsonBox::Value pocn : tmparr) {
      assert(pocn.isString() && "the mutants should be "
                                "represented as an intger string "
                                "(astParentsMutants)");
      std::string tmpstr = pocn.getString();
      // out of bound if the value is not a mutant id as unsigned (MutantIDType)
      addAstParentsOpcodeNames(mutant_id, tmpstr);
    }

    assert(tmpobj["mutantTypename"].isString() &&
           "mutantTypename must be string");
    setMutantTypename(mutant_id, tmpobj["mutantTypename"].getString());
    assert(tmpobj["stmtBBTypename"].isString() &&
           "stmtBBTypename must be string");
    setStmtBBTypename(mutant_id, tmpobj["stmtBBTypename"].getString());
    // assert (tmpobj["cfgDepth"].isString() && "cfgDepth must be int");
    // assert (tmpobj["cfgPrednum"].isString() && "cfgPredNum must be int");
    // assert (tmpobj["cfgSuccNum"].isString() && "cfgSuccNum must be int");
    setCFGDepthPredSuccNum(
        mutant_id, std::stoul(tmpobj["cfgDepth"].getToString(), nullptr, 0),
        std::stoul(tmpobj["cfgPredNum"].getToString(), nullptr, 0),
        std::stoul(tmpobj["cfgSuccNum"].getToString(), nullptr, 0));
    // assert (tmpobj["complexity"].isString() && "complexity must be int");
    setComplexity(mutant_id,
                  std::stoul(tmpobj["complexity"].getToString(), nullptr, 0));

    setChildContext(
        mutant_id, std::stoul(tmpobj["hasLiteralChild"].getToString(), nullptr, 0),
        std::stoul(tmpobj["hasIdentifierChild"].getToString(), nullptr, 0),
        std::stoul(tmpobj["hasOperatorChild"].getToString(), nullptr, 0));

    setReturnDataTypeContext(mutant_id, tmpobj["dataTypeContextReturn"].getToString());
    assert(tmpobj["dataTypeContextOperand"].isArray() &&
           "'dataTypeContextOperand' must be an Array");
    tmparr = tmpobj["dataTypeContextOperand"].getArray();
    std::vector<std::string> &xxx = getOperandDataTypeContextRef(mutant_id);
    for (JsonBox::Value str : tmparr) {
      assert(str.isString() && "the info of datatype context should be "
                               "represented as strings");
      xxx.push_back(str.getString());
    }
  }
}

void MutantDependenceGraph::exportMutantFeaturesCSV(std::string filenameCSV, 
                                                    MutantInfoList const &mutInfos,
                                                    bool isDefectPrediction) {
  // each embedded vector represent a feature
  std::vector<std::vector<float>> features_matrix;
  std::vector<std::string> features_names;

  MutantIDType numObjs;
  if (isDefectPrediction) {
    std::vector<std::unordered_set<MutantIDType>> stmt2muts;
    computeStatementFeatures(features_matrix, features_names, stmt2muts);
    numObjs = stmt2muts.size();
    // write mapping of stmt into srcloc
    std::ofstream locout(filenameCSV+".srcloc.list");
    if (locout.is_open()) {
      locout << "Statement-LOC\n";
      for (auto &mset: stmt2muts) {
        // get locs
        std::unordered_set<std::string> locset;
        for (auto mid: mset)
          locset.insert(mutInfos.getMutantSourceLoc(mid));
        //write locs
        for (auto loc: locset)
          locout << loc << ",";
        locout << "\n";
      }
    } else {
      llvm::errs() << "Err: Unable to create Stmt loc list file:" << filenameCSV+".srcloc.list"
                   << "\n\n";
      assert(false);
    }
  } else {
    computeMutantFeatures(features_matrix, features_names);
    numObjs = getMutantsNumber();
  }

  /// Dump into CSV file
  std::ofstream csvout(filenameCSV);
  if (csvout.is_open()) {
    // features list
    unsigned isFirst = 1;
    char *comma[2] = {",", ""};
    for (auto &feature : features_names) {
      assert(feature.find(',') == std::string::npos &&
             "feature name must have no comma (,)");
      csvout << comma[isFirst] << feature;
      isFirst = 0;
    }
    csvout << "\n";

    // mutants features
    for (auto obj_id = 0; obj_id < numObjs; ++obj_id) {
      isFirst = 1;
      for (auto &feature_val : features_matrix) {
        csvout << comma[isFirst] << feature_val[obj_id];
        isFirst = 0;
      }
      csvout << "\n";
    }
    csvout.close();
  } else {
    llvm::errs() << "Unable to create mutant (or STmt) features CSV file:" << filenameCSV
                 << "\n\n";
    assert(false);
  }
}
/////

// class MutantSelection

void MutantSelection::buildDependenceGraphs(std::string mutant_depend_filename,
                                            bool rerundg, bool isFlowSensitive,
                                            bool isClassicCtrlDepAlgo,
                                            bool disable_selection) {
  if (rerundg) {
    dg::CD_ALG cd_alg;
    if (isClassicCtrlDepAlgo)
      //cd_alg = dg::CLASSIC;
      cd_alg = dg::CD_ALG::CLASSIC;
    else
      cd_alg = dg::CD_ALG::CONTROL_EXPRESSION;

    // build IR DGraph
    /*dg::LLVMDependenceGraph IRDGraph;
    dg::LLVMPointerAnalysis *PTA = new dg::LLVMPointerAnalysis(&subjectModule);
    if (isFlowSensitive)
      //PTA->run<dg::analysis::pta::PointsToFlowSensitive>();
      PTA->run<dg::analysis::pta::PointerAnalysisFS>();
    else
      //PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();
      PTA->run<dg::analysis::pta::PointerAnalysisFI>();

    assert(IRDGraph.build(&subjectModule, PTA) &&
           "Error: failed to build dg dependence graph");
    assert(PTA && "BUG: Need points-to analysis");

    dg::analysis::LLVMReachingDefinitionsAnalysisOptions RDAOptions{};
    dg::analysis::rd::LLVMReachingDefinitions RDA(&subjectModule, PTA, RDAOptions);
    RDA.run(); // compute reaching definitions

    dg::LLVMDefUseAnalysis DUA(&IRDGraph, &RDA, PTA);
    DUA.run(); // add def-use edges according that

    delete PTA;

    IRDGraph.computeControlDependencies(cd_alg);
    */

    // Set options
    bool threads = false;
    const char *entry_func = "main";
    const char *rda = "dataflow";
    dg::llvmdg::LLVMDependenceGraphOptions options;
    options.cdAlgorithm = cd_alg;
    options.threads = threads; //true;
    options.PTAOptions.threads = threads;
    options.RDAOptions.threads = threads;
    options.PTAOptions.entryFunction = entry_func;
    options.RDAOptions.entryFunction = entry_func;

    if (isFlowSensitive)
        options.PTAOptions.analysisType
            = dg::analysis::LLVMPointerAnalysisOptions::AnalysisType::fs;
    else
        options.PTAOptions.analysisType
            = dg::analysis::LLVMPointerAnalysisOptions::AnalysisType::fi;

    if (strcmp(rda, "dataflow") == 0) {
        options.RDAOptions.analysisType
                = dg::analysis::LLVMReachingDefinitionsAnalysisOptions::AnalysisType::dataflow;
    } else if (strcmp(rda, "ssa") == 0) {
        options.RDAOptions.analysisType
                = dg::analysis::LLVMReachingDefinitionsAnalysisOptions::AnalysisType::ssa;
    } else {
        llvm::errs() << "Unknown reaching definitions analysis, try: dataflow, ssa\n";
        assert(false);
    }

    // dg graph
    dg::llvmdg::LLVMDependenceGraphBuilder dg_builder(&subjectModule, options);
    
    //dg::LLVMDependenceGraph IRDGraph = dg_builder.build();
    auto IRDGraph = dg_builder.build();

    // Build mutant DGraph
    //mutantDGraph.build(subjectModule, &IRDGraph, mutantInfos,
    mutantDGraph.build(subjectModule, IRDGraph.release(), mutantInfos,
                       mutant_depend_filename, disable_selection);
  } else {
    // load from file
    mutantDGraph.build(subjectModule, nullptr, mutantInfos,
                       mutant_depend_filename, disable_selection);
  }
}

MutantIDType
MutantSelection::pickMutant(std::unordered_set<MutantIDType> const &candidates,
                            std::vector<double> const &scores) {
  std::vector<MutantIDType> topScored;

  // First see which are having the maximum score
  double top_score = -1 / 0.0; // 0.0; // -Infinity
  unsigned numTopMuts = 0;
  for (auto mutant_id : candidates) {
    if (scores[mutant_id] >= top_score) {
      if (scores[mutant_id] > top_score) {
        top_score = scores[mutant_id];
        numTopMuts = 0;
      }
      ++numTopMuts;
    }
  }

  if (numTopMuts ==
      0) { // Normally should not happend. put this just in case ...
    for (auto v: candidates)
      llvm::errs() << "(" << v << ", " << scores[v] << "), ";
    assert(false && "This function is called only with candidate non empty");
    topScored.insert(topScored.end(), candidates.begin(), candidates.end());
  } else {
    topScored.reserve(numTopMuts);
    for (auto mutant_id : candidates)
      if (scores[mutant_id] == top_score) {
        topScored.push_back(mutant_id);
      }
  }
  std::vector<MutantIDType> &choiceFinalRoundList = topScored;

  /*
  // Second see which among maximum score have smallest number of incoming
  // control dependence
  std::vector<MutantIDType> choiceFinalRoundList;
  unsigned minoutctrlnum = (unsigned)-1;
  unsigned maxinctrlnum = 0;
  for (auto mutant_id : topScored) {
    if (mutantDGraph.getOutCtrlDependents(mutant_id).size() < minoutctrlnum) {
      choiceFinalRoundList.clear();
      choiceFinalRoundList.push_back(mutant_id);
      minoutctrlnum = mutantDGraph.getOutCtrlDependents(mutant_id).size();
      maxinctrlnum = mutantDGraph.getInCtrlDependents(mutant_id).size();
    } else if (mutantDGraph.getOutCtrlDependents(mutant_id).size() ==
                   minoutctrlnum &&
               mutantDGraph.getInCtrlDependents(mutant_id).size() >
                   maxinctrlnum) {
      choiceFinalRoundList.clear();
      choiceFinalRoundList.push_back(mutant_id);
      maxinctrlnum = mutantDGraph.getInCtrlDependents(mutant_id).size();
    }
  }
  */

  // shuflle IRs
  std::srand(std::time(NULL) + clock()); //+ clock() because fast running
  // program will generate same sequence
  // with only time(NULL)
  std::random_shuffle(choiceFinalRoundList.begin(), choiceFinalRoundList.end());
  MutantIDType chosenMutant = choiceFinalRoundList.front();
  return chosenMutant;
}

/**
 * \brief relaxes a mutant by affecting the score of its dependent mutants up to
 * a certain hop
 */
void MutantSelection::relaxMutant(MutantIDType mutant_id,
                                  std::vector<double> &scores) {
  // const MutantIDType ORIGINAL_ID = 0; //no mutant have id 0, that's the
  // original's

  for (auto inrel : mutantDGraph.getInMutantRelationStrength(mutant_id)) {
    scores[inrel.first] -= /*scores[inrel.first] * */(inrel.second + mutantDGraph.getOutRelationStrength(inrel.first, mutant_id));
  }
  for (auto outrel : mutantDGraph.getOutMutantRelationStrength(mutant_id)) {
    scores[outrel.first] -= /*scores[outrel.first] * */(outrel.second + mutantDGraph.getInRelationStrength(outrel.first, mutant_id));
  }
  

  /*
  // at data dependency hop n (in and out), and the corresponding node having
  // score x, set its value to x'={x - x*RELAX_STEP/n}, if (RELAX_STEP/n) >=
  // RELAX_THRESHOLD
  unsigned curhop = 1;
  double cur_relax_factor = RELAX_STEP / curhop;
  while (cur_relax_factor >= RELAX_THRESHOLD) {

    // expand
    for (auto inDatDepMut :
         mutantDGraph.getHopsInDataDependents(mutant_id, curhop)) {
      // the score still decrease if there are many dependency path to this
      // mutants (ex: a=1; b=a+3; c=a+b;). here inDatDepMut is at 'a=1'
      // explore also the case where we increase back if it was already
      // visited(see out bellow)
      scores[inDatDepMut] -= scores[inDatDepMut] * cur_relax_factor;
    }
    for (auto outDatDepMut :
         mutantDGraph.getHopsOutDataDependents(mutant_id, curhop)) {
      // the score still decrease if there are many dependency path to this
      // mutants (ex: a=1; b=a+3; c=a+b;). here inDatDepMut is at 'c=a+b'
      scores[outDatDepMut] -= scores[outDatDepMut] * cur_relax_factor;
    }

    // XXX: For now we decrease score for those ctrl dependents. TODO TODO
    for (auto inCtrlDepMut :
         mutantDGraph.getHopsInCtrlDependents(mutant_id, curhop)) {
      scores[inCtrlDepMut] -= scores[inCtrlDepMut] * cur_relax_factor;
    }
    for (auto outCtrlDepMut :
         mutantDGraph.getHopsOutCtrlDependents(mutant_id, curhop)) {
      scores[outCtrlDepMut] -= scores[outCtrlDepMut] * cur_relax_factor;
    }

    // next hop
    ++curhop;
    cur_relax_factor = RELAX_STEP / curhop;
  }
  */
}

/**
 *
 */
void MutantSelection::getMachineLearningPrediction(
    std::vector<float> &couplingProbabilitiesOut, std::string modelFilename, 
    bool isDefectPrediction) {
  MutantIDType mutants_number = mutantInfos.getMutantsNumber();
  couplingProbabilitiesOut.reserve(mutants_number);

  std::vector<std::vector<float>> features_matrix;
  std::vector<std::string> features_names;

  PredictionModule predmodule(modelFilename);

  if (isDefectPrediction) {
    std::vector<float> defectProbabilitiesOut;
    std::vector<std::unordered_set<MutantIDType>> mutantsPerStmt;
    mutantDGraph.computeStatementFeatures(features_matrix, features_names, mutantsPerStmt);
    predmodule.predict(features_matrix, features_names, defectProbabilitiesOut);
    assert(defectProbabilitiesOut.size() == mutantsPerStmt.size() && "Statement count mismatch");
    couplingProbabilitiesOut.resize(mutants_number, 0);
    for (auto sid = 0; sid < defectProbabilitiesOut.size(); ++sid)
      for (auto mid: mutantsPerStmt.at(sid))
        couplingProbabilitiesOut.at(mid-1) = defectProbabilitiesOut[sid];
  } else {
    mutantDGraph.computeMutantFeatures(features_matrix, features_names);
    predmodule.predict(features_matrix, features_names, couplingProbabilitiesOut);
  }
}

/**
 * \brief Stop when the best selected mutant's score is less to the
 * score_threshold or there are no mutant left
 */
void MutantSelection::smartSelectMutants(
    std::vector<MutantIDType> &selectedMutants,
    std::vector<float> &cachedPrediction, std::string trainedModelFilename,
    bool mlOn, bool mclOn, bool defectPredOn) {

  MutantIDType mutants_number = mutantInfos.getMutantsNumber();

  selectedMutants.reserve(mutants_number);

  // Choose starting mutants (random for now: 1 mutant per dependency cluster)
  std::vector<double> mutant_scores(mutants_number + 1);
  //std::unordered_set<MutantIDType> visited_mutants;
  std::vector<std::unordered_set<MutantIDType>> candidate_mutants_clusters(mutantDGraph.getDDClusters());

  /// Get Machine Learning coupling prediction into a vector as probability
  /// to be coupled, for each mutant
  std::vector<float> isCoupledProbability;
  if (mlOn) {
    if (cachedPrediction.empty()) {
      getMachineLearningPrediction(isCoupledProbability, trainedModelFilename, defectPredOn);
      cachedPrediction = isCoupledProbability;
    } else {
      isCoupledProbability = cachedPrediction;
    }
    /*std::vector<bool> eqprob;
    eqprob.resize(isCoupledProbability.size()*80/100.0, false);
    eqprob.resize(isCoupledProbability.size(), true);
    std::srand(std::time(NULL) + clock()); //+ clock() because fast running
    std::random_shuffle(eqprob.begin(), eqprob.end());
    for (MutantIDType i=0; i<isCoupledProbability.size(); ++i)
      isCoupledProbability[i] *= (!eqprob[i]);*/
  } else {    // MCL only
    isCoupledProbability.resize(mutants_number, 0.0);
  }
  assert(isCoupledProbability.size() == mutants_number &&
         "returned prediction list do not match with number of mutants");

  // hash the probabilities to 0 or 1
  // for (auto it=isCoupledProbability.begin(), ie=isCoupledProbability.end();
  // it!=ie; ++it)
  //  *it = (*it > 0.5);

  if (mlOn && !mclOn) {    //ML only
    for (MutantIDType mid = 1; mid <= mutants_number; ++mid)
      selectedMutants.push_back(mid);
    std::sort(selectedMutants.begin(), selectedMutants.end(),
              [isCoupledProbability](MutantIDType a, MutantIDType b) {
                //<: lower to higher, >: //higher to lower
                return (isCoupledProbability[a - 1] > isCoupledProbability[b - 1]); 
              });
    
    // randomize within same score
    auto ifirst = selectedMutants.begin();
    auto ilast = ifirst;
    auto isize = ifirst + selectedMutants.size() - 1;
    auto iend = selectedMutants.end();
    while (ilast != iend) {
      if (*ifirst != *isize) {
        while (*ifirst == *ilast)
          ++ilast;
        // randomize
        std::srand(std::time(NULL) + clock()); //+ clock() because fast running
        std::random_shuffle(ifirst, ilast - 1);
        ifirst = ilast;
      } else {
        ilast = iend;
        std::srand(std::time(NULL) + clock()); //+ clock() because fast running
        std::random_shuffle(ifirst, ilast);
      }
    }
    // for (auto i : selectedMutants) llvm::errs() << isCoupledProbability[i-1]
    // << " ";
    return; 
  }

  // Initialize scores
  for (MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id) {
    mutant_scores[mutant_id] = MAX_SCORE + isCoupledProbability[mutant_id - 1];
    //candidate_mutants.insert(mutant_id);
  }

  /*
    // XXX trim initial score according the mutants features
    unsigned maxInDataDep = 0;
    unsigned minInDataDep = (unsigned)-1;
    double kInDataDep = 0.03 / AMPLIFIER;
    unsigned maxOutDataDep = 0;
    unsigned minOutDataDep = (unsigned)-1;
    double kOutDataDep = 0.0 / AMPLIFIER;
    unsigned maxInCtrlDep = 0;
    unsigned minInCtrlDep = (unsigned)-1;
    double kInCtrlDep = 0.01 / AMPLIFIER;
    unsigned maxOutCtrlDep = 0;
    unsigned minOutCtrlDep = (unsigned)-1;
    double kOutCtrlDep = 0.01 / AMPLIFIER;
    unsigned maxTieDep = 0;
    unsigned minTieDep = (unsigned)-1;
    double kTieDep = 0.0 / AMPLIFIER;
    unsigned maxComplexity = 0;
    unsigned minComplexity = (unsigned)-1;
    double kComplexity = 0.0 / AMPLIFIER;
    unsigned maxCfgDepth = 0;
    unsigned minCfgDepth = (unsigned)-1;
    double kCfgDepth = 0.0 / AMPLIFIER;
    unsigned maxCfgPredNum = 0;
    unsigned minCfgPredNum = (unsigned)-1;
    double kCfgPredNum = 0.0 / AMPLIFIER;
    unsigned maxCfgSuccNum = 0;
    unsigned minCfgSuccNum = (unsigned)-1;
    double kCfgSuccNum = 0.0 / AMPLIFIER;
    unsigned maxNumAstParent = 0;
    unsigned minNumAstParent = (unsigned)-1;
    double kNumAstParent = 0.0 / AMPLIFIER;
  */

  // Re-initialise the weights from the JSON weight file when applicable
  /*if (!weightsJsonfilename.empty()) {
    JsonBox::Value jbvalue;
    jbvalue.loadFromFile(weightsJsonfilename);
    assert(jbvalue.isObject() &&
           "The JSON file for weights must contain JSON Object");
    JsonBox::Object wo = jbvalue.getObject();

    assert(wo.count("wInDataDep") && wo["wInDataDep"].isDouble());
    kInDataDep = std::stod(wo["wInDataDep"].getString());

    assert(wo.count("wOutDataDep") && wo["wOutDataDep"].isDouble());
    kOutDataDep = std::stod(wo["wOutDataDep"].getString());

    assert(wo.count("wInCtrlDep") && wo["wInCtrlDep"].isDouble());
    kInCtrlDep = std::stod(wo["wInCtrlDep"].getString());

    assert(wo.count("wOutCtrlDep") && wo["wOutCtrlDep"].isDouble());
    kOutCtrlDep = std::stod(wo["wOutCtrlDep"].getString());

    assert(wo.count("wTieDep") && wo["wTieDep"].isDouble());
    kTieDep = std::stod(wo["wTieDep"].getString());

    assert(wo.count("wComplexity") && wo["wComplexity"].isDouble());
    kComplexity = std::stod(wo["wComplexity"].getString());

    assert(wo.count("wCfgDepth") && wo["wCfgDepth"].isDouble());
    kCfgDepth = std::stod(wo["wCfgDepth"].getString());

    assert(wo.count("wCfgPredNum") && wo["wCfgPredNum"].isDouble());
    kCfgPredNum = std::stod(wo["wCfgPredNum"].getString());

    assert(wo.count("wCfgSuccNum") && wo["wCfgSuccNum"].isDouble());
    kCfgSuccNum = std::stod(wo["wCfgSuccNum"].getString());

    assert(wo.count("wNumAstParent") && wo["wNumAstParent"].isDouble());
    kNumAstParent = std::stod(wo["wNumAstParent"].getString());
  }*/

  /*
    // ... Initialize max, mins
    for (MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id) {
      if (mutantDGraph.getInDataDependents(mutant_id).size() > maxInDataDep)
        maxInDataDep = mutantDGraph.getInDataDependents(mutant_id).size();
      if (mutantDGraph.getInDataDependents(mutant_id).size() < minInDataDep)
        minInDataDep = mutantDGraph.getInDataDependents(mutant_id).size();
      if (mutantDGraph.getOutDataDependents(mutant_id).size() > maxOutDataDep)
        maxOutDataDep = mutantDGraph.getOutDataDependents(mutant_id).size();
      if (mutantDGraph.getOutDataDependents(mutant_id).size() < minOutDataDep)
        minOutDataDep = mutantDGraph.getOutDataDependents(mutant_id).size();

      if (mutantDGraph.getInCtrlDependents(mutant_id).size() > maxInCtrlDep)
        maxInCtrlDep = mutantDGraph.getInCtrlDependents(mutant_id).size();
      if (mutantDGraph.getInCtrlDependents(mutant_id).size() < minInCtrlDep)
        minInCtrlDep = mutantDGraph.getInCtrlDependents(mutant_id).size();
      if (mutantDGraph.getOutCtrlDependents(mutant_id).size() > maxOutCtrlDep)
        maxOutCtrlDep = mutantDGraph.getOutCtrlDependents(mutant_id).size();
      if (mutantDGraph.getOutCtrlDependents(mutant_id).size() < minOutCtrlDep)
        minOutCtrlDep = mutantDGraph.getOutCtrlDependents(mutant_id).size();

      if (mutantDGraph.getTieDependents(mutant_id).size() > maxTieDep)
        maxTieDep = mutantDGraph.getTieDependents(mutant_id).size();
      if (mutantDGraph.getTieDependents(mutant_id).size() < minTieDep)
        minTieDep = mutantDGraph.getTieDependents(mutant_id).size();

      if (mutantDGraph.getComplexity(mutant_id) > maxComplexity)
        maxComplexity = mutantDGraph.getComplexity(mutant_id);
      if (mutantDGraph.getComplexity(mutant_id) < minComplexity)
        minComplexity = mutantDGraph.getComplexity(mutant_id);

      if (mutantDGraph.getCfgDepth(mutant_id) > maxCfgDepth)
        maxCfgDepth = mutantDGraph.getCfgDepth(mutant_id);
      if (mutantDGraph.getCfgDepth(mutant_id) < minCfgDepth)
        minCfgDepth = mutantDGraph.getCfgDepth(mutant_id);

      if (mutantDGraph.getCfgPredNum(mutant_id) > maxCfgPredNum)
        maxCfgPredNum = mutantDGraph.getCfgPredNum(mutant_id);
      if (mutantDGraph.getCfgPredNum(mutant_id) < minCfgPredNum)
        minCfgPredNum = mutantDGraph.getCfgPredNum(mutant_id);

      if (mutantDGraph.getCfgSuccNum(mutant_id) > maxCfgSuccNum)
        maxCfgSuccNum = mutantDGraph.getCfgSuccNum(mutant_id);
      if (mutantDGraph.getCfgSuccNum(mutant_id) < minCfgSuccNum)
        minCfgSuccNum = mutantDGraph.getCfgSuccNum(mutant_id);

      if (mutantDGraph.getAstParentsOpcodeNames(mutant_id).size() >
          maxNumAstParent)
        maxNumAstParent =
    mutantDGraph.getAstParentsOpcodeNames(mutant_id).size();
      if (mutantDGraph.getAstParentsOpcodeNames(mutant_id).size() <
          minNumAstParent)
        minNumAstParent =
    mutantDGraph.getAstParentsOpcodeNames(mutant_id).size();
    }
    // ... Actual triming
    for (MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id) {
      if (maxInDataDep != minInDataDep)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getInDataDependents(mutant_id).size() - minInDataDep)
    *
            kInDataDep / (maxInDataDep - minInDataDep);
      if (maxOutDataDep != minOutDataDep)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getOutDataDependents(mutant_id).size() -
             minOutDataDep) *
            kOutDataDep / (maxOutDataDep - minOutDataDep);
      if (maxInCtrlDep != minInCtrlDep)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getInCtrlDependents(mutant_id).size() - minInCtrlDep)
    *
            kInCtrlDep / (maxInCtrlDep - minInCtrlDep);
      if (maxOutCtrlDep != minOutCtrlDep)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getOutCtrlDependents(mutant_id).size() -
             minOutCtrlDep) *
            kOutCtrlDep / (maxOutCtrlDep - minOutCtrlDep);
      if (maxTieDep != minTieDep)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getTieDependents(mutant_id).size() - minTieDep) *
            kTieDep / (maxTieDep - minTieDep);
      if (maxComplexity != minComplexity)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getComplexity(mutant_id) - minComplexity) *
            kComplexity / (maxComplexity - minComplexity);
      if (maxCfgDepth != minCfgDepth)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getCfgDepth(mutant_id) - minCfgDepth) * kCfgDepth /
            (maxCfgDepth - minCfgDepth);
      if (maxCfgPredNum != minCfgPredNum)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getCfgPredNum(mutant_id) - minCfgPredNum) *
            kCfgPredNum / (maxCfgPredNum - minCfgPredNum);
      if (maxCfgSuccNum != minCfgSuccNum)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getCfgSuccNum(mutant_id) - minCfgSuccNum) *
            kCfgSuccNum / (maxCfgSuccNum - minCfgSuccNum);
      if (maxNumAstParent != minNumAstParent)
        mutant_scores[mutant_id] +=
            (mutantDGraph.getAstParentsOpcodeNames(mutant_id).size() -
             minNumAstParent) *
            kNumAstParent / (maxNumAstParent - minNumAstParent);
    }
  */

  // For now put all ties here and append to list at the end
  std::vector<unsigned long> clustershuffle;
  std::vector<unsigned long> clusters__;
  clustershuffle.reserve(mutants_number);
  for (unsigned long cluster_id=0; cluster_id < candidate_mutants_clusters.size(); ++cluster_id) {
    clusters__.push_back(cluster_id);
  }
  std::srand(std::time(NULL) + clock()); //+ clock() because fast running
  std::random_shuffle(clusters__.begin(), clusters__.end());
  for(auto cluster_id: clusters__) {
    auto &cluster = candidate_mutants_clusters[cluster_id];
    for (unsigned long occ=0; occ < cluster.size(); ++occ)
      clustershuffle.push_back(cluster_id);
  }

  //llvm::errs() << "#### " << candidate_mutants_clusters.size() << " clusters\n";

  // randomize with probability change the value less than to change the proba
  std::srand(std::time(NULL) + clock()); //+ clock() because fast running
  if (rand() % 100 < 100) {
    std::srand(std::time(NULL) + clock()); //+ clock() because fast running
    std::random_shuffle(clustershuffle.begin(), clustershuffle.end());
  }

  for (auto cid: clustershuffle) {
    auto mutant_id = pickMutant(candidate_mutants_clusters[cid], mutant_scores);
    //-----llvm::errs()<<candidate_mutants.size()<<"
    //"<<mutant_scores[mutant_id]<<"\n";
    // Stop if the selected mutant has a score less than the threshold
    // if (mutant_scores[mutant_id] < score_threshold)
    //    break;

    selectedMutants.push_back(mutant_id);
    relaxMutant(mutant_id, mutant_scores);

    // insert the picked mutants and its tie-dependents into visited set
    //visited_mutants.insert(mutant_id);
    //visited_mutants.insert(mutantDGraph.getTieDependents(mutant_id).begin(),
    //                       mutantDGraph.getTieDependents(mutant_id).end());

    // tiesTemporal.insert(mutantDGraph.getTieDependents(mutant_id).begin(),
    //                    mutantDGraph.getTieDependents(mutant_id).end());
    //// -- Remove Tie Dependents from candidates
    // for (auto tie_ids : mutantDGraph.getTieDependents(mutant_id))
    //  candidate_mutants.erase(tie_ids);
    
    //for (auto tieMId : mutantDGraph.getTieDependents(mutant_id))
    //  mutant_scores[mutant_id] -= TIE_REDUCTION_DIFF 
    //                        * (1 - isCoupledProbability[mutant_id - 1]);

    // Remove picked mutants from candidates
    candidate_mutants_clusters[cid].erase(mutant_id);
  }

  // append the ties temporal to selection
  // selectedMutants.insert(selectedMutants.end(), tiesTemporal.begin(),
  //                       tiesTemporal.end());

  // shuffle the part of selected with ties
  /*auto tieStart =
      selectedMutants.begin() + (selectedMutants.size() - tiesTemporal.size());
  if (tieStart != selectedMutants.end()) {
    assert(tiesTemporal.count(*tieStart) &&
           "Error: miscomputation above. Report bug");
    std::srand(std::time(NULL) + clock()); //+ clock() because fast running
    std::random_shuffle(tieStart, selectedMutants.end());
  }

  for (auto mid : tiesTemporal)
    selectedScores.push_back(-1e-100); // default score for ties (very low
                                       // value)*/

  if (selectedMutants.size() != mutants_number) {
    llvm::errs() << "\nError: Bug in Selection: some mutants left out or added "
                    "many times: Mutants number = ";
    llvm::errs() << mutants_number
                 << ", Selected number = " << selectedMutants.size()
                 //<< ", selected scores number = " << selectedScores.size()
                 << "\n\n";
  }
}

void MutantSelection::randomMutants(
    std::vector<MutantIDType> &spreadSelectedMutants,
    std::vector<MutantIDType> &dummySelectedMutants, unsigned long number) {
  MutantIDType mutants_number = mutantInfos.getMutantsNumber();

  assert(mutantDGraph.isBuilt() && "This function must be called after the "
                                   "mutantDGraph is build. We need mutants "
                                   "IRs");

  spreadSelectedMutants.reserve(mutants_number);
  dummySelectedMutants.reserve(mutants_number);

  /// \brief Spread Random
  std::vector<llvm::Value const *> mutatedIRs;
  std::unordered_map<llvm::Value const *, std::unordered_set<MutantIDType>>
      work_map(mutantDGraph.getIR2mutantsetMap());

  mutatedIRs.reserve(work_map.size());
  for (auto &ir2mutset : work_map) {
    mutatedIRs.push_back(ir2mutset.first);
  }

  bool someselected = true;
  while (someselected) {
    someselected = false;

    // shuflle IRs
    std::srand(std::time(NULL) + clock()); //+ clock() because fast running
    // program will generate same sequence
    // with only time(NULL)
    std::random_shuffle(mutatedIRs.begin(), mutatedIRs.end());

    unsigned long numberofnulled = 0;
    std::srand(std::time(NULL) + clock()); //+ clock() because fast running
    // program will generate same sequence
    // with only time(NULL)
    for (unsigned long irPos = 0, irE = mutatedIRs.size(); irPos < irE;
         ++irPos) {
      // randomly select one of its mutant
      auto &mutSet = work_map.at(mutatedIRs[irPos]);
      if (mutSet.empty()) {
        mutatedIRs[irPos] = nullptr;
        ++numberofnulled;
        continue;
      } else {
        auto rnd = std::rand() % mutSet.size();
        auto sit = mutSet.begin();
        std::advance(sit, rnd);
        auto selMut = *sit;
        spreadSelectedMutants.push_back(selMut);
        someselected = true;

        for (auto *m_ir : mutantDGraph.getIRsOfMut(selMut))
          work_map.at(m_ir).erase(selMut);
      }
    }
    // remove those that are null
    if (numberofnulled > 0) {
      std::sort(mutatedIRs.rbegin(),
                mutatedIRs.rend()); // reverse sort (the null are at  the end:
                                    // the last numberofnulled values)
      mutatedIRs.erase(mutatedIRs.begin() +
                           (mutatedIRs.size() - numberofnulled),
                       mutatedIRs.end());
    }
  }

  assert(spreadSelectedMutants.size() == mutants_number &&
         "Bug in the selection program"); // DBG

  if (spreadSelectedMutants.size() > number)
    spreadSelectedMutants.resize(number);

  /******/

  /// \brief Dummy Random
  for (MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id) {
    dummySelectedMutants.push_back(mutant_id);
  }

  // shuffle
  std::srand(std::time(NULL) + clock()); //+ clock() because fast running
                                         // program will generate same sequence
  // with only time(NULL)
  std::random_shuffle(dummySelectedMutants.begin(), dummySelectedMutants.end());

  // keep only the needed number
  if (dummySelectedMutants.size() > number)
    dummySelectedMutants.resize(number);
}

void MutantSelection::randomSDLMutants(
    std::vector<MutantIDType> &selectedMutants, unsigned long number) {
  /// Insert all SDL mutants here, note that any two SDL spawn different IRs
  MutantIDType mutants_number = mutantInfos.getMutantsNumber();
  for (MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id) {
    std::string nametype = mutantInfos.getMutantTypeName(mutant_id);
    if (UserMaps::containsDeleteStmtConfName(nametype))
      selectedMutants.push_back(mutant_id);
  }

  // shuffle
  std::srand(std::time(NULL) + clock()); //+ clock() because fast running
                                         // program will generate same sequence
  // with only time(NULL)
  std::random_shuffle(selectedMutants.begin(), selectedMutants.end());

  // keep only the needed number
  if (selectedMutants.size() > number)
    selectedMutants.resize(number);
}
