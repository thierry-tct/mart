/**
 * -==== Mart-Utils.cpp
 *
 *                Mart Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Main source file that will be compiled to execute mutation and
 * possibly selection.
 */

#include <ctime>
#include <fstream>
#include <libgen.h> //basename
#include <sstream>
#include <string>
#include <sys/stat.h>  //mkdir, stat
#include <sys/types.h> //mkdir, stat
#include <sys/wait.h>  //wait
#include <unistd.h>    // fork, execl

#include "../lib/mutation.h"
#include "../lib/typesops.h"
#include "ReadWriteIRObj.h"

#include "llvm/Support/FileSystem.h"       //for llvm::sys::fs::create_link
#include "llvm/Transforms/Utils/Cloning.h" //for CloneModule

#include "FunctionToModule.h"

#include "llvm/Support/CommandLine.h" //llvm::cl

using namespace mart;

#define TOOLNAME "Mart-Utils"
#include "tools_commondefs.h"

//static std::string outputDir("mart-out-");
static const std::string mutantsFolder("mutants.out");

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

  llvm::cl::opt<std::string> inputOriginalBc(llvm::cl::Positional,
                                         llvm::cl::Required,
                                         llvm::cl::desc("<input original preprocessed BC>"));
  llvm::cl::opt<std::string> inputMetaMuBc(llvm::cl::Positional,
                                         llvm::cl::Required,
                                         llvm::cl::desc("<input final meta mutant BC>"));
  llvm::cl::opt<std::string> inexistantOutdir(llvm::cl::Positional,
                                         llvm::cl::Required,
                                         llvm::cl::desc("<inexistant output folder>"));
  llvm::cl::opt<bool> dumpMutantsBcs(
      "write-mutants-bc", llvm::cl::desc("(Optional) Enable writing mutant bc files"));
  llvm::cl::opt<std::string> candidateMutantList(
      "mutant-list-file",
      llvm::cl::desc("(Optional) Specify the file containing the candidate mutant list"),
      llvm::cl::value_desc("filename"), llvm::cl::init(""));

  llvm::cl::SetVersionPrinter(printVersion);

  llvm::cl::ParseCommandLineOptions(argc, argv, "Mart Mutantion");

  time_t totalRunTime = time(NULL);

  /// Load both BC modules
  llvm::Module *moduleOrig, *moduleMeta;
  std::unique_ptr<llvm::Module> _MOrig, _MMeta;
  if (!ReadWriteIRObj::readIR(inputOriginalBc, _MOrig))
    return 1;
  moduleOrig = _MOrig.get();
  if (!ReadWriteIRObj::readIR(inputMetaMuBc, _MMeta))
    return 1;
  moduleMeta = _MMeta.get();
  
  /// Create Mutation object
  std::string useful_conf_dir = getUsefulAbsPath(argv[0]);
  const char *defaultMconfFile = "mconf-scope/default_allmax.mconf";
  std::string mutantConfigfile, mutantScopeJsonfile;
  mutantConfigfile.assign(useful_conf_dir + defaultMconfFile);
  Mutation mut(*moduleOrig, mutantConfigfile, nullptr, mutantScopeJsonfile);
  
  /// Load mutant list
  std::set<MutantIDType> cand_mut_ids;
  if (!candidateMutantList.empty()) {
    std::ifstream ifs(candidateMutantList); 
    if (ifs.is_open()) {
      MutantIDType tmpid;
      ifs >> tmpid;
      while (ifs.good()) {
        cand_mut_ids.insert(tmpid);
        ifs >> tmpid;
      }
      cand_mut_ids.insert(tmpid); // Just in case the user forget the las new line
      ifs.close();
      if (cand_mut_ids.empty()) {
        llvm::errs() << "ERROR: The candidate mutant list passed is empty!\n";
        return 1;
      }
    } else {
      llvm::errs() << "ERROR: Unable to open Mutants Candidate file: " 
                   << candidateMutantList << "\n";
      assert(false);
      return 1;
    }
  } else {
    // get the maximum mutant Id and get all mutants Id in the range
    for (MutantIDType mid=1; mid <= mut.getHighestMutantID(moduleMeta); ++mid)
      cand_mut_ids.insert(mid);
  }
  // Insert original
  cand_mut_ids.insert(0);
  
  /// test and create Output dir
  struct stat st; 
  if (stat(inexistantOutdir.c_str(), &st) != -1) {
    llvm::errs() << "ERROR: Output folder already exists. Specify non existing!\n";
    assert(false);
    return 1;
  }
  if (mkdir(inexistantOutdir.c_str(), 0777) != 0)
    assert(false && "Failed to create output directory");

  
  /// For each mutant: (1) clone meta-Mu module, (2) set to mutant, (3) dump
  std::string outFile;
  char *tmpStr = nullptr;
  tmpStr = new char[1 + inputOriginalBc.length()]; // Alocate tmpStr
  std::strcpy(tmpStr, inputOriginalBc.c_str());
  // basename changes the contain of its parameter
  outFile.assign(basename(tmpStr)); 
  delete[] tmpStr;
  tmpStr = nullptr; // del tmpStr2
  if (!outFile.substr(outFile.length() - 3, 3).compare(".ll") ||
      !outFile.substr(outFile.length() - 3, 3).compare(".bc"))
    outFile.replace(outFile.length() - 3, 3, "");

  for (auto mid: cand_mut_ids) {
    // (1) clone
    #if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
      std::unique_ptr<llvm::Module> mutmodule(llvm::CloneModule(moduleMeta));
    #else
      std::unique_ptr<llvm::Module> mutmodule = llvm::CloneModule(moduleMeta);
    #endif

    // (2) set to mutant
    if (!mut.getMutant(*mutmodule, mid, nullptr)) {
      llvm::errs() << "Error: failed to get mutant " << mid << "\n";
      assert (false && "Failed to getMutant");
      return 1;
    }

    // (3) dump
    if (mkdir((inexistantOutdir + "/" + std::to_string(mid)).c_str(), 0777) != 0)
      assert(false && "Failed to create mutant output directory");
    if (!ReadWriteIRObj::writeIR(mutmodule.get(),
                            inexistantOutdir + "/" + std::to_string(mid) 
				              + "/" + outFile + commonIRSuffix)) {
      assert(false && "Failed to output mutant IR file");
      return 1;
    }
  }

  llvm::outs() << "\nMart-Utils@Progress:  TOTAL RUNTIME: "
               << (difftime(time(NULL), totalRunTime) / 60) << " min.\n";
  llvm::outs() << "\nMArt-Utils@Progress: Done!\n";

  return 0;
}
