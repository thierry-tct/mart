/**
 * -==== Mart-Training.cpp
 *
 *                Mart Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Main source file that implements model training
 * for Smart Selection.
 */

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
//#include <unordered_map>
//#include <unordered_set>

#include "../lib/mutantsSelection/MutantSelection.h" //for PredictionModule

#include "llvm/Support/CommandLine.h" //llvm::cl

using namespace mart;
using namespace mart::selection;

#define TOOLNAME "Mart-Training"
#include "tools_commondefs.h"

#define NaN (0.0)

void loadMapOfConsideredMutants(
    std::string inputMapFilename,
    std::unordered_map<std::string, std::unordered_set<MutantIDType>> &project2consideredMuts) {
  std::ifstream m_in(inputMapFilename);
  std::string line, projectID, tmp;
  while (std::getline(m_in, line)) {
    std::istringstream ss(line);
    std::getline(ss, projectID, ' ');
    project2consideredMuts[projectID];
    while (std::getline(ss, tmp, ' '))
      project2consideredMuts[projectID].insert(std::stoul(tmp));
  }
}

void loadDescription(
    std::string inputDescriptionFile,
    std::vector<std::tuple<std::string, std::string, std::string>> &programTrainSets) {
  std::ifstream csvin(inputDescriptionFile);
  std::string line;
  std::getline(csvin, line); // read the header (projectID,X,Y)
  std::string projectID, Xfilename, Yfilename;
  while (std::getline(csvin, line)) {
    std::istringstream ss(line);
    std::getline(ss, projectID, ',');
    std::getline(ss, Xfilename, ',');
    std::getline(ss, Yfilename, ',');
    programTrainSets.emplace_back(projectID, Xfilename, Yfilename);
  }
}

void readXY(std::string const &fileX, std::string const &fileY,
            std::unordered_map<std::string, std::vector<float>> &matrixX,
            std::vector<bool> &vectorY, std::vector<float> &weights,
            std::vector<bool> &isSubsumingVector, 
            std::vector<float> &killedRatioVector,
            std::vector<MutantIDType> &mutantIDList,
            std::unordered_set<MutantIDType> const *consideredMutants) {
  // read Y and weights, subsumption and killratio
  std::ifstream listin(fileY);
  std::string line;
  std::getline(listin, line); // read the header and discard
  int isCoupled;
  float weight;
  MutantIDType mutID;
  mutID = 0;
  while (std::getline(listin, line)) {
    ++mutID;
    if (consideredMutants && consideredMutants->count(mutID) == 0)
      continue;
    mutantIDList.push_back(mutID);
    std::string tmp;
    std::istringstream ssin(line);
    std::getline(ssin, tmp, ',');
    vectorY.push_back(std::stoul(tmp) != 0);
    std::getline(ssin, tmp, ',');
    weights.push_back(std::stof(tmp));
    std::getline(ssin, tmp, ',');
    isSubsumingVector.push_back(std::stoul(tmp) != 0);
    std::getline(ssin, tmp, ',');
    killedRatioVector.push_back(std::stof(tmp));
  }

  std::ifstream csvin(fileX);
  std::getline(csvin, line); // read the header
  std::istringstream ssh(line);
  std::vector<std::string> features;
  unsigned long nFeatures = 0;
  while (ssh.good()) {
    std::string substr;
    std::getline(ssh, substr, ',');
    features.push_back(substr);
    matrixX[substr].reserve(vectorY.size());
    ++nFeatures;
  }

  float featurevalue;
  mutID = 0;
  while (std::getline(csvin, line)) {
    ++mutID;
    if (consideredMutants && consideredMutants->count(mutID) == 0)
      continue;
    std::istringstream ss(line);
    unsigned long index = 0;
    while (ss.good()) {
      std::string substr;
      std::getline(ss, substr, ',');
      matrixX.at(features[index]).push_back(std::stof(substr));
      ++index;
    }
  }

  if (vectorY.size() != matrixX.begin()->second.size()) {
    llvm::errs() << "\n" << fileX << " " << fileY << "\n";
    assert(vectorY.size() == matrixX.begin()->second.size());
  }
}

void merge2into1(
    std::unordered_map<std::string, std::vector<float>> &matrixX1,
    std::vector<bool> &vectorY1, std::vector<float> &weights1, 
    std::vector<bool> &IsSubsumingVector1, std::vector<float> &KilledRatioVector1, 
    std::vector<MutantIDType> &mutantIDs1,
    std::unordered_map<std::string, std::vector<float>> const &matrixX2,
    std::vector<bool> const &vectorY2, std::vector<float> &weights2, 
    std::vector<bool> &IsSubsumingVector2, std::vector<float> &KilledRatioVector2, 
    std::vector<MutantIDType> &mutantIDs2, std::string const &size) {
  auto curTotNMuts = vectorY1.size();
  std::vector<MutantIDType> eventsIndices(vectorY2.size(), 0);
  for (MutantIDType v = 0, ve = vectorY2.size(); v < ve; ++v)
    eventsIndices[v] = v;
  std::srand(std::time(NULL) + clock()); //+ clock() because fast running
  std::random_shuffle(eventsIndices.begin(), eventsIndices.end());

  MutantIDType num_muts = eventsIndices.size();
  if (size.back() == '%') {
    num_muts = (num_muts *
                std::min((MutantIDType)100, (MutantIDType)std::stoul(size))) /
               100;
  } else {
    num_muts = std::min((MutantIDType)num_muts, (MutantIDType)std::stoul(size));
  }
  num_muts = std::max(num_muts, (MutantIDType)1);
  eventsIndices.resize(num_muts);

  auto missing = NaN;
  // equilibrate by adding missing features into matrixX1
  for (auto &feat_It : matrixX2)
    if (matrixX1.count(feat_It.first) == 0) {
      matrixX1[feat_It.first].reserve(num_muts + curTotNMuts);
      matrixX1[feat_It.first].resize(
          curTotNMuts, missing); // put nan for those without the feature
    }
  for (auto &feat_It : matrixX1) {
    for (auto indx : eventsIndices) {
      if (matrixX2.count(feat_It.first) > 0)
        feat_It.second.push_back(matrixX2.at(feat_It.first)[indx]);
      else
        feat_It.second.push_back(missing);
    }
  }
  // add Y's
  for (auto indx : eventsIndices) {
    vectorY1.push_back(vectorY2[indx]);
    weights1.push_back(weights2[indx]);
    IsSubsumingVector1.push_back(IsSubsumingVector2[indx]);
    KilledRatioVector1.push_back(KilledRatioVector2[indx]);
    mutantIDs1.push_back(mutantIDs2[indx]);
  }
}

void writeModelInfos (std::vector<std::string> &selectedProjectIDs, std::map<unsigned long, double> const &featuresScores, std::vector<std::string> &featuresnames, std::string modelInfosFilename) {
  assert (featuresScores.size() > 0 && "Error: Problem in ML computation (maybe failed: check its log above)");
  //if (featuresScores.size() <= 0)
  //    llvm::errs() << "WARNING: Problem in ML computation (maybe failed: check its log above). No feature importance\n";
  std::fstream out_stream(modelInfosFilename, std::ios_base::out | std::ios_base::trunc);
  out_stream << "{\n";
  out_stream << "\"training-projects\": [";
  bool notfirst = false;
  for (auto &pID: selectedProjectIDs) {
    if (notfirst)
      out_stream << ",";
    out_stream << "\"" << pID << "\"";
    notfirst = true;
  }
  out_stream << "\t],\n";
  out_stream << "\"features-scores\": {";
  notfirst = false;
  for (unsigned long fpos = 0; fpos < featuresnames.size() ; ++fpos) {
    if (notfirst)
      out_stream << ",";
    out_stream << "\"" << featuresnames[fpos] << "\": ";
    if (featuresScores.count(fpos) > 0) 
      out_stream << featuresScores.at(fpos) << "\n";
    else
      out_stream << -1.0 << "\n";  //Not present in training set
    notfirst = true;
  }
  out_stream << "\t}\n";
  out_stream << "}\n";
  out_stream.close();
}

void checkPredictionScore(std::vector<bool> const &Yvector, std::vector<float> const &scores) {
  float sum = 0;
  unsigned ntruecoupl, numcorrect, couplecorrect, npredcoupl;
  numcorrect = couplecorrect = npredcoupl = ntruecoupl = 0;
  for (unsigned long i = 0; i < scores.size(); ++i) {
    if (Yvector[i] - scores[i] < 0.5 && Yvector[i] - scores[i] > -0.5)
      numcorrect++;
    if (scores[i] > 0.5)
      npredcoupl++;
    if (Yvector[i]) {
      ntruecoupl++;
      if (Yvector[i] - scores[i] < 0.5 && Yvector[i] - scores[i] > -0.5)
        couplecorrect++;
    }
    sum += (static_cast<int>(Yvector[i]) - scores[i]) *
           (static_cast<int>(Yvector[i]) - scores[i]);
  }
  llvm::errs() << "SUM: " << sum
               << ", Acc: " << numcorrect * 100.0 / Yvector.size()
               << ", Coupled Precision:" << 100.0 * couplecorrect / npredcoupl
               << ", Coupled Recall: " << 100.0 * couplecorrect / ntruecoupl
               << "\n";

  // Accuracy of random
  std::vector<size_t> yindex(Yvector.size());
  for (size_t i=0; i< yindex.size(); ++i)
    yindex[i] = i;
  float rrecall=0.0, rprecision=0.0;
  for (auto i=1; i <= 100; ++i) {
    //randomly sample npredcoupl element and compute precision and recall
    std::srand(std::time(NULL) + clock()); //+ clock() because fast running
    std::random_shuffle(yindex.begin(), yindex.end());
    size_t ncouple = 0;
    for (auto j=1; j <= npredcoupl; ++j)
      if(Yvector[yindex[j]])
        ++ncouple;
    rprecision += 100.0 * ncouple / npredcoupl;
    rrecall += 100.0 * ncouple / ntruecoupl;
  }
  llvm::errs() << "\n--- RANDOM: \n" 
               << ", Coupled Precision:" << rprecision / 100
               << ", Coupled Recall: " << rrecall / 100
               << "\n";

}

int main(int argc, char **argv) {

// Remove the option we don't want to display in help
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
  llvm::StringMap<llvm::cl::Option *> optMap;
  llvm::cl::getRegisteredOptions(optMap);
#else
  llvm::StringMap<llvm::cl::Option *> &optMap =
      llvm::cl::getRegisteredOptions();
#endif
  for (auto &option : optMap) {
    auto optstr = option.getKey();
    if (!(optstr.startswith("help") || optstr.equals("version")))
      optMap[optstr]->setHiddenFlag(llvm::cl::Hidden);
  }

  llvm::cl::opt<std::string> inputDescriptionFile(
      llvm::cl::Positional, llvm::cl::Required,
      llvm::cl::desc("<Input Description file or X,Y>"));
  llvm::cl::extrahelp("\nADDITIONAL HELP ON INPUT:\n\n  Specify the filepath "
                      "in CSV(header free) or JSON array of arrays, of tree "
                      "string elements: 1st = programID, 2nd = traning X "
                      "matrix, 3rd = training Y vector. \n It is also "
                      "possible, for single matrices X and Y to specify their "
                      "file path separated by comma as following: 'X,Y'\n");
  llvm::cl::value_desc("filename or comma separated filenames");
  llvm::cl::opt<std::string> outputModelFilename(
      "o", llvm::cl::Required,
      llvm::cl::desc(
          "(Required) Specify the filepath to store the trained model"),
      llvm::cl::value_desc("model filename"));
  llvm::cl::opt<std::string> trainingSetEventSize(
      "train-set-event-size",
      llvm::cl::desc("(optional) Specify the the size of events in the input "
                     "data for each program. Append '%' to the number to have "
                     "percentage."),
      llvm::cl::init("100%"));
  llvm::cl::opt<std::string> trainingSetProgramSize(
      "train-set-program-size",
      llvm::cl::desc("(optional) Specify the the size of the training set "
                     "w.r.t the input data program number. Append '%' to the "
                     "number to have percentage. If this is not 100%, the "
                     "algorithm with try to select the random set that cover "
                     "the maximum number of features for the specified size"),
      llvm::cl::init("100%"));
  llvm::cl::opt<unsigned> treesNumber(
      "trees-number",
      llvm::cl::desc(
          "(optional) Specify the number of trees in tree based classifier "
          ".default is 1000 trees"),
      llvm::cl::init(1000));
  llvm::cl::opt<unsigned> treesDepth(
      "trees-depth",
      llvm::cl::desc(
          "(optional) Specify the depth of the trees in boosted tree based classifier "
          ".default is 3 trees"),
      llvm::cl::init(3));
  llvm::cl::opt<bool> prioritiseHardToFindFault(
      "hard-to-find-first",
      llvm::cl::desc("(optional) enable prioritising hard to find fault for training"));
  llvm::cl::opt<bool> onlyAstAndMutantType(
      "only-astparent-and-mutanttype",
      llvm::cl::desc("(optional) enable using only AST parent and Mutant type features (ISSTA-2017 paper)"));
  llvm::cl::opt<bool> onlyMutantType(
      "only-mutanttype",
      llvm::cl::desc("(optional) enable using only Mutant type features"));
  llvm::cl::opt<bool> defectPrediction(
      "defect-prediction-mode",
      llvm::cl::desc("(optional) enable using defect prediction"));
  llvm::cl::opt<double> overSamplingWeightThreshold(
      "oversampling-weight-threshold",
      llvm::cl::desc(
          "(optional) Specify the minimum weight to consider for positive for oversampling."
          " This helps to equilibrate the number of positive and negative data for training."
          " Take effect only if oversampling-ratio is set to non zero positive real number."),
      llvm::cl::init(1.0));
  llvm::cl::opt<double> overSamplingRatio(
      "oversampling-ratio",
      llvm::cl::desc(
          "(optional) Specify the minimum ratio of positive over negative. oversample to reach it."),
      llvm::cl::init(0.0));
  llvm::cl::opt<bool> forEquivalent(
      "for-equivalent",
      llvm::cl::desc("(optional) enable training for mode to select equivalent mutants"));
  llvm::cl::opt<bool> forSubsuming(
      "for-subsuming",
      llvm::cl::desc("(optional) enable training for mode to select subsuming mutants"));
  llvm::cl::opt<bool> forHardToKill(
      "for-hardtokill",
      llvm::cl::desc("(optional) enable training for mode to select hard to kill mutants"));
  llvm::cl::opt<float> hardToKill_Threshold(
      "hardtokillthreshold",
      llvm::cl::desc("(optional) set the max killed ratio threshold for hard to kill mutant. value in interval ]0.0,1.0]"),
      llvm::cl::init(0.05));
  llvm::cl::opt<bool> noRandom(
      "no-random",
      llvm::cl::desc("(optional) Do not randomize the specified project, use from the top downward"));
  llvm::cl::opt<std::string> mapOfMutantsToConsiderPerProject( 
      "considered-mutants",
      llvm::cl::desc(
          "(Required) Specify the filepath of the file containing the mutants to use for training. each row has: <projectID> <list of mutants>"));

  llvm::cl::SetVersionPrinter(printVersion);

  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Mart Mutant Selection Training");

  std::vector<std::tuple<std::string, std::string, std::string>> programTrainSets;

  // Make sure that only one of equivalent, subsuming and hard to kill is used
  if (int(forEquivalent)+int(forSubsuming)+int(forHardToKill) > 1) {
    llvm::errs() << "error: should enable only one of equivalent, subsuming and hard to kill";
    assert (false);
    exit(1);
  }

  std::string Xfilename;
  std::string Yfilename;
  auto commapos = inputDescriptionFile.find(',');
  if (commapos == std::string::npos) {
    loadDescription(inputDescriptionFile, programTrainSets);
  } else {
    // only one set
    std::string Xfilename = inputDescriptionFile.substr(0, commapos);
    std::string Yfilename =
        inputDescriptionFile.substr(commapos + 1, std::string::npos);
    programTrainSets.emplace_back(std::string(""), Xfilename, Yfilename);
  }

  llvm::outs() << "# Specified " << programTrainSets.size()
               << " projects sets in description.\n";

  /// Construct the data for the prediction
  // get the actual training size
  unsigned long num_programs = programTrainSets.size();
  if (trainingSetProgramSize.back() == '%') {
    num_programs =
        (num_programs *
         std::min((unsigned long)100, std::stoul(trainingSetProgramSize))) /
        100;
  } else {
    num_programs = std::min(num_programs, std::stoul(trainingSetProgramSize));
  }
  num_programs = std::max(num_programs, (unsigned long)1);

  // TODO choose the programs randomly and such as to cover the maximum number
  // of features
  // mainly mutantTypename and stmtBBTypename
  std::vector<unsigned> selectedPrograms;
  if (num_programs < programTrainSets.size()) {
    /// For now we just randomly pick some projects and mutants
    for (unsigned pos = 0; pos < programTrainSets.size(); ++pos)
      selectedPrograms.push_back(pos);
    if (prioritiseHardToFindFault) {
      if (!noRandom) {
        std::srand(std::time(NULL) + clock()); //+ clock() because fast running
        std::random_shuffle(selectedPrograms.begin(), selectedPrograms.begin()+num_programs);
        std::random_shuffle(selectedPrograms.begin()+num_programs, selectedPrograms.end());
      }
      auto hardStopAt = (size_t)(num_programs * ((double)num_programs / programTrainSets.size()));
      size_t nToPad = num_programs - hardStopAt;
      for (auto i=1; i < nToPad; ++i)
        selectedPrograms[i+hardStopAt] = selectedPrograms[num_programs+i-1];
    } else {
      if (!noRandom) {
        std::srand(std::time(NULL) + clock()); //+ clock() because fast running
        std::random_shuffle(selectedPrograms.begin(), selectedPrograms.end());
      }
    }
    selectedPrograms.resize(num_programs);
  } else {
    for (unsigned pos = 0; pos < programTrainSets.size(); ++pos)
      selectedPrograms.push_back(pos);
  }

  std::vector<std::vector<float>> Xmatrix;
  std::vector<bool> Yvector;
  std::vector<float> Weightsvector;
  std::vector<bool> IsSubsumingVector;
  std::vector<float> KilledRatioVector;
  std::vector<MutantIDType> MutantsIDvector;

  std::unordered_map<std::string, std::vector<float>> Xmapmatrix;
  std::unordered_map<std::string, std::vector<float>> tmpXmapmatrix;
  std::vector<bool> tmpYvector;
  std::vector<float> tmpWeightsvector;
  std::vector<bool> tmpIsSubsumingVector;
  std::vector<float> tmpKilledRatioVector;
  std::vector<MutantIDType> tmpMutantsIDvector;
  std::vector<std::string> featuresnames;

  llvm::outs() << "# Loading CSVs for " << selectedPrograms.size()
               << " programs ...\n";

  
  std::unordered_map<std::string, std::unordered_set<MutantIDType>> project2consideredMuts;
  if (! mapOfMutantsToConsiderPerProject.empty()) {
    loadMapOfConsideredMutants(mapOfMutantsToConsiderPerProject, 
                                project2consideredMuts);
  }

  std::vector<std::string> selectedProjectIDs;
  std::vector<std::string> projectIDPerRow;
  // for (auto &pair: programTrainSets) {
  for (auto posindex : selectedPrograms) {
    auto &triple = programTrainSets.at(posindex);
    tmpXmapmatrix.clear();
    tmpYvector.clear();
    tmpWeightsvector.clear();
    tmpIsSubsumingVector.clear();
    tmpKilledRatioVector.clear();
    tmpMutantsIDvector.clear();

    // time costly
    selectedProjectIDs.push_back(std::get<0>(triple));
    if (mapOfMutantsToConsiderPerProject.empty()) {
      readXY(std::get<1>(triple), std::get<2>(triple), tmpXmapmatrix, tmpYvector, tmpWeightsvector,
              tmpIsSubsumingVector, tmpKilledRatioVector,
              tmpMutantsIDvector, nullptr);
    } else {
      readXY(std::get<1>(triple), std::get<2>(triple), tmpXmapmatrix, tmpYvector, tmpWeightsvector,
              tmpIsSubsumingVector, tmpKilledRatioVector,
              tmpMutantsIDvector, &(project2consideredMuts[std::get<0>(triple)]));
    }

    if (tmpYvector.size() > 0) {
      projectIDPerRow.resize(projectIDPerRow.size() + tmpYvector.size(), std::get<0>(triple));
      merge2into1(Xmapmatrix, Yvector, Weightsvector, IsSubsumingVector, KilledRatioVector, 
                  MutantsIDvector, tmpXmapmatrix, tmpYvector, tmpWeightsvector,
                  tmpIsSubsumingVector, tmpKilledRatioVector, tmpMutantsIDvector, trainingSetEventSize);
    }
  }

  project2consideredMuts.clear();

  llvm::outs() << "# CSVs Loaded. Preparing training data ...\n";

  // apply feature selection if specified
  if (defectPrediction) {
    assert (!(onlyAstAndMutantType || onlyMutantType) && "Must not enable ISSTA 2017 nor mutant type on defect prediction mode");
  } else {
    assert (!(onlyAstAndMutantType && onlyMutantType) && "Must not enable ISSTA 2017 and mutant type only mode at the same time");
    if (onlyAstAndMutantType || onlyMutantType) {
      // remove other features like: depth, complexity, data/ctrl dependency
      std::vector<std::string> fToRemove;
      for (auto &it : Xmapmatrix) {
        llvm::StringRef tmp(it.first);
        if (!tmp.endswith("-Matcher") && !tmp.endswith("-Replacer"))
          if (onlyMutantType || (!tmp.endswith("-BBType") && !tmp.endswith("-ASTp") && !tmp.endswith("-ChildContext") && !tmp.endswith("-DataTypeContext"))) 
            fToRemove.push_back(it.first);
      }
      for (auto &str: fToRemove)
        Xmapmatrix.erase(str);
    } else {
      // Remove feature not needed (relative's BBType fog now)
      std::vector<std::string> fToRemove;
      for (auto &it : Xmapmatrix) {
        llvm::StringRef tmp(it.first);
        
#if 0
        // Top 6 features only (IG >= 0.02)
        if (!tmp.equals("Complexity") && !tmp.equals("NumInCtrlDeps") && !tmp.equals("NumOutDataDeps") && !tmp.equals("NumInDataDeps") && !tmp.equals("NumTieDeps") && !tmp.equals("CfgDepth"))
          fToRemove.push_back(it.first);
#endif
        // No stmtBB of relative
        if (tmp.endswith("-BBType-astparent") || tmp.endswith("-BBType-outdatadep") || tmp.endswith("-BBType-indatadep") || tmp.endswith("-BBType-outctrldep") || tmp.endswith("-BBType-inctrldep")) 
          fToRemove.push_back(it.first);
      }
      for (auto &str: fToRemove)
        Xmapmatrix.erase(str);
    }
  }

  // convert Xmapmatrix to Xmatrix
  for (auto &it : Xmapmatrix) {
    Xmatrix.push_back(it.second);
    featuresnames.push_back(it.first);
  }

  // verify data
  assert(!Yvector.empty() && !Xmatrix.empty() && Weightsvector.size() == Yvector.size() &&
         "mart-training@error: training data cannot be empty");
  for (auto &featVect : Xmatrix)
    assert(featVect.size() == Yvector.size() && "mart-training@error: all "
                                                "feature vector in X must have "
                                                "same size with Y");

#if 0
  ///DBG : XXX This is used to get all projects data in a matrix, for use for example with python
  assert (projectIDPerRow.size() == Weightsvector.size() && "projectIDPerRow size don't match number of rows");
  std::fstream out_all(outputModelFilename+".svmdata.tmp", std::ios_base::out | std::ios_base::trunc);
  out_all << "projectID,";
  out_all << "mutantID,";
  for (auto feat: featuresnames)
    out_all << feat << ",";
  out_all << "coupling-weight,is-subsuming,killed-ratio" << "\n";
  for (auto i=0; i < Weightsvector.size(); ++i) {
    out_all << projectIDPerRow[i] << ",";
    out_all << MutantsIDvector[i] << ",";
    for (auto &fv:Xmatrix)
      out_all << fv[i] << ",";
    out_all << Weightsvector[i] << ",";
    out_all << IsSubsumingVector[i] << ",";
    out_all << KilledRatioVector[i] << "\n";
  }
  out_all.close();
  llvm::outs() << "## merged training data saved into " << outputModelFilename+".svmdata.tmp" << "\n";
  return 0;
  ///DBG ~
#endif

  // XXX Handle unkilled mutants (weigh is -1) and tune weights
  if (forEquivalent) {
    for (auto i=0; i < Yvector.size(); ++i) {
      Yvector[i] = (Weightsvector[i] == -1.0);
      Weightsvector[i] = 1.0;  //all have same weight for simplicity
    }
  } else if (forSubsuming) {
    for (auto i=0; i < Yvector.size(); ++i) {
      Yvector[i] = IsSubsumingVector[i];
      Weightsvector[i] = 1.0;  //all have same weight for simplicity
    }
  } else  if (forHardToKill) {
    for (auto i=0; i < Yvector.size(); ++i) {
      Yvector[i] = (KilledRatioVector[i] > 0 && KilledRatioVector[i] <= hardToKill_Threshold);
      Weightsvector[i] = 1.0;  //all have same weight for simplicity
    }
  } else {
    for (auto i=0; i < Weightsvector.size(); ++i) {
      //treat equivalent mutants as killed by no failing test
      if (Weightsvector[i] < 0.0)    //== -1
        Weightsvector[i] = 1.0; 
      else if (Weightsvector[i] == 0.0)
        Weightsvector[i] = 0.99; //a bit better than equivalent (since killed) 
      else
        if (! Yvector[i])
          Weightsvector[i] = 1 - Weightsvector[i];
      // Consider mutant at least killed once by failing test as a bit good...
      // but with small weight
       //Yvector[i] = true;   //the weight stay the same
    }
  }

  // Oversampling if enabled
  if (overSamplingRatio > 0.0) {
    assert (overSamplingRatio <= 1.0 && "Invalid overSamplingRatio passed");
    assert (overSamplingWeightThreshold <= 1.0 && overSamplingWeightThreshold >= 0.0 && "Invalid overSamplingWeightThreshold passed");
    llvm::outs() << "## Oversampling: Ratio is " << overSamplingRatio << ", threshold is " << overSamplingWeightThreshold << "\n";
    std::vector<size_t> nonAbsPositive;
    std::vector<size_t> threshPositive;
    size_t nThreshNegative = 0;
    for (auto i = 0; i < Yvector.size(); ++i) {
      if (Yvector[i]) {
        threshPositive.push_back(i);
      } else {
        auto invweigththresh = 1 - overSamplingWeightThreshold;
        if (Weightsvector[i] <= invweigththresh)
          nonAbsPositive.push_back(i);
        else
          ++nThreshNegative;
      }
    }
    threshPositive.insert(threshPositive.end(), nonAbsPositive.begin(), nonAbsPositive.end());
    nonAbsPositive.clear();
    size_t nFinalPositive = nThreshNegative * overSamplingRatio;
    if (nFinalPositive > threshPositive.size()) {
      // do oversampling
      auto extra = nFinalPositive - threshPositive.size();
      llvm::outs() << "### Oversampling is adding " << extra << " rows!\n";
      // - compute the number of repetition of each positive
      size_t nRepPerPos = extra / threshPositive.size();
      size_t offset = extra % threshPositive.size();
      // - actually copy
      Yvector.reserve(Yvector.size() + extra);
      Weightsvector.reserve(Weightsvector.size() + extra);
      for (auto &featVect : Xmatrix)
        featVect.reserve(featVect.size() + extra);

      size_t index;
      for (index = 0; index < threshPositive.size(); ++index) {
        auto reps = nRepPerPos;
        if (offset > 0) {
          ++reps;
          --offset;
        }
        for (auto i = 0; i < reps; ++i) {
          auto pos = threshPositive[index];
          Yvector.push_back(Yvector[pos]);
          Weightsvector.push_back(Weightsvector[pos]);
          for (auto &featVect : Xmatrix)
            featVect.push_back(featVect[pos]);
        }
      }
    }
  }

  llvm::outs() << "# X Matrix and Y Vector ready. Training ...\n";

  PredictionModule predmod(outputModelFilename);
  std::map<unsigned long, double> featuresScores = predmod.train(Xmatrix, featuresnames, Yvector, Weightsvector, treesNumber, treesDepth);

  // Get features relevance weights
  std::string modelInfosFilename(outputModelFilename+".infos.json");
  writeModelInfos (selectedProjectIDs, featuresScores, featuresnames, modelInfosFilename);
  

  // Check prediction score
  std::vector<float> scores;
  predmod.predict(Xmatrix, featuresnames, scores);
  checkPredictionScore (Yvector, scores);
  
  std::cout << "\n# Training completed, model written to file "
            << outputModelFilename << "\n\n";
  return 0;
}
