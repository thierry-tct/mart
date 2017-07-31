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

#include <ctime>
#include <fstream>
#include <string>
#include <algorithm>
#include <iostream>

#include "../lib/mutantsSelection/MutantSelection.h"  //for PredictionModule

#include "llvm/Support/CommandLine.h" //llvm::cl

using namespace mart;
using namespace mart::selection;

#define TOOLNAME "Mart-Training"
#include "tools_commondefs.h"

void loadDescription (std::string inputDescriptionFile, std::vector<std::pair<std::string, std::string>> &programTrainSets) {
  std::ifstream csvin(inputDescriptionFile);
  std::string line;
  std::getline(csvin, line);  //read the header (projectID,X,Y)
  std::string projectID, Xfilename, Yfilename;
  while(std::getline(csvin, line))
  {
    std::istringstream ss(line);
    std::getline( ss, projectID, ',' );
    std::getline( ss, Xfilename, ',' );
    std::getline( ss, Yfilename, ',' );
    programTrainSets.emplace_back(Xfilename, Yfilename);
  }
}

void readXY (std::string fileX, std::string fileY, std::unordered_map<std::string, std::vector<float>> matrixX, std::vector<bool> vectorY) {
  //read Y
  std::ifstream listin(fileY);
  std::string line;
  std::getline(listin, line);  //read the headeer and discard
  int isCoupled;
  while(std::getline(listin, line)) {
    std::istringstream ssin(line);
    ssin >> isCoupled;
    vectorY.push_back(isCoupled != 0);
  }
  
  std::ifstream csvin(fileX);
  std::getline(csvin, line);  //read the header
  std::istringstream ssh(line);
  std::vector<std::string> features;
  unsigned long nFeatures = 0;
  while( ssh.good() )
  {
      std::string substr;
      std::getline( ssh, substr, ',' );
      features.push_back( substr );
      matrixX[substr].reserve(vectorY.size());
      ++nFeatures;
  }
    
  float featurevalue;
  while(std::getline(csvin, line)) {
    std::istringstream ss(line);
    unsigned long index = 0;
    while( ss.good() )
    {
        std::string substr;
        std::getline( ss, substr, ',' );
        matrixX.at(features[index]).push_back( std::stof(substr) );
        ++index;
    }
  }
  
  assert (vectorY.size() == matrixX.begin()->second.size());
}

void merge2into1(std::unordered_map<std::string, std::vector<float>> matrixX1, std::vector<bool> vectorY1, std::unordered_map<std::string, std::vector<float>> matrixX2, std::vector<bool> vectorY2, std::string size) {
  auto curTotNMuts = vectorY1.size();
  std::vector<MutantIDType> eventsIndices(vectorY2.size(), 0);
  for (MutantIDType v=0, ve = vectorY2.size(); v < ve; ++v)
    eventsIndices[v] = v;
  std::srand(std::time(NULL) + clock()); //+ clock() because fast running
  std::random_shuffle(eventsIndices.begin(), eventsIndices.end());
  
  MutantIDType num_muts = eventsIndices.size();
  if (size.back() == '%') {
    num_muts *= std::min((MutantIDType)100, (MutantIDType)std::stoul(size)) / 100;
  } else {
    num_muts = std::min((MutantIDType)num_muts, (MutantIDType)std::stoul(size));
  }
  num_muts = std::max(num_muts, (MutantIDType)1);
  eventsIndices.resize(num_muts);
  
  auto missing = 0.0/0.0;
  //equilibrate by adding missing features into matrixX1  
  for (auto &feat_It: matrixX2) 
    if (matrixX1.count(feat_It.first) == 0){
      matrixX1[feat_It.first].reserve(num_muts+curTotNMuts);
      matrixX1[feat_It.first].resize(curTotNMuts, missing); //put nan for those without the feature
    }
  for (auto &feat_It: matrixX1) {
    for (auto indx: eventsIndices) {
      if (matrixX2.count(feat_It.first) > 0)
        feat_It.second.push_back(matrixX2.at(feat_It.first)[indx]);
      else
        feat_It.second.push_back(missing);
    }
  }
  // add Y's
  for (auto indx: eventsIndices) {
    vectorY1.push_back(vectorY2[indx]);
  }
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
  llvm::cl::extrahelp("\nADDITIONAL HELP ON INPUT:\n\n  Specify the filepath in CSV(header free) or JSON array of arrays, of tree string elements: 1st = programID, 2nd = traning X matrix, 3rd = training Y vector. \n It is also possible, for single matrices X and Y to specify their file path separated by comma as following: 'X,Y'\n");
      llvm::cl::value_desc("filename or comma separated filenames");
  llvm::cl::opt<std::string> outputModelFilename(
      "o", llvm::cl::Required,
      llvm::cl::desc("(Required) Specify the filepath to store the trained model"),      
      llvm::cl::value_desc("model filename"));
  llvm::cl::opt<std::string> trainingSetEventSize(
      "train-set-event-size",
      llvm::cl::desc("(optional) Specify the the size of events in the input data for each program. Append '%' to the number to have percentage."),
      llvm::cl::init("100%"));
  llvm::cl::opt<std::string> trainingSetProgramSize(
      "train-set-program-size",
      llvm::cl::desc("(optional) Specify the the size of the training set w.r.t the input data program number. Append '%' to the number to have percentage. If this is not 100%, the algorithm with try to select the random set that cover the maximum number of features for the specified size"),
      llvm::cl::init("100%"));


  llvm::cl::SetVersionPrinter(printVersion);

  llvm::cl::ParseCommandLineOptions(argc, argv, "Mart Mutant Selection Training");

  std::vector<std::pair<std::string, std::string>> programTrainSets; 
  
  std::string Xfilename;
  std::string Yfilename;
  auto commapos = inputDescriptionFile.find(',');
  if (commapos == std::string::npos) {
    loadDescription (inputDescriptionFile, programTrainSets);
  } else {
    //only one set
    std::string Xfilename = inputDescriptionFile.substr(0, commapos);
    std::string Yfilename = inputDescriptionFile.substr(commapos+1, std::string::npos);
    programTrainSets.emplace_back(Xfilename, Yfilename);
  }

  /// Construct the data for the prediction
  // get the actual training size
  unsigned long num_programs = programTrainSets.size();
  if (trainingSetProgramSize.back() == '%') {
    num_programs *= std::min((unsigned long)100, std::stoul(trainingSetProgramSize)) / 100;
  } else {
    num_programs = std::min(num_programs, std::stoul(trainingSetProgramSize));
  }
  num_programs = std::max(num_programs, (unsigned long)1);

  // TODO choose the programs randomly and such as to cover the maximum number of features
  // mainly mutantTypename and stmtBBTypename
  std::vector<unsigned> selectedPrograms;
  if (num_programs < programTrainSets.size()) {
    ///For now we just randomly pick some projects and mutants
    for (unsigned pos=0; pos < programTrainSets.size(); ++pos)
      selectedPrograms.push_back(pos);
    std::srand(std::time(NULL) + clock()); //+ clock() because fast running
    std::random_shuffle(selectedPrograms.begin(), selectedPrograms.end());
    selectedPrograms.resize(num_programs);
  } else {
    for (unsigned pos=0; pos < programTrainSets.size(); ++pos)
      selectedPrograms.push_back(pos);
  }

  // missing value (features related to one hot encoding) are filled with 0
  //TODO
  std::vector<std::vector<float>> Xmatrix;
  std::vector<bool> Yvector;
  
  std::unordered_map<std::string, std::vector<float>> Xmapmatrix;
  std::unordered_map<std::string, std::vector<float>> tmpXmapmatrix;
  std::vector<bool> tmpYvector;

  for (auto &pair: programTrainSets) {
    readXY (pair.first, pair.second, tmpXmapmatrix, tmpYvector);
    merge2into1(Xmapmatrix, Yvector, tmpXmapmatrix, tmpYvector, trainingSetEventSize);
  }
  
  // convert Xmapmatrix to Xmatrix
  for (auto &it: Xmapmatrix) {
    Xmatrix.push_back(it.second);
  }
  
  PredictionModule predmod(outputModelFilename);
  predmod.train(Xmatrix, Yvector);

  std::cout << "\n# Training completed, model written to file " << outputModelFilename << "\n\n";
  return 0;
}

