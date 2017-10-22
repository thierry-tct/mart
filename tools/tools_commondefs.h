
#ifndef __MART_SEMU_GENMU_tools_tools - commondefs__
#define __MART_SEMU_GENMU_tools_tools -commondefs__

#include <libgen.h> //dirname
#include <string>

static const std::string mutantsInfosFileName("mutantsInfos.json");
static const std::string equivalentduplicate_mutantsInfosFileName("equidup-mutantsInfos.json");
static const char *wmOutIRFileSuffix = ".WM.bc";
static const char *preTCEMetaIRFileSuffix = ".preTCE.MetaMu.bc";
static const char *commonIRSuffix = ".bc";
static const char *metaMuIRFileSuffix = ".MetaMu.bc";
static const char *usefulFolderName = "useful";
#ifdef MART_SEMU_GENMU_OBJECTFILE
static const char *metaMuObjFileSuffix = ".MetaMu.o";
#endif

void printVersion() {
  llvm::outs() << "\nMart (Framework for Multi-Programming Language Mutation "
                  "Testing based on LLVM)\n";
  llvm::outs() << "\t" << TOOLNAME << " 1.0\n";
  llvm::outs() << "\nLLVM (http://llvm.org/):\n";
  llvm::outs() << "\tLLVM version " << LLVM_VERSION_MAJOR << "."
               << LLVM_VERSION_MINOR << "." << LLVM_VERSION_PATCH << "\n";
  llvm::outs() << "\tLLVM tools dir: " << LLVM_TOOLS_BINARY_DIR << "\n";
  llvm::outs() << "\n";
}

std::string getUsefulAbsPath(char *argv0) {
  std::string useful_conf_dir;

  char *tmpStr = nullptr;
  tmpStr = new char[1 + std::strlen(argv0)]; // Alocate tmpStr1
  std::strcpy(tmpStr, argv0);
  useful_conf_dir.assign(dirname(tmpStr)); // TODO: check this for install,
                                           // where to put useful. (see klee's)
  delete[] tmpStr;
  tmpStr = nullptr; // del tmpStr1

  useful_conf_dir = useful_conf_dir + "/" + usefulFolderName + "/";
  return useful_conf_dir;
}

#endif
