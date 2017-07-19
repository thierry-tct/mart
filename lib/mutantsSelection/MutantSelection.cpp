/**
 * -==== MutantSelection.cpp
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Implementation of mutant selection based on dependency analysis
 * and ... (this make use of DG - https://github.com/mchalupa/dg).
 */

#include <algorithm>
#include <cstdlib> /* srand, rand */
#include <ctime>

#include "llvm/Analysis/CFG.h"

#include "MutantSelection.h"

#ifndef ENABLE_CFG
#define ENABLE_CFG // needed by dg
#endif
// https://github.com/mchalupa/dg
#include "../third-parties/dg/src/llvm/LLVMDependenceGraph.h"
#include "../third-parties/dg/src/llvm/analysis/DefUse.h"
#include "../third-parties/dg/src/llvm/analysis/PointsTo/PointsTo.h"
#include "../third-parties/dg/src/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"

#include "../third-parties/dg/src/analysis/PointsTo/PointsToFlowInsensitive.h"
#include "../third-parties/dg/src/analysis/PointsTo/PointsToFlowSensitive.h"

using namespace mart;
using namespace mart::selection;

// Amplifier help reduce the different factors...
#define AMPLIFIER 10000

// class MutantDependenceGraph

void MutantDependenceGraph::addDataCtrlFor(
    dg::LLVMDependenceGraph const *subIRDg) {
  for (auto nodeIt = subIRDg->begin(), ne = subIRDg->end(); nodeIt != ne;
       ++nodeIt) {
    auto *nodefrom = nodeIt->second;
    llvm::Value *irFrom = nodefrom->getKey();
    for (auto ndata = nodefrom->data_begin(), nde = nodefrom->data_end();
         ndata != nde; ++ndata) {
      llvm::Value *irDataTo = (*ndata)->getKey();
      for (MutantIDType m_id_from : IR2mutantset[irFrom])
        for (MutantIDType m_id_datato : IR2mutantset[irDataTo])
          if (m_id_from != m_id_datato)
            addDataDependency(m_id_from, m_id_datato);
    }
    for (auto nctrl = nodefrom->control_begin(), nce = nodefrom->control_end();
         nctrl != nce; ++nctrl) {
      llvm::Value *irCtrlTo = (*nctrl)->getKey();
      for (MutantIDType m_id_from : IR2mutantset[irFrom])
        for (MutantIDType m_id_ctrlto : IR2mutantset[irCtrlTo])
          if (m_id_from != m_id_ctrlto)
            addCtrlDependency(m_id_from, m_id_ctrlto);
    }
  }
}

bool MutantDependenceGraph::build(llvm::Module const &mod,
                                  dg::LLVMDependenceGraph const *irDg,
                                  MutantInfoList const &mutInfos,
                                  std::string mutant_depend_filename) {
  std::unordered_map<std::string,
                     std::unordered_map<unsigned, llvm::Value const *>>
      functionName_position2IR;

  // First compute IR2mutantset
  for (auto &Func : mod) {
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
    for (auto &func: mod) {
      if (func.isDeclaration())
          continue;
      //workQ.clear();
      block_depth_map.clear();
      auto &entryBB = func.getEntryBlock();
      workQ.push(&entryBB);
      block_depth_map.emplace(&entryBB, 1);
      while (!workQ.empty()) {  //BFS
        auto *bb = workQ.front();
        auto depth = block_depth_map.at(bb);
        workQ.pop();
        unsigned nSuccs = 0;
        unsigned nPreds = 0;
        for (auto succIt=llvm::succ_begin(bb), succE=llvm::succ_end(bb); succIt != succE; ++succIt) {
          if (block_depth_map.count(*succIt) == 0) {
            block_depth_map.emplace(*succIt, depth+1);
            workQ.push(*succIt);
          }
          ++nSuccs;
        }

        for (auto predIt=llvm::pred_begin(bb), predE=llvm::pred_end(bb); predIt != predE; ++predIt) 
          ++nPreds;
        
        std::string bbTypename = bb->getName().str();

        // Update the info for all the mutants for this popped basic block TODO TODO
        std::unordered_set<MutantIDType> mutantsofstmt; 
        std::unordered_set<const llvm::Instruction *> visitedI;
        for (auto &inst: *bb) { 
          if (visitedI.count(&inst) || IR2mutantset.count(&inst) == 0)
            continue;
          mutantsofstmt.clear();
          while (true) {
            std::unordered_set<MutantIDType> tmpmuts(IR2mutantset.at(&inst));
            std::unordered_set<MutantIDType> tmpnewmuts;
            for (auto mId: tmpmuts) {
              if (mutantsofstmt.insert(mId).second) {
                tmpnewmuts.insert(mId);
              }
            }
            if (tmpnewmuts.empty())
              break;
            for (auto mId: tmpnewmuts) {
              for (auto *minst: mutant2IRset.at(mId)) {
                assert (llvm::dyn_cast<llvm::Instruction>(minst)->getParent() == bb 
                         && "mutants spawning multiple BBs not yet implemented. TODO");
                visitedI.insert(llvm::dyn_cast<llvm::Instruction>(minst));
              }
              tmpmuts.insert(getTieDependents(mId).begin(), getTieDependents(mId).end());
            }
          }
          // set the info for each mutant here
          for (auto mutant_id: mutantsofstmt) {
            setMutantTypename(mutant_id, mutInfos.getMutantTypeName(mutant_id));
            setStmtBBTypename(mutant_id, bbTypename);
            setComplexity(mutant_id, mutantsofstmt.size());
            setCFGDepthPredSuccNum(mutant_id, depth, nPreds, nSuccs);
            auto &mutInsts = mutant2IRset.at(mutant_id);
            for (auto *minst: mutInsts) {
              for (auto *par: minst->users())
                if (mutInsts.count(par) == 0)
                  if (auto astPar = llvm::dyn_cast<llvm::Instruction>(par))
                    addAstParents(mutant_id, astPar);
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
  llvm::errs() << "\t... graph Builded/Loaded\n";
}

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

    //Others
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
    tmpobj["cfgPredNum"] = JsonBox::Value(std::to_string(getCfgPredNum(mutant_id)));
    tmpobj["cfgSuccNum"] = JsonBox::Value(std::to_string(getCfgSuccNum(mutant_id)));
    tmpobj["complexity"] = JsonBox::Value(std::to_string(getComplexity(mutant_id)));

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
      addDataDependency(
          mutant_id, mutantid_str2int.at(tmpstr)); // out of bound if the value
                                                   // is not a mutant id as
                                                   // unsigned (MutantIDType)
    }
    assert(tmpobj["outCtrlDependents"].isArray() &&
           "'outCtrlDependents' must be an Array");
    tmparr = tmpobj["outCtrlDependents"].getArray();
    for (JsonBox::Value ocid : tmparr) {
      assert(ocid.isString() && "the mutant in each dependence set should be "
                                "represented as an intger string "
                                "(outCtrlDepends)");
      std::string tmpstr = ocid.getString();
      addCtrlDependency(
          mutant_id, mutantid_str2int.at(tmpstr)); // out of bound if the value
                                                   // is not a mutant id as
                                                   // unsigned (MutantIDType)
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

    //Others
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
    
    assert (tmpobj["mutantTypename"].isString() && "mutantTypename must be string");
    setMutantTypename(mutant_id, tmpobj["mutantTypename"].getString());
    assert (tmpobj["stmtBBTypename"].isString() && "stmtBBTypename must be string");
    setStmtBBTypename(mutant_id, tmpobj["stmtBBTypename"].getString());
    assert (tmpobj["cfgDepth"].isInteger() && "cfgDepth must be int");
    assert (tmpobj["cfgPrednum"].isInteger() && "cfgPredNum must be int");
    assert (tmpobj["cfgSuccNum"].isInteger() && "cfgSuccNum must be int");
    setCFGDepthPredSuccNum(mutant_id, tmpobj["cfgDepth"].getInteger(), tmpobj["cfgPredNum"].getInteger(), tmpobj["cfgSuccNum"].getInteger());
    assert (tmpobj["complexity"].isInteger() && "complexity must be int");
    setComplexity(mutant_id, tmpobj["complexity"].getInteger());
  }
}
/////

// class MutantSelection

void MutantSelection::buildDependenceGraphs(std::string mutant_depend_filename,
                                            bool rerundg, bool isFlowSensitive,
                                            bool isClassicCtrlDepAlgo) {
  if (rerundg) {
    dg::CD_ALG cd_alg;
    if (isClassicCtrlDepAlgo)
      cd_alg = dg::CLASSIC;
    else
      cd_alg = dg::CONTROL_EXPRESSION;

    // build IR DGraph
    dg::LLVMDependenceGraph IRDGraph;
    dg::LLVMPointerAnalysis *PTA = new dg::LLVMPointerAnalysis(&subjectModule);
    if (isFlowSensitive)
      PTA->run<dg::analysis::pta::PointsToFlowSensitive>();
    else
      PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();

    assert(IRDGraph.build(&subjectModule, PTA) &&
           "Error: failed to build dg dependence graph");
    assert(PTA && "BUG: Need points-to analysis");

    dg::analysis::rd::LLVMReachingDefinitions RDA(&subjectModule, PTA);
    RDA.run(); // compute reaching definitions

    dg::LLVMDefUseAnalysis DUA(&IRDGraph, &RDA, PTA);
    DUA.run(); // add def-use edges according that

    delete PTA;

    IRDGraph.computeControlDependencies(cd_alg);

    // Build mutant DGraph
    mutantDGraph.build(subjectModule, &IRDGraph, mutantInfos,
                       mutant_depend_filename);
  } else {
    // load from file
    mutantDGraph.build(subjectModule, nullptr, mutantInfos,
                       mutant_depend_filename);
  }
}

MutantIDType
MutantSelection::pickMutant(std::unordered_set<MutantIDType> const &candidates,
                            std::vector<double> const &scores) {
  std::vector<MutantIDType> topScored;

  // First see which are having the maximum score
  double top_score = -1/0.0; //0.0; // -Infinity
  for (auto mutant_id : candidates) {
    if (scores[mutant_id] >= top_score) {
      if (scores[mutant_id] > top_score) {
        top_score = scores[mutant_id];
        topScored.clear();
      }
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
  const double RELAX_STEP = 0.05 / AMPLIFIER;
  const double RELAX_THRESHOLD = 0.01 / AMPLIFIER; // 5 hops
  // const MutantIDType ORIGINAL_ID = 0; //no mutant have id 0, that's the
  // original's

  // at data dependency hop n (in and out), and the corresponding node having
  // score x, set its value to x'={x - x*RELAX_STEP/n}, if (RELAX_STEP/n) >=
  // RELAX_THRESHOLD
  std::unordered_set<MutantIDType> visited;
  std::queue<MutantIDType> workQueue;
  MutantIDType next_hop_first_mutant = mutant_id;
  unsigned curhop = 0;
  double cur_relax_factor = 0.0;
  workQueue.push(mutant_id);
  visited.insert(mutant_id);
  while (!workQueue.empty()) {
    auto elem = workQueue.front();
    workQueue.pop();

    if (next_hop_first_mutant == elem) {
      ++curhop;
      cur_relax_factor = RELAX_STEP / curhop;
    }

    // Check if treshold reached.
    // Since we use BFS, when an element reaches the threshold, all consecutive
    // element will also reach so we can safely stop
    if (cur_relax_factor < RELAX_THRESHOLD)
      break;

    // expand
    for (auto inDatDepMut : mutantDGraph.getInDataDependents(elem)) {
      // relax
      if (visited.insert(inDatDepMut).second) {
        if (next_hop_first_mutant == elem)
          next_hop_first_mutant = inDatDepMut;
        workQueue.push(inDatDepMut);
      }

      // the score still decrease if there are many dependency path to this
      // mutants (ex: a=1; b=a+3; c=a+b;). here inDatDepMut is at 'a=1'
      // explore also the case where we increase back if it was already
      // visited(see out bellow)
      scores[inDatDepMut] -= scores[inDatDepMut] * cur_relax_factor;
    }
    for (auto outDatDepMut : mutantDGraph.getOutDataDependents(elem)) {
      // relax
      if (visited.insert(outDatDepMut).second) {
        if (next_hop_first_mutant == elem)
          next_hop_first_mutant = outDatDepMut;
        workQueue.push(outDatDepMut);
      }

      // the score still decrease if there are many dependency path to this
      // mutants (ex: a=1; b=a+3; c=a+b;). here inDatDepMut is at 'c=a+b'
      scores[outDatDepMut] -= scores[outDatDepMut] * cur_relax_factor;
    }
  }
}

/**
 * \brief Stop when the best selected mutant's score is less to the
 * score_threshold or there are no mutant left
 */
void MutantSelection::smartSelectMutants(
    std::vector<MutantIDType> &selectedMutants,
    std::vector<double> &selectedScores) {
  const double MAX_SCORE = 1.0;

  MutantIDType mutants_number = mutantInfos.getMutantsNumber();

  selectedMutants.reserve(mutants_number);
  selectedScores.reserve(mutants_number);

  // Choose starting mutants (random for now: 1 mutant per dependency cluster)
  std::vector<double> mutant_scores(mutants_number + 1);
  std::unordered_set<MutantIDType> visited_mutants;
  std::unordered_set<MutantIDType> candidate_mutants;

  // Initialize scores
  for (MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id) {
    mutant_scores[mutant_id] = MAX_SCORE;
    candidate_mutants.insert(mutant_id);
  }

  // XXX trim initial score according the mutants features
  unsigned maxInDataDep = 0;
  unsigned minInDataDep = (unsigned)-1;
  double kInDataDep = 0.05 / AMPLIFIER;
  unsigned maxOutDataDep = 0;
  unsigned minOutDataDep = (unsigned)-1;
  double kOutDataDep = -0.025 / AMPLIFIER;
  unsigned maxInCtrlDep = 0;
  unsigned minInCtrlDep = (unsigned)-1;
  double kInCtrlDep = 0.1 / AMPLIFIER;
  unsigned maxOutCtrlDep = 0;
  unsigned minOutCtrlDep = (unsigned)-1;
  double kOutCtrlDep = 0.01 / AMPLIFIER;
  unsigned maxTieDep = 0;
  unsigned minTieDep = (unsigned)-1;
  double kTieDep = 0.05 / AMPLIFIER;
  unsigned maxComplexity = 0;
  unsigned minComplexity = (unsigned)-1;
  double kComplexity = 0.1 / AMPLIFIER;
  unsigned maxCfgDepth = 0;
  unsigned minCfgDepth = (unsigned)-1;
  double kCfgDepth = 0.01 / AMPLIFIER;
  unsigned maxCfgPredNum = 0;
  unsigned minCfgPredNum = (unsigned)-1;
  double kCfgPredNum = 0.025 / AMPLIFIER;
  unsigned maxCfgSuccNum = 0;
  unsigned minCfgSuccNum = (unsigned)-1;
  double kCfgSuccNum = 0.025 / AMPLIFIER;
  unsigned maxNumAstParent = 0;
  unsigned minNumAstParent = (unsigned)-1;
  double kNumAstParent = -0.01 / AMPLIFIER;

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

    if (mutantDGraph.getAstParentsOpcodeNames(mutant_id).size() > maxNumAstParent)
      maxNumAstParent = mutantDGraph.getAstParentsOpcodeNames(mutant_id).size();
    if (mutantDGraph.getAstParentsOpcodeNames(mutant_id).size() < minNumAstParent)
      minNumAstParent = mutantDGraph.getAstParentsOpcodeNames(mutant_id).size();
  }
  // ... Actual triming
  for (MutantIDType mutant_id = 1; mutant_id <= mutants_number; ++mutant_id) {
    mutant_scores[mutant_id] += (mutantDGraph.getInDataDependents(mutant_id).size() - minInDataDep) * kInDataDep / (maxInDataDep - minInDataDep);
    mutant_scores[mutant_id] += (mutantDGraph.getOutDataDependents(mutant_id).size() - minOutDataDep) * kOutDataDep / (maxOutDataDep - minOutDataDep);
    mutant_scores[mutant_id] += (mutantDGraph.getInCtrlDependents(mutant_id).size() - minInCtrlDep) * kInCtrlDep / (maxInCtrlDep - minInCtrlDep);
    mutant_scores[mutant_id] += (mutantDGraph.getOutCtrlDependents(mutant_id).size() - minOutCtrlDep) * kOutCtrlDep / (maxOutCtrlDep - minOutCtrlDep);
    mutant_scores[mutant_id] += (mutantDGraph.getTieDependents(mutant_id).size() - minTieDep) * kTieDep / (maxTieDep - minTieDep);
    mutant_scores[mutant_id] += (mutantDGraph.getComplexity(mutant_id) - minComplexity) * kComplexity / (maxComplexity - minComplexity);
    mutant_scores[mutant_id] += (mutantDGraph.getCfgDepth(mutant_id) - minCfgDepth) * kCfgDepth / (maxCfgDepth - minCfgDepth);
    mutant_scores[mutant_id] += (mutantDGraph.getCfgPredNum(mutant_id) - minCfgPredNum) * kCfgPredNum / (maxCfgPredNum - minCfgPredNum);
    mutant_scores[mutant_id] += (mutantDGraph.getCfgSuccNum(mutant_id) - minCfgSuccNum) * kCfgSuccNum / (maxCfgSuccNum - minCfgSuccNum);
    mutant_scores[mutant_id] += (mutantDGraph.getAstParentsOpcodeNames(mutant_id).size() - minNumAstParent) * kNumAstParent / (maxNumAstParent - minNumAstParent);
  }

  // For now put all ties here and append to list at the end
  std::unordered_set<MutantIDType> tiesTemporal;
  double tieReductiondiff = 0.2 / AMPLIFIER;

  while (!candidate_mutants.empty()) {
    auto mutant_id = pickMutant(candidate_mutants, mutant_scores);
    //-----llvm::errs()<<candidate_mutants.size()<<"
    //"<<mutant_scores[mutant_id]<<"\n";
    // Stop if the selected mutant has a score less than the threshold
    // if (mutant_scores[mutant_id] < score_threshold)
    //    break;

    selectedMutants.push_back(mutant_id);
    selectedScores.push_back(mutant_scores[mutant_id]);
    relaxMutant(mutant_id, mutant_scores);

    // insert the picked mutants and its tie-dependents into visited set
    visited_mutants.insert(mutant_id);
    visited_mutants.insert(mutantDGraph.getTieDependents(mutant_id).begin(),
                           mutantDGraph.getTieDependents(mutant_id).end());

    //tiesTemporal.insert(mutantDGraph.getTieDependents(mutant_id).begin(),
    //                    mutantDGraph.getTieDependents(mutant_id).end());
    //// -- Remove Tie Dependents from candidates
    //for (auto tie_ids : mutantDGraph.getTieDependents(mutant_id))
    //  candidate_mutants.erase(tie_ids);
    for (auto tieMId: mutantDGraph.getTieDependents(mutant_id))
      mutant_scores[mutant_id] -= tieReductiondiff;

    // Remove picked mutants from candidates
    candidate_mutants.erase(mutant_id);
  }

  // append the ties temporal to selection
  selectedMutants.insert(selectedMutants.end(), tiesTemporal.begin(),
                         tiesTemporal.end());

  // shuffle the part of selected with ties
  auto tieStart =
      selectedMutants.begin() + (selectedMutants.size() - tiesTemporal.size());
  if (tieStart != selectedMutants.end()) {
    assert(tiesTemporal.count(*tieStart) &&
           "Error: miscomputation above. Report bug");
    std::srand(std::time(NULL) + clock()); //+ clock() because fast running
    std::random_shuffle(tieStart, selectedMutants.end());
  }

  for (auto mid : tiesTemporal)
    selectedScores.push_back(-1e-100); // default score for ties (very low
                                       // value)

  if (selectedMutants.size() != mutants_number ||
      selectedScores.size() != mutants_number) {
    llvm::errs() << "\nError: Bug in Selection: some mutants left out or added "
                    "many times: Mutants number = ";
    llvm::errs() << mutants_number
                 << ", Selected number = " << selectedMutants.size()
                 << ", selected scores number = " << selectedScores.size()
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
