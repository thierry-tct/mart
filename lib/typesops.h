
#ifndef __KLEE_SEMU_GENMU_typesops__
#define __KLEE_SEMU_GENMU_typesops__

#include <sstream>
#include <vector>
#include <set>

#include "llvm/IR/Value.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/DenseMap.h"

#include "llvm/IR/LLVMContext.h"    //context

#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DataLayout.h"     //datalayout

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
#include "llvm/DebugInfo.h"     //DIScope
#else
#include "llvm/IR/DebugInfoMetadata.h"  //DIScope
#endif

#include "usermaps.h"

#include "third-parties/JsonBox/include/JsonBox.h"      //https://github.com/anhero/JsonBox

class UtilsFunctions
{
    /**
     * \brief Print src location into the string stream. @note: better use getSrcLoc() to access src loc info of an instruction
     */
    static void printLoc (llvm::DebugLoc const & Loc, llvm::raw_string_ostream & ross)
    {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        if (! Loc.isUnknown()) 
        {
            llvm::DIScope Scope(Loc.getScope(llvm::getGlobalContext()));
            ross << Scope.getFilename();
            ross << ':' << Loc.getLine();
            if (Loc.getCol() != 0)
                ross << ':' << Loc.getCol();
            
            llvm::MDNode * inlineMN =  Loc.getInlinedAt(llvm::getGlobalContext());
            if (inlineMN)
            {
                llvm::DebugLoc InlinedAtDL = llvm::DebugLoc::getFromDILocation(inlineMN);
                if (! InlinedAtDL.isUnknown()) 
                {
                    ross << " @[ ";
                    printLoc (InlinedAtDL, ross);
                    ross << " ]";
                }
            }
        }
#else
        if (Loc) 
        {
            auto *Scope = llvm::cast<llvm::DIScope>(Loc.getScope());
            ross << Scope->getFilename();
            ross << ':' << Loc.getLine();
            if (Loc.getCol() != 0)
                ross << ':' << Loc.getCol();
            
            if (llvm::DebugLoc InlinedAtDL = Loc.getInlinedAt()) 
            {
                ross << " @[ ";
                printLoc (InlinedAtDL, ross);
                ross << " ]";
            }
        }
#endif
    }
  
  public:
    /**
     * \bief get the src location of the instruction and return it as a string, if do not exist, return the empty string
     */
    static std::string getSrcLoc (llvm::Instruction const *inst)
    {
         const llvm::DebugLoc& Loc = inst->getDebugLoc();
         std::string tstr;
         llvm::raw_string_ostream ross(tstr);
         printLoc (Loc, ross);
         return ross.str();
    }
};

/**
 * \bref This class define the information about the current modules, that can be useful for the mutation operator 
 */     
struct ModuleUserInfos
{
  private:
    // \bref Current Module
    llvm::Module * curModule;
    
    // \bref Data Layout of the current module (useful to get information like pointer size...)
    llvm::DataLayout *DL;
    
    // \bref The current context
    llvm::LLVMContext *curContext;
    
    // \bref The 
    UserMaps * usermaps;
    
  public:
    const char * G_MAIN_FUNCTION_NAME = "main";
    
    ModuleUserInfos (llvm::Module *module, UserMaps *umaps)
    {
        DL = nullptr;
        setModule(module);
        setUserMaps(umaps);
    }
    ~ModuleUserInfos ()
    {
        delete DL;
    }
    
    inline void setModule (llvm::Module *module)
    {
        curModule = module;
        if (DL)
        {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            DL->init(module->getDataLayout());   //here 'module->getDataLayout()' returns a string
#else
            DL->init(module);
#endif
        }
        else
        {
            DL = new llvm::DataLayout(curModule);
        }
        curContext = &(llvm::getGlobalContext());
    }
    inline void setUserMaps (UserMaps * umaps) {usermaps = umaps;}
    
    inline llvm::Module *getModule() const {return curModule;}
    inline llvm::DataLayout const &getDataLayout() const {return *DL;}
    inline llvm::LLVMContext &getContext() const {return *curContext;}
    inline UserMaps *getUserMaps() const {return usermaps;}
};


// Each statement is a string where elements are separated by spaces
class llvmMutationOp
{
public:
    struct MutantReplacors
    {
      private:
        enum ExpElemKeys expElemKey;
        std::vector<unsigned> oprdIndexes;
        std::string mutOpName;
      public:
        MutantReplacors (enum ExpElemKeys e, std::vector<unsigned> o, std::string mn): expElemKey(e), oprdIndexes(o), mutOpName(mn) {}
        inline enum ExpElemKeys getExpElemKey() const {return expElemKey;}     //first
        inline const std::vector<unsigned> & getOprdIndexList() const {return oprdIndexes;}   //second
        inline const std::string & getMutOpName() const {return mutOpName;}
        inline const std::string * getMutOpNamePtr() const {return &mutOpName;}
    };
    
private:
    enum ExpElemKeys matchOp;
    std::vector<MutantReplacors> mutantReplacorsList;
    std::vector<enum codeParts> oprdCPType;

public:    
    llvmMutationOp(){};
    ~llvmMutationOp (){};
    void setMatchOp (enum ExpElemKeys m, std::vector<std::string> const &oprdsStr, unsigned fromPos=0)
    {
        matchOp = m;
        for (std::vector<std::string>::const_iterator it=oprdsStr.begin()+fromPos, ie=oprdsStr.end(); it != ie; ++it)
            oprdCPType.push_back(UserMaps::getCodePartType(*it));
    }
    void addReplacor (const enum ExpElemKeys repop, std::vector<unsigned> const &oprdpos, std::string const &name)
    {
        mutantReplacorsList.push_back(MutantReplacors(repop, oprdpos, name));
    }
    inline std::vector<MutantReplacors> const & getMutantReplacorsList() const {return mutantReplacorsList;}
    inline const MutantReplacors & getReplacor(unsigned ind) const {return mutantReplacorsList.at(ind);}
    inline unsigned getNumReplacor() const {return mutantReplacorsList.size();}
    inline enum ExpElemKeys getMatchOp() const {return matchOp;}
    inline enum codeParts getCPType (unsigned pos) const {return oprdCPType.at(pos);}
    
    ////////////////                          //////////////////
    /// These static fields are used to manage constant values /
    ////////////////////////////////////////////////////////////
private:
    static std::map<unsigned, std::string> posConstValueMap;
    static const unsigned maxOprdNum = 1000;

public:
    /// Check whether the index (pos) represent the index in the ConstValue Map of a user specified constant or an operand index.
    inline static bool isSpecifiedConstIndex(unsigned pos) {return (pos > maxOprdNum);}
    
    /**
     * \brief This method 
     */
    static unsigned insertConstValue (std::string val, bool numeric=true)
    {
        //check that the string represent a Number (Int(radix 10 or 16) or FP(normal or exponential))
        if (numeric && ! isNumeric(llvm::StringRef(val).lower().c_str(), 10))   /// radix 10 only for now. \todo implement others (8, 16) @see ifConstCreate
        {
            //return a value <= maxOprdNum
            return 0;
        }
        
        static unsigned curPos = maxOprdNum;        //static, keep value of previous call
        curPos ++;
        
        for (std::map<unsigned, std::string>::iterator it = posConstValueMap.begin(), 
                ie = posConstValueMap.end(); it != ie; ++it)
        {
            if (! val.compare(it->second))
                return it->first;
        }
        if (! posConstValueMap.insert(std::pair<unsigned, std::string>(curPos, val)).second)
            assert (false && "Error: inserting an already existing key in map posConstValueMap");
        return curPos;
    }
    static std::string &getConstValueStr (unsigned pos) 
    {
        //Fails if pos is not in map
        return posConstValueMap.at(pos);
    }
    static void destroyPosConstValueMap()
    {
        posConstValueMap.clear();
    }
    
    static bool isNumeric( const char* pszInput, int nNumberBase )
    {
	    std::istringstream iss( pszInput );

	    if ( nNumberBase == 10 )
	    {
		    double dTestSink;
		    iss >> dTestSink;
	    }
	    else if ( nNumberBase == 8 || nNumberBase == 16 )
	    {
		    int nTestSink;
		    iss >> ( ( nNumberBase == 8 ) ? std::oct : std::hex ) >> nTestSink;
	    }
	    else
	        return false;

	    // was any input successfully consumed/converted?
	    if ( ! iss )
	        return false;

	    // was all the input successfully consumed/converted?
	    return ( iss.rdbuf()->in_avail() == 0 );
    }
    
    std::string toString()
    {
        std::string ret(std::to_string(matchOp) + " --> ");
        for (auto &v: mutantReplacorsList)
        {
            ret+=std::to_string(v.getExpElemKey())+"(";
            for (unsigned pos: v.getOprdIndexList())
            {
                ret+="@"+std::to_string(pos)+",";
            }
            if (ret.back() == ',')
                ret.pop_back();
            ret+=");";
        }
        ret+="\n";
        return ret; 
    }
};

//std::map<unsigned, std::string> llvmMutationOp::posConstValueMap;     //To avoid linking error (double def), this is moved into usermaps.cpp


/**
 * \brief Define the scope of the mutation (Which source file's code to mutate, which function to mutate, should we mutate function call parameters? array indexing?)
 */
class MutationScope
{
  private:
    bool mutateAllFuncs;
    std::vector<llvm::Function *> funcsToMutate;
  public:
    void Initialize (llvm::Module &module, std::string inJsonFilename="")
    {
        // No scope data was given, use everything
        if (inJsonFilename.empty())
        {
            mutateAllFuncs = true;
            funcsToMutate.clear();
            return;
        }
        
        std::set<std::string> specSrcFiles;
        std::set<std::string> specFuncs;
        
        std::set<std::string> seenSrcs;
        
        JsonBox::Value inScope;
	    inScope.loadFromFile(inJsonFilename);
	    if (! inScope.isNull())
	    {
	        assert (inScope.isObject() && "The JSON file data of mutation scope must be a JSON object");
	        if (! inScope["Source-Files"].isNull())     //If "Source-Files" is not absent (select some)
	        {
	            assert (inScope["Source-Files"].isArray() && "The list of Source-File to mutate, if present, must be a JSON array of string");
	            JsonBox::Array const &srcList = inScope["Source-Files"].getArray();
	            if (srcList.size() > 0)     //If there are some srcs specified
	            {
	                for (auto &val: srcList)
	                {
	                    assert (val.isString() && "An element of the JSON array Source-Files is not a string. source file name must be string.");
	                    if (! specSrcFiles.insert(val.getString()).second)
	                    {
	                        llvm::errs() << "Failed to insert the source " << val.getString() << " into set of srcs. Probably specified twice.";
	                        assert (false);
	                    }
	                }
	            }
	        }
	        if (! inScope["Functions"].isNull())
	        {
	            //Check if no function is specified, mutate all (from the selected sources), else only mutate those specified
	            assert (inScope["Functions"].isArray() && "The list of Functions to mutate, if present, must be a JSON array of string");
	            JsonBox::Array const &funcList = inScope["Functions"].getArray();
	            if (funcList.size() > 0)     //If there are some srcs specified
	            {
	                for (auto &val: funcList)
	                {
	                    assert (val.isString() && "An element of the JSON array Functions is not a string. Functions name must be string.");
	                    if (! specFuncs.insert(val.getString()).second)
	                    {
	                        llvm::errs() << "Failed to insert the source " << val.getString() << " into set of Funcs. Probably specified twice.";
	                        assert (false);
	                    }
	                }
	            }
	        }
	        
	        /*if (! inScope[""].isNull())
	        {
	        
	        }*/
	        
	        bool hasDbgIfSrc = (specSrcFiles.empty());      //When no source is specified, no need to check existance of dbg info
	        for (auto &Func: module)
	        {
	            if (Func.isDeclaration())
                    continue;
                
                bool canMutThisFunc = true;
                if (!specSrcFiles.empty())
                {
                    std::string srcOfF;
                    for (auto &BB: Func)
                    {
                        for (auto &Inst: BB)
                        {
                            ///Get Src level LOC
                            std::string tmpSrc = UtilsFunctions::getSrcLoc(&Inst);
                            if (! tmpSrc.empty())
                            {
                                srcOfF.assign(tmpSrc);
                                assert (srcOfF.length() > 0 && "LOC in source is empty string");
                                break;
                            }
                        }
                        if (srcOfF.length() > 0)
                        {
                            hasDbgIfSrc = true;
                            std::size_t found = srcOfF.find(":");
                            assert (found!=std::string::npos && "Problem with the src loc info");
                            if (specSrcFiles.count(srcOfF.substr(0, found+1)))
                            {
                                canMutThisFunc = false;
                                seenSrcs.insert(srcOfF.substr(0, found+1));
                            }
                            break;
                        }
                    }
                }
                if (!specFuncs.empty())
                {
                    if (specFuncs.count(Func.getName()))
                    {
                        assert (canMutThisFunc && "Error in input file: Function selected to be mutated but corresponding source file not selected.");
                        funcsToMutate.push_back(&Func);
                    }
                }
                else if (canMutThisFunc)
                {
                    funcsToMutate.push_back(&Func);
                }
	        }
	        assert (hasDbgIfSrc && "No debug information in the module, but mutation source file scope given");
	        if (specFuncs.empty())
	            assert (specFuncs.size() == funcsToMutate.size() && "Error: Either some specified function do not exist or Bug (non specified function is added).");
	        assert (seenSrcs.size() == specSrcFiles.size() && "Some specified sources file are not found in the module.");
	    }
	    mutateAllFuncs = funcsToMutate.empty();
    }
};


/**
 * \brief This class represent the list of mutants of a statement, generated by mutation op, these are not yet attached to the module. Once attached, update @see MutantList
 */
struct MutantsOfStmt
{
    struct RawMutantStmt
    {
        std::vector<llvm::Value *> mutantStmtIR;
        
        //Mutant type
        std::string typeName;
        
        // Location infos
        std::vector<unsigned> irRelevantPos;  /// The statement mutation operation object sets this to the position in the original's (toMatch) vector (can use @seeDoReplaceUseful 's relevantIRPos)
        
        RawMutantStmt(std::vector<llvm::Value *> const & toMatch, std::vector<llvm::Value *> const &toMatchClone, llvmMutationOp::MutantReplacors const &repl, std::vector<unsigned> const &relevantPos)
        {
            mutantStmtIR = toMatchClone;
            typeName = repl.getMutOpName();
            irRelevantPos = relevantPos;
        }
    };
    std::vector<RawMutantStmt> results;
    
    /**
     * \brief This method add a new mutant statement.
     * \detail It computes the corresponding Weak mutation using difference between @param toMatch and @param toMatchClone
     * @param toMatch is the original Stmt (list of IRs)
     * @param toMatchClone is the mutant Stmt (list of IRs)
     * @param repl is the mutation replacer that was used to tranform @param toMatch and obtain @param toMatchClone  (list of IRs)
     * @param relevantPos is the list of indices in @param toMatch of the IRs modified by the mutation (relevant to mutation)
     */
    inline void add (std::vector<llvm::Value *> const & toMatch, std::vector<llvm::Value *> const &toMatchClone, llvmMutationOp::MutantReplacors const &repl, std::vector<unsigned> const &relevantPos)
    {
        results.emplace_back(toMatch, toMatchClone, repl, relevantPos);
    }
    
    inline void clear() {results.clear();}
    
    inline unsigned getNumMuts() {return results.size();}
    
    inline const std::vector<llvm::Value *> & getMutantStmtIR(unsigned index) {return results[index].mutantStmtIR;}
    inline const std::string & getTypeName(unsigned index) {return results[index].typeName;}
    inline const std::vector<unsigned> & getIRRelevantPos(unsigned index) {return results[index].irRelevantPos;}
};

/**
 * \brief This class define the final list of all mutant and their informations. @Note: This is increased after each statement mutation and modifed (reduced) during TCE equivalent mutant removal
 */
struct MutantInfoList  
{
    struct MutantInfo
    {
        //Mutant id
        unsigned id;
        
        //Mutant type
        std::string typeName;
        
        // Location infos
        std::string locFuncName;
        std::vector<unsigned> irLeveLocInFunc;
        std::string srcLevelLoc;
        
        MutantInfo (unsigned mid, std::vector<llvm::Value *> const &toMatch, std::string const &mName, std::vector<unsigned> const &relpos, llvm::Function *curFunc, std::vector<unsigned> const & absPos)//:
            //typeName (mName)
        {
            id = mid;
            typeName = mName;
            locFuncName = curFunc->getName();
            for (auto ind:relpos)
                irLeveLocInFunc.push_back(absPos[ind]);
            
            /// Get SRC level LOC    
            for (auto * val: toMatch)
            {
                if (const llvm::Instruction *I = llvm::dyn_cast<llvm::Instruction>(val)) 
                {
                    std::string tmpSrc = UtilsFunctions::getSrcLoc(I);
                    if (!tmpSrc.empty())
                    {
                        assert ((srcLevelLoc.empty() || srcLevelLoc == tmpSrc) && "A stmt spawn more than 1 src level line of code");
                        if (srcLevelLoc.empty())
                        {
                            srcLevelLoc.assign(tmpSrc);
                            //break;
                        }
                    }
                }
            }
        }
    };
    
    std::vector<MutantInfo> mutants;
    
    /**
     * \brief This method add a new mutant's info.
     * \detail It computes the corresponding location using @param relpos and @param curFunc
     * @param toMatch is the original Stmt (list of IRs). Needed to obtain metadata for src level location
     * @param mid is the id of the mutant
     * @param name is the name type of this mutant (as given by the user)
     * @param relpos is the list of indices in toMatch of the mutants IRs
     * @param curFunc is the function containing @param toMatch
     * @param toMatchIRPosInFunc is the list of positions of each IR in toMatch in the function @param curFunc
     */
    void add (unsigned mid, std::vector<llvm::Value *> const &toMatch, std::string const &mName, std::vector<unsigned> const &relpos, llvm::Function *curFunc, std::vector<unsigned> const &  toMatchIRPosInFunc)
    {
        mutants.emplace_back(mid, toMatch, mName, relpos, curFunc, toMatchIRPosInFunc);
    }
    
    /**
     *  \brief remove the TCE's equivalent and duplicate mutants
     */
    void postTCEUpdate(std::map<unsigned, std::vector<unsigned>> const & duplicateMap)
    {
        std::vector<unsigned> posToDel;
        unsigned pos = 0;
        for (auto &mut: mutants)
        {
            if (duplicateMap.count(mut.id) == 0)
                posToDel.push_back(pos);
            else
            {
                assert (duplicateMap.at(mut.id).size() == 1 && "(CHECK) The number of element in second vector must be one here (containing the new mutant ID)");
                mut.id = duplicateMap.at(mut.id).back();
            }
            pos++;
        }
        std::sort(posToDel.begin(), posToDel.end());
        for (auto it = posToDel.rbegin(), ie = posToDel.rend(); it != ie; ++it)
        {
            assert (mutants.at(*it).id == (1 + (*it)) && "Problem with the order of mutants, or should delete last element first");
            mutants.erase (mutants.begin() + (*it));
        }
    }
    
    void printToStdout () 
    {
        llvm::errs() << "\n~~~~~~~~~ MUTANTS INFOS ~~~~~~~~~\n\nID, Name, Location\n-------------------\n";
        for (auto &info: mutants)
        {
            llvm::errs() << info.id << "," << info.typeName << "," << info.srcLevelLoc << "\n";
        }
        //JsonBox::Object outJSON;
        //getJson (outJSON);
        //outJSON.writeToStream(std::cout, true, true);
    }
    
    void printToJsonFile (std::string filename)
    {
        JsonBox::Object outJSON;
        getJson (outJSON);
        JsonBox::Value vout(outJSON);
	    vout.writeToFile(filename, false, false);
    }
    
    void getJson (JsonBox::Object &outJ)
    {
        for (auto &info: mutants)
        {
            std::string mid(std::to_string(info.id));
            //add a mutant field 
            outJ[mid] = JsonBox::Object();
            
            //Fill in the mutant infos
            outJ[mid]["Type"] = JsonBox::Value(info.typeName);
            outJ[mid]["SrcLoc"] = JsonBox::Value(info.srcLevelLoc);
            outJ[mid]["FuncName"] = JsonBox::Value(info.locFuncName);
            JsonBox::Array tmparr;
            for (auto pos: info.irLeveLocInFunc)
                tmparr.push_back(JsonBox::Value((int)pos));
            outJ[mid]["IRPosInFunc"] = tmparr;
        }
    }
    
    void loadFromJsonFile (std::string filename)
    {
        assert (false && "To Be Implemented");
    }
};

#endif  //__KLEE_SEMU_GENMU_typesops__

