/*
 Copyright 2019 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   Compiler.cpp
 * Author: alain
 *
 * Created on March 4, 2017, 5:16 PM
 */
#include <stdint.h>
#include <cstdlib>
#include "CommandLine/CommandLineParser.h"
#include "ErrorReporting/ErrorContainer.h"
#include "SourceCompile/SymbolTable.h"
#include "SourceCompile/CompilationUnit.h"
#include "SourceCompile/PreprocessFile.h"
#include "SourceCompile/CompilationUnit.h"
#include "SourceCompile/CompileSourceFile.h"
#include "SourceCompile/Compiler.h"
#include "SourceCompile/CheckCompile.h"
#include "antlr4-runtime.h"
#include "DesignCompile/CompileDesign.h"
#include "SourceCompile/AnalyzeFile.h"
#include "Library/ParseLibraryDef.h"
#include "Utils/FileUtils.h"
#include "Package/Precompiled.h"
#include "Utils/StringUtils.h"
#include "Utils/Timer.h"
#include <math.h>
using namespace antlr4;

#include "API/PythonAPI.h"
#include "SourceCompile/CheckCompile.h"
#include <mutex>
#include <thread>
#include <vector>
#include <iostream>
#include <fstream>
using namespace SURELOG;

Compiler::Compiler(CommandLineParser* commandLineParser, ErrorContainer* errors,
                   SymbolTable* symbolTable)
    : m_commandLineParser(commandLineParser),
      m_errors(errors),
      m_symbolTable(symbolTable),
      m_commonCompilationUnit(NULL) {
  m_design = NULL;
#ifdef USETBB
  if (getCommandLineParser()->useTbb() &&
      (getCommandLineParser()->getNbMaxTreads() > 0))
    tbb::task_scheduler_init init(getCommandLineParser()->getNbMaxTreads());
#endif
}

Compiler::Compiler(const Compiler& orig) {}

Compiler::~Compiler() {
  std::map<SymbolId, PreprocessFile::AntlrParserHandler*>::iterator itr;
  for (itr = m_antlrPpMap.begin(); itr != m_antlrPpMap.end(); itr++) {
    delete (*itr).second;
  }
  if (m_commonCompilationUnit) {
    delete m_commonCompilationUnit;
  }
  cleanup_();
}

struct FunctorCompileOneFile {
  FunctorCompileOneFile(CompileSourceFile* compileSource,
                        CompileSourceFile::Action action)
      : m_compileSourceFile(compileSource), m_action(action) {}

  int operator()() const {
    if (m_compileSourceFile->getCommandLineParser()->pythonListener() ||
        m_compileSourceFile->getCommandLineParser()
            ->pythonEvalScriptPerFile()) {
      PyThreadState* interpState = PythonAPI::initNewInterp();
      m_compileSourceFile->setPythonInterp(interpState);
    }
    return m_compileSourceFile->compile(m_action);
  }

 private:
  CompileSourceFile* m_compileSourceFile;
  CompileSourceFile::Action m_action;
};

bool Compiler::compileOneFile_(CompileSourceFile* compiler,
                               CompileSourceFile::Action action) {
  bool status = compiler->compile(action);
  return status;
}

bool Compiler::isLibraryFile(SymbolId id) {
  if (m_libraryFiles.find(id) == m_libraryFiles.end()) {
    return false;
  } else {
    return true;
  }
}

bool Compiler::ppinit_() {
  if (!m_commandLineParser->fileunit()) {
    m_commonCompilationUnit = new CompilationUnit(false);
  }

  CompilationUnit* comp_unit = m_commonCompilationUnit;

  // Source files (.v, .sv on the command line)
  std::set<SymbolId> sourceFiles;
  unsigned int size = m_commandLineParser->getSourceFiles().size();
  for (unsigned int i = 0; i < size; i++) {
    SymbolTable* symbols = m_symbolTable;
    if (m_commandLineParser->fileunit()) {
      comp_unit = new CompilationUnit(true);
      m_compilationUnits.push_back(comp_unit);
      symbols = new SymbolTable(*m_commandLineParser->getSymbolTable());
      m_symbolTables.push_back(symbols);
    }
    ErrorContainer* errors = new ErrorContainer(symbols);
    m_errorContainers.push_back(errors);
    errors->regiterCmdLine(m_commandLineParser);

    SymbolId id = m_commandLineParser->getSourceFiles()[i];

    std::string fileName = m_commandLineParser->getSymbolTable()->getSymbol(id);
    std::string fullPath = FileUtils::getFullPath(fileName);
    SymbolId fullPathId =
        m_commandLineParser->getSymbolTable()->registerSymbol(fullPath);
    Library* library = m_librarySet->getLibrary(fullPathId);
    sourceFiles.insert(fullPathId);

    CompileSourceFile* compiler = new CompileSourceFile(
        m_commandLineParser->getSourceFiles()[i], m_commandLineParser, errors,
        this, symbols, comp_unit, library);
    m_compilers.push_back(compiler);
  }

  // Library files
  std::set<SymbolId> libFiles;
  // (-v <file>)
  size = m_commandLineParser->getLibraryFiles().size();
  for (unsigned int i = 0; i < size; i++) {
    SymbolId id = m_commandLineParser->getLibraryFiles()[i];
    libFiles.insert(id);
  }
  // (-y <path> +libext+<ext>)
  for (auto path : m_commandLineParser->getLibraryPaths()) {
    for (auto ext : m_commandLineParser->getLibraryExtensions()) {
      auto files = FileUtils::collectFiles(
          path, ext, m_commandLineParser->getSymbolTable());
      for (auto file : files) {
        libFiles.insert(file);
      }
    }
  }
  for (auto id : libFiles) {
    SymbolTable* symbols = m_symbolTable;
    if (m_commandLineParser->fileunit()) {
      comp_unit = new CompilationUnit(true);
      m_compilationUnits.push_back(comp_unit);
      symbols = new SymbolTable(*m_commandLineParser->getSymbolTable());
      m_symbolTables.push_back(symbols);
    }
    ErrorContainer* errors = new ErrorContainer(symbols);
    m_errorContainers.push_back(errors);
    errors->regiterCmdLine(m_commandLineParser);

    std::string fullPath = FileUtils::getFullPath(
        m_commandLineParser->getSymbolTable()->getSymbol(id));

    // This line registers the file in the "work" library:
    SymbolId fullPathId =
        m_commandLineParser->getSymbolTable()->registerSymbol(fullPath);
    /*Library* library  = */ m_librarySet->getLibrary(fullPathId);
    m_libraryFiles.insert(id);
    m_libraryFiles.insert(fullPathId);
    // No need to register a compiler
    // CompileSourceFile* compiler = new CompileSourceFile
    // (m_commandLineParser->getLibraryFiles ()[i],
    //                                                     m_commandLineParser,
    //                                                     errors, this,
    //                                                     symbols, comp_unit,
    //                                                     library);
    // m_compilers.push_back (compiler);
  }

  // Libraries (.map)
  for (auto& lib : m_librarySet->getLibraries()) {
    for (auto id : lib.getFiles()) {
      std::string fileName = lib.getSymbols()->getSymbol(id);
      if (sourceFiles.find(id) != sourceFiles.end()) {
        // These files are already included in the command line
        continue;
      }

      if (strstr(fileName.c_str(), ".map")) {
        // .map files are not parsed with the regular parser
        continue;
      }
      SymbolTable* symbols = m_symbolTable;
      if (m_commandLineParser->fileunit()) {
        comp_unit = new CompilationUnit(true);
        m_compilationUnits.push_back(comp_unit);
        symbols = new SymbolTable(*m_commandLineParser->getSymbolTable());
        m_symbolTables.push_back(symbols);
      }
      ErrorContainer* errors = new ErrorContainer(symbols);
      m_errorContainers.push_back(errors);
      errors->regiterCmdLine(m_commandLineParser);

      CompileSourceFile* compiler = new CompileSourceFile(
          id, m_commandLineParser, errors, this, symbols, comp_unit, &lib);
      m_compilers.push_back(compiler);
    }
  }
  return true;
}

bool Compiler::createFileList_()
{
  if (m_commandLineParser->writePpOutput() ||
          (m_commandLineParser->writePpOutputFileId() != 0)) {
    SymbolTable* symbolTable = getSymbolTable();
    const std::string& directory =
            symbolTable->getSymbol(m_commandLineParser->getFullCompileDir());
    std::ofstream ofs;
    std::string fileList = directory + "/file.lst";
    ofs.open(fileList);
    if (ofs.good()) {
      unsigned int size = m_compilers.size();
      for (unsigned int i = 0; i < size; i++) {
        std::string fileName = m_compilers[i]->getSymbolTable()->getSymbol(
                m_compilers[i]->getPpOutputFileId());
        fileName = StringUtils::replaceAll(fileName, "//", "/");
        ofs << fileName << std::flush << std::endl;
      }
      ofs.close();
    } else {
      std::cout << "Could not create filelist: " << fileList << std::endl;
    }
  }
  return true;
}
 
bool Compiler::createMultiProcess_() {
  unsigned int nbProcesses = m_commandLineParser->getNbMaxProcesses();
  if (nbProcesses == 0)
    return true;
  // Create CMakeLists.txt
  if (m_commandLineParser->writePpOutput() ||
          (m_commandLineParser->writePpOutputFileId() != 0)) {
    SymbolTable* symbolTable = getSymbolTable();
    const std::string& directory =
            symbolTable->getSymbol(m_commandLineParser->getFullCompileDir());
    std::ofstream ofs;
    std::string fileList = directory + "/CMakeLists.txt";
    ofs.open(fileList);
    if (ofs.good()) {
      ofs << "cmake_minimum_required (VERSION 3.0)" << std::endl;
      ofs << "# Auto generated by Surelog" << std::endl;

      // Optimize the load balance, try to even out the work in each thread by the
      // size of the files
      std::vector<std::vector < CompileSourceFile*>> jobArray(nbProcesses);
      std::vector<unsigned long> jobSize(nbProcesses);
      unsigned int largestJob = 0;
      for (unsigned int i = 0; i < m_compilers.size(); i++) {
        unsigned int size = m_compilers[i]->getJobSize(CompileSourceFile::Action::Parse);
        if (size > largestJob) {
          largestJob = size;
        }
      }
      std::cout << "LARGEST JOB SIZE: " << largestJob << std::endl;
      unsigned int bigJobThreashold = (largestJob / nbProcesses) * 3;
      std::cout << "LARGE JOB THREASHOLD: " << bigJobThreashold << std::endl;
      std::vector<CompileSourceFile*> bigJobs;
      for (unsigned short i = 0; i < nbProcesses; i++) jobSize[i] = 0;
      Precompiled* prec = Precompiled::getSingleton();
      for (unsigned int i = 0; i < m_compilers.size(); i++) {
        std::string root = m_compilers[i]->getSymbolTable()->getSymbol(m_compilers[i]->getFileId());
        root = StringUtils::getRootFileName(root);
        if (prec->isFilePrecompiled(root)) {
          continue;
        }
        unsigned int size = m_compilers[i]->getJobSize(CompileSourceFile::Action::Parse);
        if (size > bigJobThreashold) {
          bigJobs.push_back(m_compilers[i]);
          continue;
        }
        unsigned int newJobIndex = 0;
        uint64_t minJobQueue = ULLONG_MAX;
        for (unsigned short ii = 0; ii < nbProcesses; ii++) {
          if (jobSize[ii] < minJobQueue) {
            newJobIndex = ii;
            minJobQueue = jobSize[ii];
          }
        }
        jobSize[newJobIndex] += size;
        jobArray[newJobIndex].push_back(m_compilers[i]);
      }
      
      std::string full_exe_path = m_commandLineParser->getExePath();
      full_exe_path = FileUtils::getFullPath(full_exe_path);
      full_exe_path = StringUtils::eliminateRelativePath(full_exe_path);
      if (m_commandLineParser->profile()) {
        full_exe_path += " -profile";
      }
        
      std::string fileUnit = "";
      if (m_commandLineParser->fileunit())
        fileUnit = " -fileunit ";
     
      std::vector<std::string > targets;
      int absoluteIndex = 0;
      
      // Big jobs
      for (CompileSourceFile* compiler : bigJobs) {
        absoluteIndex++;
        std::string fileName = compiler->getSymbolTable()->getSymbol(
                                        compiler->getPpOutputFileId());
        std::string targetname = std::to_string(absoluteIndex) + "_"  + FileUtils::fileName(fileName);
        targets.push_back(targetname);
        ofs <<"add_custom_command(OUTPUT " << targetname << std::endl;
        ofs <<"  COMMAND " << full_exe_path << fileUnit <<
                            " -parseonly -mt 0 -mp 0 -nostdout -nobuiltin -l "
                           <<  directory + FileUtils::fileName(targetname) + ".log" << " " << fileName << std::endl;
        ofs << ")" << std::endl;  
      }
        
      // Small jobs batch in clump processes
      for (unsigned short i = 0; i < nbProcesses; i++) {  
        std::string fileList; 
        std::string lastFile;
        absoluteIndex++;
        for (unsigned int j = 0; j < jobArray[i].size(); j++) {
          CompileSourceFile* compiler = jobArray[i][j];
          std::string fileName = compiler->getSymbolTable()->getSymbol(
                                        compiler->getPpOutputFileId());
          fileList += " " + fileName;
          lastFile = fileName;
        } 
        if (fileList != "") {
          std::string targetname = std::to_string(absoluteIndex) + "_" + FileUtils::fileName(lastFile);
          targets.push_back(targetname);
          ofs << "add_custom_command(OUTPUT " << targetname << std::endl;
          ofs << "  COMMAND " << full_exe_path << fileUnit <<
                  " -parseonly -mt 0 -mp 0 -nostdout -nobuiltin -l "
                  << directory + FileUtils::fileName(targetname) + ".log" << " " << fileList << std::endl;
          ofs << ")" << std::endl; 
        }
      }
            
      ofs << "add_custom_target(Parse ALL DEPENDS" << std::endl; 
      for (auto target : targets) {
        ofs << target << std::endl;  
      }
      ofs << ")" << std::endl; 
      ofs << std::flush;      
      ofs.close();
      std::string command = "cd " + directory + ";" + "cmake .; make -j " + std::to_string(nbProcesses);
      std::cout << "Running: " << command << std::endl << std::flush;
      int result = system(command.c_str());
      std::cout << "cmake result: " << result << std::endl;
    } else {
      std::cout << "Could not create CMakeLists.txt: " << fileList << std::endl;
    }
  }
  
  
  return true;
}
  
bool Compiler::parseinit_() {
  Precompiled* prec = Precompiled::getSingleton();
  // Single out the large files.
  // Small files are going to be scheduled in multiple threads based on size.
  // Large files are going to be compiled in a different batch in multithread
  
  if (!m_commandLineParser->fileunit()) {
    unsigned int size = m_symbolTables.size();
    for (unsigned int i = 0; i < size; i++) {
      delete m_symbolTables[i];
    }
    size = m_errorContainers.size();
    for (unsigned int i = 0; i < size; i++) {
      delete m_errorContainers[i];
    }
    m_symbolTables.clear();
    m_errorContainers.clear();
  }

  std::vector<CompileSourceFile*> tmp_compilers;
  unsigned int size = m_compilers.size();
  for (unsigned int i = 0; i < size; i++) {
    std::string fileName = m_compilers[i]->getSymbolTable()->getSymbol(
        m_compilers[i]->getPpOutputFileId());
    std::string origFile = m_compilers[i]->getSymbolTable()->getSymbol(
        m_compilers[i]->getFileId());
    unsigned int nbThreads = m_commandLineParser->getNbMaxTreads();
    std::string root = fileName;
    root = StringUtils::getRootFileName(root);
    if (prec->isFilePrecompiled(root)) {
      nbThreads = 0;
    }
    int effectiveNbThreads = 0;

    if (nbThreads > 4)
      effectiveNbThreads = (int) (log(((float) nbThreads + 1.0) / 4.0) * 10.0);
    else
      effectiveNbThreads = nbThreads;
    
    AnalyzeFile* fileAnalyzer = new AnalyzeFile(
        m_commandLineParser, m_design, fileName, origFile, effectiveNbThreads);
    fileAnalyzer->analyze();
    m_compilers[i]->setFileAnalyzer(fileAnalyzer);
    if (fileAnalyzer->getSplitFiles().size() > 1) {
      // Schedule parent
      m_compilersParentFiles.push_back(m_compilers[i]);
      m_compilers[i]->initParser();

      if (!m_commandLineParser->fileunit()) {
        SymbolTable* symbols =
            new SymbolTable(*m_commandLineParser->getSymbolTable());
        m_symbolTables.push_back(symbols);
        m_compilers[i]->setSymbolTable(symbols);
        // fileContent->setSymbolTable(symbols);
        ErrorContainer* errors = new ErrorContainer(symbols);
        m_errorContainers.push_back(errors);
        errors->regiterCmdLine(m_commandLineParser);
        m_compilers[i]->setErrorContainer(errors);
      }

      FileContent* fileContent =
          new FileContent(m_compilers[i]->getParser()->getFileId(0),
                          m_compilers[i]->getParser()->getLibrary(),
                          m_compilers[i]->getSymbolTable(),
                          m_compilers[i]->getErrorContainer(), NULL, 0);
      m_compilers[i]->getParser()->setFileContent(fileContent);

      int j = 0;
      for (auto chunk : fileAnalyzer->getSplitFiles()) {
        SymbolTable* symbols =
            new SymbolTable(*m_commandLineParser->getSymbolTable());
        m_symbolTables.push_back(symbols);
        SymbolId ppId = symbols->registerSymbol(chunk);
        symbols->registerSymbol(
            m_compilers[i]->getParser()->getFileName(LINE1));
        CompileSourceFile* chunkCompiler = new CompileSourceFile(
            m_compilers[i], ppId, fileAnalyzer->getLineOffsets()[j]);
        // Schedule chunk
        tmp_compilers.push_back(chunkCompiler);

        chunkCompiler->setSymbolTable(symbols);
        ErrorContainer* errors = new ErrorContainer(symbols);
        m_errorContainers.push_back(errors);
        errors->regiterCmdLine(m_commandLineParser);
        chunkCompiler->setErrorContainer(errors);
        // chunkCompiler->getParser ()->setFileContent (fileContent);

        FileContent* fileContent =
            new FileContent(m_compilers[i]->getParser()->getFileId(0),
                            m_compilers[i]->getParser()->getLibrary(), symbols,
                            errors, NULL, ppId);
        chunkCompiler->getParser()->setFileContent(fileContent);
        getDesign()->addFileContent(m_compilers[i]->getParser()->getFileId(0),
                                    fileContent);

        j++;
      }
    } else {
      if (!m_commandLineParser->fileunit()) {
        SymbolTable* symbols =
            new SymbolTable(*m_commandLineParser->getSymbolTable());
        m_symbolTables.push_back(symbols);
        m_compilers[i]->setSymbolTable(symbols);
        ErrorContainer* errors = new ErrorContainer(symbols);
        m_errorContainers.push_back(errors);
        errors->regiterCmdLine(m_commandLineParser);
        m_compilers[i]->setErrorContainer(errors);
      }

      tmp_compilers.push_back(m_compilers[i]);
    }
  }
  m_compilers = tmp_compilers;

  return true;
}

bool Compiler::pythoninit_() { return parseinit_(); }

ErrorContainer::Stats Compiler::getErrorStats() {
  ErrorContainer::Stats stats;
  unsigned int size = m_errorContainers.size();
  for (unsigned int i = 0; i < size; i++) {
    ErrorContainer::Stats tmp;
    tmp = m_errorContainers[i]->getErrorStats();
    stats += tmp;
  }

  return stats;
}

bool Compiler::cleanup_() {
  unsigned int size = m_compilers.size();
  for (unsigned int i = 0; i < size; i++) {
    delete m_compilers[i];
  }
  size = m_compilationUnits.size();
  for (unsigned int i = 0; i < size; i++) {
    delete m_compilationUnits[i];
  }
  size = m_symbolTables.size();
  for (unsigned int i = 0; i < size; i++) {
    delete m_symbolTables[i];
  }
  size = m_errorContainers.size();
  for (unsigned int i = 0; i < size; i++) {
    delete m_errorContainers[i];
  }
  return true;
}

bool Compiler::compileFileSet_(CompileSourceFile::Action action,
                               bool allowMultithread,
                               std::vector<CompileSourceFile*>& container) {
  unsigned short maxThreadCount = m_commandLineParser->getNbMaxTreads();
  if (allowMultithread == false) {
    maxThreadCount = 0;
  }

  if (maxThreadCount == 0) {
    // Single thread
    unsigned int size = container.size();
    for (unsigned int i = 0; i < size; i++) {
      container[i]->setPythonInterp(PythonAPI::getMainInterp());
      bool status = compileOneFile_(container[i], action);
      m_errors->appendErrors(*container[i]->getErrorContainer());
      m_errors->printMessages(m_commandLineParser->muteStdout());
      if ((!status) || container[i]->getErrorContainer()->hasFatalErrors())
        return false;
    }
  } else if (getCommandLineParser()->useTbb() &&
             (action != CompileSourceFile::Action::Parse)) {
#ifdef USETBB
    // TBB Thread management
    int maxThreadCount = m_commandLineParser->getNbMaxTreads();
    if (allowMultithread == false) {
      maxThreadCount = 0;
    }
    unsigned int size = container.size();
    if (maxThreadCount) {
      for (unsigned int i = 0; i < size; i++) {
        m_taskGroup.run(FunctorCompileOneFile(container[i], action));
      }
      m_taskGroup.wait();
      bool fatalErrors = false;
      for (unsigned int i = 0; i < size; i++) {
        // Promote report to master error container
        m_errors->appendErrors(*container[i]->getErrorContainer());
        if (container[i]->getErrorContainer()->hasFatalErrors()) {
          fatalErrors = true;
        }
        if (getCommandLineParser()->pythonListener()) {
          container[i]->shutdownPythonInterp();
        }
        m_errors->printMessages(m_commandLineParser->muteStdout());
      }
      if (fatalErrors) return false;
    } else {
      for (unsigned int i = 0; i < size; i++) {
        container[i]->setPythonInterp(PythonAPI::getMainInterp());
        bool status = compileOneFile_(container[i], action);
        m_errors->appendErrors(*container[i]->getErrorContainer());
        m_errors->printMessages(m_commandLineParser->muteStdout());
        if ((!status) || container[i]->getErrorContainer()->hasFatalErrors())
          return false;
      }
    }
#endif
  } else {
    // Custom Thread management

    // Optimize the load balance, try to even out the work in each thread by the
    // size of the files
    std::vector<std::vector<CompileSourceFile*>> jobArray(maxThreadCount);

    std::vector<unsigned long> jobSize(maxThreadCount);

    for (unsigned short i = 0; i < maxThreadCount; i++) jobSize[i] = 0;

    for (unsigned int i = 0; i < container.size(); i++) {
      unsigned int size = container[i]->getJobSize(action);
      unsigned int newJobIndex = 0;
      uint64_t minJobQueue = ULLONG_MAX;
      for (unsigned short ii = 0; ii < maxThreadCount; ii++) {
        if (jobSize[ii] < minJobQueue) {
          newJobIndex = ii;
          minJobQueue = jobSize[ii];
        }
      }

      jobSize[newJobIndex] += size;
      jobArray[newJobIndex].push_back(container[i]);
    }

    if (getCommandLineParser()->profile()) {
      if (action == CompileSourceFile::Preprocess)
        std::cout << "Preprocessing task\n";
      else if (action == CompileSourceFile::Parse)
        std::cout << "Parsing task\n";
      else
        std::cout << "Misc Task\n";
      for (unsigned short i = 0; i < maxThreadCount; i++) {
        std::cout << "Thread " << i << " : \n";
        int sum = 0;
        for (unsigned int j = 0; j < jobArray[i].size(); j++) {
          std::string fileName;
          if (jobArray[i][j]->getPreprocessor())
            fileName = jobArray[i][j]->getPreprocessor()->getFileName(0);
          if (jobArray[i][j]->getParser())
            fileName = jobArray[i][j]->getParser()->getFileName(0);
          sum += jobArray[i][j]->getJobSize(action);
          std::cout << jobArray[i][j]->getJobSize(action) << " " << fileName
                    << "\n";
        }
        std::cout << ", Total: " << sum << std::endl << std::flush;
      }
    }

    // Create the threads with their respective workloads
    std::vector<std::thread*> threads;
    for (unsigned short i = 0; i < maxThreadCount; i++) {
      std::thread* th = new std::thread([=] {
        for (unsigned int j = 0; j < jobArray[i].size(); j++) {
          if (getCommandLineParser()->pythonListener() ||
              getCommandLineParser()->pythonEvalScriptPerFile()) {
            PyThreadState* interpState = PythonAPI::initNewInterp();
            jobArray[i][j]->setPythonInterp(interpState);
          }

          jobArray[i][j]->compile(action);

          if (getCommandLineParser()->pythonListener() ||
              getCommandLineParser()->pythonEvalScriptPerFile()) {
            jobArray[i][j]->shutdownPythonInterp();
          }
        }
      });
      threads.push_back(th);
    }

    // Sync the threads
    for (unsigned int th = 0; th < threads.size(); th++) {
      threads[th]->join();
    }

    // Delete the threads
    for (unsigned int th = 0; th < threads.size(); th++) {
      delete threads[th];
    }

    // Promote report to master error container
    bool fatalErrors = false;
    for (unsigned int j = 0; j < container.size(); j++) {
      m_errors->appendErrors(*container[j]->getErrorContainer());
      if (container[j]->getErrorContainer()->hasFatalErrors())
        fatalErrors = true;
    }
    m_errors->printMessages(m_commandLineParser->muteStdout());

    if (fatalErrors) return false;
  }
  return true;
}

bool Compiler::compile() {
  std::string profile;
  Timer tmr;
  Timer tmrTotal;
  // Scan the libraries definition
  if (!parseLibrariesDef_()) return false;

  if (m_commandLineParser->profile()) {
    std::string msg = "Scan libraries took " +
                      StringUtils::to_string(tmr.elapsed_rounded()) + "s\n";
    std::cout << msg << std::endl;
    profile += msg;
    tmr.reset();
  }

  // Preprocess
  ppinit_();
  if (!compileFileSet_(CompileSourceFile::Preprocess,
                       m_commandLineParser->fileunit(), m_compilers))
    return false;

  // Single thread post Preprocess
  if (!compileFileSet_(CompileSourceFile::PostPreprocess, false, m_compilers))
    return false;

  if (m_commandLineParser->profile()) {
    std::string msg = "Preprocessing took " +
                      StringUtils::to_string(tmr.elapsed_rounded()) + "s\n";
    std::cout << msg << std::endl;
    for (unsigned int i = 0; i < m_compilers.size(); i++) {
      msg += m_compilers[i]->getPreprocessor()->getProfileInfo();
    }
    std::cout << msg << std::endl;
    profile += msg;
    tmr.reset();
  }

  // Parse
  bool parserInitialized = false;
  if (m_commandLineParser->parse() || m_commandLineParser->pythonListener() ||
      m_commandLineParser->pythonEvalScriptPerFile() ||
      m_commandLineParser->pythonEvalScript()) {
    parseinit_();
    createFileList_();
    createMultiProcess_();
    parserInitialized = true;
    if (!compileFileSet_(CompileSourceFile::Parse, true, m_compilers))
      return false;  // Small files and large file chunks
    if (!compileFileSet_(CompileSourceFile::Parse, true,
                         m_compilersParentFiles))
      return false;  // Recombine chunks
  } else {
    createFileList_();
  }

  if (m_commandLineParser->profile()) {
    std::string msg =
        "Parsing took " + StringUtils::to_string(tmr.elapsed_rounded()) + "s\n";
    for (unsigned int i = 0; i < m_compilersParentFiles.size(); i++) {
      msg += m_compilersParentFiles[i]->getParser()->getProfileInfo();
    }
    for (unsigned int i = 0; i < m_compilers.size(); i++) {
      msg += m_compilers[i]->getParser()->getProfileInfo();
    }

    std::cout << msg << std::endl;
    profile += msg;
    tmr.reset();
  }

  // Check Parsing
  CheckCompile* checkComp = new CheckCompile(this);
  checkComp->check();
  delete checkComp;
  m_errors->printMessages(m_commandLineParser->muteStdout());

  // Python Listener
  if (m_commandLineParser->pythonListener() ||
      m_commandLineParser->pythonEvalScriptPerFile()) {
    if (!parserInitialized) pythoninit_();
    if (!compileFileSet_(CompileSourceFile::PythonAPI, true, m_compilers))
      return false;
    if (!compileFileSet_(CompileSourceFile::PythonAPI, true,
                         m_compilersParentFiles))
      return false;

    if (m_commandLineParser->profile()) {
      std::string msg = "Python file processing took " +
                        StringUtils::to_string(tmr.elapsed_rounded()) + "s\n";
      std::cout << msg << std::endl;
      profile += msg;
      tmr.reset();
    }
  }

  if (m_commandLineParser->compile()) {
    // Compile Design, has its own thread management
    CompileDesign* compileDesign = new CompileDesign(this);
    compileDesign->compile();
    m_errors->printMessages(m_commandLineParser->muteStdout());

    if (m_commandLineParser->profile()) {
      std::string msg = "Compilation took " +
                        StringUtils::to_string(tmr.elapsed_rounded()) + "s\n";
      std::cout << msg << std::endl;
      profile += msg;
      tmr.reset();
    }

    if (m_commandLineParser->elaborate()) {
      compileDesign->elaborate();
      m_errors->printMessages(m_commandLineParser->muteStdout());

      if (m_commandLineParser->profile()) {
        std::string msg = "Elaboration took " +
                          StringUtils::to_string(tmr.elapsed_rounded()) + "s\n";
        std::cout << msg << std::endl;
        profile += msg;
        tmr.reset();
      }

      if (m_commandLineParser->pythonEvalScript()) {
        PythonAPI::evalScript(m_commandLineParser->getSymbolTable()->getSymbol(
                                  m_commandLineParser->pythonEvalScriptId()),
                              m_design);
        if (m_commandLineParser->profile()) {
          std::string msg = "Python design processing took " +
                            StringUtils::to_string(tmr.elapsed_rounded()) +
                            "s\n";
          profile += msg;
          std::cout << msg << std::endl;
          tmr.reset();
        }
      }
      m_errors->printMessages(m_commandLineParser->muteStdout());
    }

    std::string directory = m_commandLineParser->getSymbolTable()->getSymbol(m_commandLineParser->getFullCompileDir());
    std::string uhdmFile = directory + "/surelog.uhdm";
    compileDesign->writeUHDM(uhdmFile);
    
    delete compileDesign;
  }
  if (m_commandLineParser->profile()) {
    std::string msg = "Total time " +
                      StringUtils::to_string(tmrTotal.elapsed_rounded()) +
                      "s\n";
    profile += msg;
    profile = std::string("==============\n") + "PROFILE\n" +
              std::string("==============\n") + profile + "==============\n";
    std::cout << profile << std::endl;
    m_errors->printToLogFile(profile);
  }
  return true;
}

void Compiler::registerAntlrPpHandlerForId(
    SymbolId id, PreprocessFile::AntlrParserHandler* pp) {
  std::map<SymbolId, PreprocessFile::AntlrParserHandler*>::iterator itr =
      m_antlrPpMap.find(id);
  if (itr != m_antlrPpMap.end()) {
    delete (*itr).second;
    m_antlrPpMap.erase(itr);
    m_antlrPpMap.insert(std::make_pair(id, pp));
    return;
  }
  m_antlrPpMap.insert(std::make_pair(id, pp));
}

PreprocessFile::AntlrParserHandler* Compiler::getAntlrPpHandlerForId(
    SymbolId id) {
  std::map<SymbolId, PreprocessFile::AntlrParserHandler*>::iterator itr =
      m_antlrPpMap.find(id);
  if (itr != m_antlrPpMap.end()) {
    PreprocessFile::AntlrParserHandler* ptr = (*itr).second;
    return ptr;
  }
  return NULL;
}

bool Compiler::parseLibrariesDef_() {
  m_librarySet = new LibrarySet();
  m_configSet = new ConfigSet();
  m_design = new Design(getErrorContainer(), m_librarySet, m_configSet);
  ParseLibraryDef* libParser = new ParseLibraryDef(
      m_commandLineParser, m_errors, m_symbolTable, m_librarySet, m_configSet);
  return libParser->parseLibrariesDefinition();
}
