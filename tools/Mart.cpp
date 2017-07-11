/**
 * -==== Mart.cpp
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
#include "ReadWriteIRObj.h"

#include "llvm/Support/FileSystem.h"       //for llvm::sys::fs::create_link
#include "llvm/Transforms/Utils/Cloning.h" //for CloneModule

#include "FunctionToModule.h"

#include "llvm/Support/CommandLine.h" //llvm::cl

using namespace mart;

static std::string outputDir("mart-out-");
static const std::string mutantsFolder("mutants.out");
static const std::string mutantsInfosFileName("mutantsInfos.json");
static const std::string generalInfo("info");
static std::stringstream loginfo;
static std::string outFile;
static const std::string tmpFuncModuleFolder("tmp-func-module-dir.tmp");

void insertMutSelectGetenv(llvm::Module *mod) {
  // insert getenv and atol
}

/**
 * \brief print all the modules of mutants, sorted from mutant 0(original) to
 * mutant max
 * XXX This function modifies the values in parameters 'mods' and 'mutFunctions'
 */
bool dumpMutantsCallback(Mutation *mutEng,
                         std::map<unsigned, std::vector<unsigned>> *poss,
                         std::vector<llvm::Module *> *mods,
                         llvm::Module *wmModule /*=nullptr*/,
                         std::vector<llvm::Function *> const *mutFunctions) {
  clock_t curClockTime = clock();
  // weak mutation
  if (wmModule) {
    llvm::outs() << "Mart@Progress: writing weak mutation...\n";
    std::string wmFilePath = outputDir + "/" + outFile + ".WM.bc";
    if (!ReadWriteIRObj::writeIR(wmModule, wmFilePath)) {
      assert(false && "Failed to output weak mutation IR file");
    }
  }

  // Strong Mutants
  if (poss && mods) {
    llvm::outs() << "Mart@Progress: writing mutants to files (" << poss->size()
                 << ")...\n";

    std::unordered_map<llvm::Module *, llvm::Function *> backedFuncsByMods;

    std::string mutantsDir = outputDir + "/" + mutantsFolder;
    if (mkdir(mutantsDir.c_str(), 0777) != 0)
      assert(false && "Failed to create mutants output directory");

    std::string tmpFunctionDir = outputDir + "/" + tmpFuncModuleFolder;

    llvm::Module *formutsModule = mods->at(0);
    bool separateFunctionModule =
        (formutsModule->size() >
         1); // separate only if we have more than one function XXX
    unsigned mid = 0;

    std::string infoFuncPathModPath;
    const std::string infoFuncPathModPath_File(tmpFunctionDir + "/" +
                                               "mapinfo");
    if (separateFunctionModule)
      if (mkdir(tmpFunctionDir.c_str(), 0777) != 0)
        assert(false && "Failed to create function temporal directory");

    // original
    if (mkdir((mutantsDir + "/0").c_str(), 0777) != 0)
      assert(false && "Failed to create output directory for original (0)");
    if (!ReadWriteIRObj::writeIR(formutsModule,
                                 mutantsDir + "/0/" + outFile + ".bc")) {
      assert(false && "Failed to output post-TCE original IR file");
    }
    infoFuncPathModPath += mutantsFolder + "/0/" + outFile + ".bc\n";

    // mutants
    for (auto &m : *poss) {
      formutsModule = mods->at(m.first);
      if (mutFunctions != nullptr) {
        mid = m.second.front();
        llvm::Function *currFunc = mutFunctions->at(m.first);
        std::string funcName = currFunc->getName();
        std::string funcFile = tmpFunctionDir + "/" + funcName + ".bc";
        std::string mutDirPath = mutantsDir + "/" + std::to_string(mid);
        if (mkdir(mutDirPath.c_str(), 0777) != 0)
          assert(false && "Failed to create output directory for mutant");

        if (backedFuncsByMods.count(formutsModule) == 0) {
          llvm::ValueToValueMapTy vmap;
          if (separateFunctionModule) {
            backedFuncsByMods[formutsModule]; // just insert function
            formutsModule->getFunction(funcName)->dropAllReferences();
            std::unique_ptr<llvm::Module> tmpM =
                FunctionToModule::martSplitFunctionsOutOfModule(formutsModule,
                                                                funcName);
            // wrtite the function's IR in tmpFunctionDir
            if (!ReadWriteIRObj::writeIR(tmpM.get(), funcFile)) {
              llvm::errs() << "Function " << funcName << "...\n";
              assert(false && "Failed to output function's module IR file");
            }

            // now 'formutsModule' contain only the function of interest, delete
            // it so that we can insert the mutants later

            if (!formutsModule->getFunction(funcName)
                     ->use_empty()) // The function has recursive call. all the
                                    // mutants point here. Make them point on
                                    // themselves
            {
              auto *tmpFF = formutsModule->getFunction(funcName);
              while (!tmpFF->use_empty()) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                if (llvm::Instruction *URInst =
                        llvm::dyn_cast<llvm::Instruction>(tmpFF->use_back()))
#else
                if (llvm::Instruction *URInst =
                        llvm::dyn_cast<llvm::Instruction>(tmpFF->user_back()))
#endif
                {
                  llvm::Function *fofi = URInst->getParent()->getParent();
                  assert(!fofi->getParent() && "the only users should be the "
                                               "mutant functions here, and "
                                               "thery have no parent.");
                  URInst->replaceUsesOfWith(tmpFF, fofi);
                } else {
                  llvm::errs() << "\nError: Function '" << funcName
                               << "' should have NO use here, since in its own "
                                  "module. But still have "
                               << tmpFF->getNumUses() << " uses\n";
                  llvm::errs() << "> Please report the bug.\n";
                  // llvm::errs() << UR << "\n";
                  // if (auto *Uinst = llvm::dyn_cast<llvm::Instruction>(UR))
                  ////Uinst->getParent()->getParent()->dump();
                  //    llvm::errs() << Uinst->getParent()->getParent() << "" <<
                  //    Uinst->getParent()->getParent()->getName() << " --- " <<
                  //    Uinst->getParent()->getParent()->getParent() << "\n";
                  assert(false);
                }
              }
            }
            formutsModule->getFunction(funcName)->eraseFromParent();
          } else {
            backedFuncsByMods.emplace(
                formutsModule,
                llvm::CloneFunction(formutsModule->getFunction(funcName), vmap,
                                    true /*moduleLevelChanges*/));
          }
        }

        if (separateFunctionModule) {
          infoFuncPathModPath += mutantsFolder + "/" + std::to_string(mid) +
                                 "/" + outFile + ".bc " + tmpFuncModuleFolder +
                                 "/" + funcName + ".bc\n";
          // std::error_code EC = llvm::sys::fs::create_link(funcFile,
          // mutDirPath+"//"+funcName+"_");    //'_' to avaoid clash
          // assert (!EC && "Failed to create link to function for a mutant");

          auto linkageBak = currFunc->getLinkage();
          formutsModule->getFunctionList().push_back(currFunc);
          currFunc->setLinkage(llvm::GlobalValue::ExternalLinkage);
          if (!ReadWriteIRObj::writeIR(formutsModule,
                                       mutDirPath + "/" + outFile + ".bc")) {
            llvm::errs() << "Mutant " << mid << "...\n";
            assert(false && "Failed to output post-TCE mutant IR file");
          }
          currFunc->setLinkage(linkageBak);
          currFunc->removeFromParent();
        } else {
          mutEng->setModFuncToFunction(formutsModule, currFunc);
          if (!ReadWriteIRObj::writeIR(formutsModule,
                                       mutDirPath + "/" + outFile + ".bc")) {
            llvm::errs() << "Mutant " << mid << "...\n";
            assert(false && "Failed to output post-TCE mutant IR file");
          }
        }
      } else {
        mid = m.second.front();
        if (mkdir((mutantsDir + "/" + std::to_string(mid)).c_str(), 0777) != 0)
          assert(false && "Failed to create output directory for mutant");
        if (!ReadWriteIRObj::writeIR(formutsModule,
                                     mutantsDir + "/" + std::to_string(mid) +
                                         "/" + outFile + ".bc")) {
          llvm::errs() << "Mutant " << mid << "...\n";
          assert(false && "Failed to output post-TCE mutant IR file");
        }
      }
    }
    if (mutFunctions != nullptr) {
      if (separateFunctionModule) {
        std::ofstream xxx(infoFuncPathModPath_File);
        if (xxx.is_open()) {
          xxx << infoFuncPathModPath;
          xxx.close();
        } else
          llvm::errs() << "Unable to open file for write:"
                       << infoFuncPathModPath_File;

        // for (auto &itt: extractedFuncMod)
        //    delete itt.second;
      } else { // restore
        for (auto &itt : backedFuncsByMods) {
          mutEng->setModFuncToFunction(itt.first, itt.second);
          delete itt.second; // the fuction just created above as clone..
        }
      }
    }
  }
  llvm::outs() << "Mart@Progress: writing mutants to file took: "
               << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
               << " Seconds.\n";
  loginfo << "Mart@Progress: writing mutants to file took: "
          << (float)(clock() - curClockTime) / CLOCKS_PER_SEC << " Seconds.\n";
  return true;
}

void printVersion() {
  llvm::outs() << "\nMart (Framework for Multi-Programming Language Mutation "
                  "Testing based on LLVM)\n";
  llvm::outs() << "\tMart 1.0\n";
  llvm::outs() << "\nLLVM (http://llvm.org/):\n";
  llvm::outs() << "\tLLVM version " << LLVM_VERSION_MAJOR << "."
               << LLVM_VERSION_MINOR << "." << LLVM_VERSION_PATCH << "\n";
  llvm::outs() << "\tLLVM tools dir: " << LLVM_TOOLS_BINARY_DIR << "\n";
  llvm::outs() << "\n";
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

  llvm::cl::opt<std::string> inputIRfile(llvm::cl::Positional,
                                         llvm::cl::Required,
                                         llvm::cl::desc("<input IR file>"));

  llvm::cl::opt<std::string> mutantConfigfile(
      "mutant-config",
      llvm::cl::desc("(Optional) Specify the mutation operations to apply"),
      llvm::cl::value_desc("filename"), llvm::cl::init(""));

  llvm::cl::opt<std::string> mutantScopeJsonfile(
      "mutant-scope",
      llvm::cl::desc(
          "(Optional) Specify the mutation scope: Functions, source files."),
      llvm::cl::value_desc("filename"), llvm::cl::init(""));

  llvm::cl::opt<bool> dumpPreTCEMeta(
      "print-preTCE-Meta",
      llvm::cl::desc("Enable dumping Meta module before applying TCE"));
  llvm::cl::opt<bool> disableDumpMetaIRbc(
      "no-Meta",
      llvm::cl::desc("Disable dumping Meta Module after applying TCE"));
  llvm::cl::opt<bool> dumpMutants(
      "write-mutants", llvm::cl::desc("Enable writing mutant files"));
  llvm::cl::opt<bool> disabledWeakMutation(
      "no-WM", llvm::cl::desc("Disable dumping Weak Mutatin Module"));
  llvm::cl::opt<bool> disableDumpMutantInfos(
      "no-mutant-info",
      llvm::cl::desc("Disable dumping mutants info JSON file"));

  llvm::cl::opt<bool> keepMutantsBCs(
      "keep-mutants-bc",
      llvm::cl::desc("Keep the different LLVM IR module of all mutants (only "
                     "active when enabled write-mutants)"));

  llvm::cl::SetVersionPrinter(printVersion);

  llvm::cl::ParseCommandLineOptions(argc, argv, "Mart Mutantion");

  time_t totalRunTime = time(NULL);
  clock_t curClockTime;

  const char *wmLogFuncinputIRfileName = "wmlog-driver.bc";

  /// \brief set this to false if the module is small enough, that all mutants
  /// will fit in memory
  bool isTCEFunctionMode = true;

#ifdef MART_SEMU_GENMU_OBJECTFILE
  bool dumpMetaObj = false;
#endif //#ifdef MART_SEMU_GENMU_OBJECTFILE

  char *tmpStr = nullptr;

  std::string useful_conf_dir;

  tmpStr = new char[1 + std::strlen(argv[0])]; // Alocate tmpStr1
  std::strcpy(tmpStr, argv[0]);
  useful_conf_dir.assign(dirname(tmpStr)); // TODO: check this for install,
                                           // where to put useful. (see klee's)
  delete[] tmpStr;
  tmpStr = nullptr; // del tmpStr1

  useful_conf_dir += "/useful/";

  const char *defaultMconfFile = "mconf-scope/default_allmax.mconf";

  if (mutantConfigfile.empty())
    mutantConfigfile.assign(useful_conf_dir + defaultMconfFile);

  assert(!inputIRfile.empty() && "Error: No input llvm IR file passed!");

  llvm::Module *moduleM;
  std::unique_ptr<llvm::Module> modWMLog(nullptr), _M;

  // Read IR into moduleM
  /// llvm::LLVMContext context;
  if (!ReadWriteIRObj::readIR(inputIRfile, _M))
    return 1;
  moduleM = _M.get();
  // ~

  /// Weak mutation
  if (!disabledWeakMutation) {
    std::string wmLogFuncinputIRfile(useful_conf_dir +
                                     wmLogFuncinputIRfileName);
    // get the module containing the function to log WM info. to be linked with
    // WMModule
    if (!ReadWriteIRObj::readIR(wmLogFuncinputIRfile, modWMLog))
      return 1;
  } else {
    modWMLog = nullptr;
  }

  /**********************  To BE REMOVED (used to extrac line num from bc whith
  llvm 3.8.0)
  for (auto &Func: *moduleM)
  {
      if (Func.isDeclaration())
          continue;
      for (auto &BB: Func)
      {
          for (auto &Inst:BB)
          {
              //Location in source file
              if (const llvm::Instruction *I =
  llvm::dyn_cast<llvm::Instruction>(&Inst)) {
                  std::string tmpStr = UtilsFunctions::getSrcLoc(I);
                  if(!tmpStr.empty()) {
                      llvm::errs() << Func.getName() << " ";
                      llvm::errs() << tmpStr();
                      llvm::errs() << "\n";
                  }
              }
          }
      }
  }
  return 0;
  **********************/

  // test
  /// llvm::errs() << "\n@Before\n"; moduleM->dump(); llvm::errs() <<
  /// "\n============\n";
  // std::string mutconffile;

  // @Mutation
  Mutation mut(*moduleM, mutantConfigfile, dumpMutantsCallback,
               mutantScopeJsonfile);

// Keep Phi2Mem-preprocessed module
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
  std::unique_ptr<llvm::Module> preProPhi2MemModule(llvm::CloneModule(moduleM));
#else
  std::unique_ptr<llvm::Module> preProPhi2MemModule =
      llvm::CloneModule(moduleM);
#endif

  // do mutation
  llvm::outs() << "Mart@Progress: Mutating...\n";
  curClockTime = clock();
  if (!mut.doMutate()) {
    llvm::errs() << "\nMUTATION FAILED!!\n\n";
    return 1;
  }
  llvm::outs() << "Mart@Progress: Mutation took: "
               << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
               << " Seconds.\n";
  loginfo << "Mart@Progress: Mutation took: "
          << (float)(clock() - curClockTime) / CLOCKS_PER_SEC << " Seconds.\n";

  // @Output setup
  struct stat st; // = {0};
  int dcount = 0;
  while (stat((outputDir + std::to_string(dcount)).c_str(), &st) != -1)
    dcount++;

  outputDir += std::to_string(dcount);

  if (mkdir(outputDir.c_str(), 0777) != 0)
    assert(false && "Failed to create output directory");

  //@ Output file name root

  tmpStr = new char[1 + inputIRfile.length()]; // Alocate tmpStr2
  std::strcpy(tmpStr, inputIRfile.c_str());
  outFile.assign(
      basename(tmpStr)); // basename changes the contain of its parameter
  delete[] tmpStr;
  tmpStr = nullptr; // del tmpStr2
  if (!outFile.substr(outFile.length() - 3, 3).compare(".ll") ||
      !outFile.substr(outFile.length() - 3, 3).compare(".bc"))
    outFile.replace(outFile.length() - 3, 3, "");

  // ensure no name clash between executables and folders:
  assert(outFile != mutantsFolder &&
         "please change input IR's name, it clashes with mutants folder");
  assert(outFile != tmpFuncModuleFolder && "please change input IR's name, it "
                                           "clashes with temporary Function "
                                           "Module Folder folder");

  //@ Store Phi2Mem-preprocessed module with the same name of the input file
  if (!ReadWriteIRObj::writeIR(preProPhi2MemModule.get(),
                               outputDir + "/" + outFile + ".bc"))
    assert(false && "Failed to output Phi-preprocessed IR file");

  //@ Print pre-TCE meta-mutant
  if (dumpPreTCEMeta) {
    if (!ReadWriteIRObj::writeIR(moduleM, outputDir + "/" + outFile +
                                              ".preTCE.MetaMu.bc"))
      assert(false && "Failed to output pre-TCE meta-mutatant IR file");
    // mut.dumpMutantInfos (outputDir+"//"+outFile+"mutantLocs-preTCE.json");
  }

  //@ Remove equivalent mutants and //@ print mutants in case on
  llvm::outs() << "Mart@Progress: Removing TCE Duplicates & WM & writing "
                  "mutants IRs (with initially "
               << mut.getHighestMutantID() << " mutants)...\n";
  curClockTime = clock();
  mut.doTCE(modWMLog, dumpMutants, isTCEFunctionMode);
  llvm::outs() << "Mart@Progress: Removing TCE Duplicates  & WM & writing "
                  "mutants IRs took: "
               << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
               << " Seconds.\n";
  loginfo << "Mart@Progress: Removing TCE Duplicates  & WM & writing mutants "
             "IRs took: "
          << (float)(clock() - curClockTime) / CLOCKS_PER_SEC << " Seconds.\n";

  /// Mutants Infos into json
  if (!disableDumpMutantInfos)
    mut.dumpMutantInfos(outputDir + "//" + mutantsInfosFileName);

  //@ Print post-TCE meta-mutant
  if (!disableDumpMetaIRbc) {
    if (!ReadWriteIRObj::writeIR(moduleM,
                                 outputDir + "/" + outFile + ".MetaMu.bc"))
      assert(false && "Failed to output post-TCE meta-mutatant IR file");
  }

//@ print mutants
/*if (dumpMutants)
{
    unsigned highestMutID = mut.getHighestMutantID (*moduleM);
    std::string mutantsDir = outputDir+"//"+mutantsFolder;
    if (mkdir(mutantsDir.c_str(), 0777) != 0)
        assert (false && "Failed to create mutants output directory");
    for (unsigned mid=0; mid <= highestMutID; mid++)
    {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        llvm::Module *formutsModule = llvm::CloneModule(moduleM);
#else
        llvm::Module *formutsModule = llvm::CloneModule(moduleM).get();
#endif
        if (mkdir((mutantsDir+"/"+std::to_string(mid)).c_str(), 0777) != 0)
            assert (false && "Failed to create output directory for mutant");
        if (! mut.getMutant (*formutsModule, mid))
        {
            llvm::errs() << "\nError Failed to generate module for Mutant " <<
mid << "\n\n";
            assert (false && "");
        }
        if (! ReadWriteIRObj::writeIR (formutsModule,
mutantsDir+"//"+std::to_string(mid)+"//"+outFile+".bc"))
        {
            llvm::errs() << "Mutant " << mid << "...\n";
            assert (false && "Failed to output post-TCE meta-mutatant IR file");
        }
    }
}*/

#ifdef MART_SEMU_GENMU_OBJECTFILE
  if (dumpMetaObj) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    std::unique_ptr<llvm::Module> forObjModule(llvm::CloneModule(moduleM));
#else
    std::unique_ptr<llvm::Module> forObjModule = llvm::CloneModule(moduleM);
#endif
    // TODO: insert mutant selection code into the cloned module
    if (!ReadWriteIRObj::writeObj(forObjModule.get(),
                                  outputDir + "/" + outFile + ".MetaMu.o"))
      assert(false && "Failed to output meta-mutatant object file");
  }
#endif //#ifdef MART_SEMU_GENMU_OBJECTFILE
  // llvm::errs() << "@After Mutation->TCE\n"; moduleM->dump(); llvm::errs() <<
  // "\n";

  llvm::outs() << "Mart@Progress: Compiling Mutants ...\n";
  // curClockTime = clock();
  time_t timer = time(NULL); // clock_t do not measure time when calling a
                             // script
  tmpStr = new char[1 + std::strlen(argv[0])]; // Alocate tmpStr3
  std::strcpy(tmpStr, argv[0]);
  std::string compileMutsScript(
      dirname(tmpStr)); // dirname change the contain of its parameter
  delete[] tmpStr;
  tmpStr = nullptr; // del tmpStr3
  // llvm::errs() << ("bash " + compileMutsScript+"/useful/CompileAllMuts.sh
  // "+outputDir+" "+tmpFuncModuleFolder+" yes").c_str() <<"\n";
  /*******/
  // auto sc_code = system(("bash " +
  // compileMutsScript+"/useful/CompileAllMuts.sh "+outputDir+"
  // "+tmpFuncModuleFolder+" yes").c_str());
  // if (sc_code != 0)
  //{
  //     llvm::errs() << "\n:( ERRORS: Mutants Compile script failed (probably
  //     not enough memory) with error: " << sc_code << "!\n\n";
  //     assert (false);
  //}

  // using fork - exec
  pid_t my_pid;
  int child_status;
  if ((my_pid = fork()) < 0) {
    perror("fork failure");
    exit(1);
  }
  if (my_pid == 0) {
    llvm::errs() << "## Child process: compiler\n";
    execl("/bin/bash", "bash",
          (compileMutsScript + "/useful/CompileAllMuts.sh").c_str(),
          LLVM_TOOLS_BINARY_DIR, outputDir.c_str(), tmpFuncModuleFolder.c_str(),
          keepMutantsBCs ? "no" : "yes", (char *)NULL);
    llvm::errs() << "\n:( ERRORS: Mutants Compile script failed (probably not "
                    "enough memory)!!!"
                 << "!\n\n";
    assert(false && "Child's exec failed!");
  } else {
    llvm::errs() << "### Parent process: waiting\n";
    wait(&child_status);
    if (WIFEXITED(child_status)) {
      const int es = WEXITSTATUS(child_status);
      if (es) {
        llvm::errs() << "Compilation failed!!";
        assert(false);
      }
    } else {
      llvm::errs() << "Compilation failed (did not terminate)!!";
      assert(false);
    }
  }
  /********/
  // llvm::outs() << "Mart@Progress:  Compiling Mutants took: "<<
  // (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";

  llvm::outs() << "Mart@Progress:  Compiling Mutants took: "
               << difftime(time(NULL), timer) << " Seconds.\n";
  loginfo << "Mart@Progress:  Compiling Mutants took: "
          << difftime(time(NULL), timer) << " Seconds.\n";

  llvm::outs() << "\nMart@Progress:  TOTAL RUNTIME: "
               << (difftime(time(NULL), totalRunTime) / 60) << " min.\n";
  loginfo << "\nMart@Progress:  TOTAL RUNTIME: "
          << (difftime(time(NULL), totalRunTime) / 60) << " min.\n";

  loginfo << mut.getMutationStats();

  std::ofstream xxx(outputDir + "/" + generalInfo);
  if (xxx.is_open()) {
    xxx << loginfo.str();
    xxx.close();
  } else {
    llvm::errs() << "Unable to create info file:"
                 << outputDir + "/" + generalInfo << "\n\n";
    assert(false);
  }

  return 0;
}
