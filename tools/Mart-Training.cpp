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
  cl::extrahelp("\nADDITIONAL HELP ON INPUT:\n\n  Specify the filepath in CSV(header free) or JSON array of arrays, of tree string elements: 1st = programID, 2nd = traning X matrix, 3rd = training Y vector. \n It is also possible, for single matrices X and Y to specify their file path separated by comma as following: 'X,Y'\n");
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
    // open the JSON or CSV file to read the different training sets

    // read the different data into programTrainSets

  } else {
    //only one set
    Xfilename = inputDescriptionFile.substr(0, commapos);
    Yfilename = inputDescriptionFile.substr(commapos+1, std::string::npos);
    // read the different data into programTrainSets

  }

  /// Construct the data for the prediction
  // get the actual training size
  unsigned num_programs = programTrainSets.size()
  if (trainingSetProgramSize.back() == '%') {
    num_programs *= std::stol(trainingSetProgramSize) / 100;
  } else {
    num_programs = std::min(num_programs, std::stol(trainingSetProgramSize));
  }

  // choose the programs randomly and such as to cover the maximum number of features
  // mainly mutantTypename and stmtBBTypename
  std::vector<unsigned> selectedPrograms;
  if (num_programs < programTrainSets.size()) {
    ///TODO
  } else {
    for (unsigned pos=0; pos < programTrainSets; ++pos)
      selectedPrograms.push_back(pos);
  }

  // missing value (features related to one hot encoding) are filled with 0
  //TODO
  std::vector<std::vector<float>> Xmatrix,
  std::vector<bool> Yvector;

  
  PredictionModule predmod(outputModelFilename);
  predmod.predict(Xmatrix, Yvector);

  std::cout << "\n# Training completed, model written to file " << outputModelFilename << "\n\n";
  return 0;
}

