/*
* Copyright: 2008 by Nadav Rotem. all rights reserved.
* IMPORTANT: This software is supplied to you by Nadav Rotem in consideration
* of your agreement to the following terms, and your use, installation, 
* modification or redistribution of this software constitutes acceptance
* of these terms.  If you do not agree with these terms, please do not use, 
* install, modify or redistribute this software. You may not redistribute, 
* install copy or modify this software without written permission from 
* Nadav Rotem. 
*/
#include "VTargetMachine.h"
#include "llvm/CallingConv.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/TypeSymbolTable.h"
#include "llvm/Intrinsics.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/InlineAsm.h"
#include "llvm/Analysis/ConstantsScanner.h"
#include "llvm/Analysis/FindUsedTypes.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Target/TargetRegistry.h "
#include "llvm/Target/TargetData.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/Support/InstVisitor.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Target/Mangler.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Config/config.h"
#include <algorithm>
#include <sstream>
#include "llvm/Support/raw_ostream.h"

#include <llvm/ADT/DenseMap.h>
#include "llvm/Transforms/Utils/Cloning.h"

#include "listScheduler.h"
#include "RTLWriter.h"
#include "designScorer.h"
#include "vbe/params.h"

using namespace llvm;
using namespace xVerilog;

extern "C" void LLVMInitializeVerilogBackendTarget() { 
  // Register the target.
  RegisterTargetMachine<VTargetMachine> X(TheVBackendTarget);
}

using std::string;
using std::stringstream;
using llvm::TargetData; //JAWAD
namespace {
class VWriter : public FunctionPass {
  llvm::raw_ostream &Out;
 
public:
  static char ID;
  VWriter(llvm::raw_ostream &o) : FunctionPass((intptr_t)&ID),Out(o) {}

  virtual const char *getPassName() const { return "verilog backend"; }
  
  /// @name Pass Interface.
  //{
  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfo>();
    AU.addRequired<TargetData>();//JAWAD
    AU.addRequired<VLang>();
    AU.setPreservesAll();
  }
  virtual bool doInitialization(Module &M);
  virtual bool doFinalization(Module &M);
  bool runOnFunction(Function &F);
  //}
};
} // namespace

char VWriter::ID = 0;

bool VWriter::runOnFunction(Function &F) { 
  for (Function::arg_iterator I = F.arg_begin(), E = F.arg_end(); I != E; ++I) { //JAWAD
    string argname = I->getName(); 
    argname = machineResourceConfig::chrsubst(argname,'.','_');
    I->setName (argname);
  };

  TargetData *TD =  &getAnalysis<TargetData>();//JAWAD
  RTLWriter DesignWriter(getAnalysis<VLang>(), TD);

  listSchedulerVector lv;

  Out<<
    "/*       This module was generated by c-to-verilog.com\n"
    " * THIS SOFTWARE IS PROVIDED BY www.c-to-verilog.com ''AS IS'' AND ANY\n"
    " * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n"
    " * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n"
    " * DISCLAIMED. IN NO EVENT SHALL c-to-verilog.com BE LIABLE FOR ANY\n"
    " * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n"
    " * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES)\n"
    " * \n"
    " * Found a bug? email info@c-to-verilog.com \n"
    " */\n\n\n";

  LoopInfo *LInfo = &getAnalysis<LoopInfo>();
  designScorer ds(LInfo);

  for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB) {
    listScheduler *ls = new listScheduler(BB,TD); //JAWAD
    lv.push_back(ls);
    ds.addListScheduler(ls);
  }

  std::map<string, unsigned int> resourceMap =
    machineResourceConfig::getResourceTable();

  unsigned int include_size = resourceMap["include_size"];
  unsigned int include_freq = resourceMap["include_freq"];
  unsigned int include_clocks = resourceMap["include_clocks"];
  float MDF = (float)resourceMap["delay_memport"];

  float freq = ds.getDesignFrequency();
  float clocks = ds.getDesignClocks();
  float gsize = ds.getDesignSizeInGates(&F);

  if (0==include_freq) freq = 1;
  if (0==include_clocks) clocks = 1;
  if (0==include_size) gsize = 1;

  errs()<<"\n\n---  Synthesis Report ----\n";
  errs()<<"Estimated circuit delay   : " << freq<<"ns ("<<1000/freq<<"Mhz)\n";
  errs()<<"Estimated circuit size    : " << gsize<<"\n";
  errs()<<"Calculated loop throughput: " << clocks<<"\n";
  errs()<<"--------------------------\n";

  errs()<<"/* Total Score= |"<< ((clocks*sqrt(clocks))*(freq)*(gsize))/(MDF) <<"| */"; 
  errs()<<"/* freq="<<freq<<" clocks="<<clocks<<" size="<<gsize<<"*/\n"; 
  errs()<<"/* Clocks to finish= |"<< clocks <<"| */\n"; 
  errs()<<"/* Design Freq= |"<< freq <<"| */\n"; 
  errs()<<"/* Gates Count = |"<< gsize <<"| */\n"; 
  errs()<<"/* Loop BB Percent = |"<< ds.getLoopBlocksCount() <<"| */\n"; 

  Out<<DesignWriter.getFunctionSignature(&F);
  Out<<DesignWriter.getMemDecl(&F);
  Out<<DesignWriter.getFunctionLocalVariables(lv);
  Out<<DesignWriter.getStateDefs(lv);

  Out<<DesignWriter.getAssignmentString(lv);

  Out<<DesignWriter.getClockHeader();
  Out<<"\n// Datapath \n";
  for (listSchedulerVector::iterator I = lv.begin(), E = lv.end();
      I != E; ++I)
    Out<<DesignWriter.printBasicBlockDatapath(*I);

  Out<<"\n\n// Control \n";
  Out<<DesignWriter.getCaseHeader();
  for (listSchedulerVector::iterator I = lv.begin(), E = lv.end();
      I != E; ++I)
    Out<<DesignWriter.printBasicBlockControl(*I);

  Out<<DesignWriter.getCaseFooter();
  Out<<DesignWriter.getClockFooter();
  Out<<DesignWriter.getModuleFooter();
  Out<<"\n\n// -- Library components --  \n";
  Out<<DesignWriter.createBinOpModule("mul","*",resourceMap["delay_mul"]);
  Out<<DesignWriter.createBinOpModule("div","/",resourceMap["delay_div"]);
  Out<<DesignWriter.createBinOpModule("shl","<<",resourceMap["delay_shl"]);
  Out<<DesignWriter.getBRAMDefinition(resourceMap["mem_wordsize"],resourceMap["membus_size"]);
  return false;
}

bool VWriter::doFinalization(Module &M) {
  globalVarRegistry gvr;
  gvr.destroy();
  return true;
}

bool VWriter::doInitialization(Module &M) { 
  globalVarRegistry gvr;
  gvr.init(&M);
  return true;
}


//===----------------------------------------------------------------------===//
//                       External Interface declaration
//===----------------------------------------------------------------------===//

bool VTargetMachine::addPassesToEmitWholeFile(PassManager &PM,
                                              formatted_raw_ostream &Out,
                                              CodeGenFileType FileType,
                                              CodeGenOpt::Level OptLevel,
                                              bool DisableVerify) {
    if (FileType != TargetMachine::CGFT_AssemblyFile) return true;

    //Add the language writer.
    PM.add(new VLang());

    PM.add(new VWriter(Out));
    return false;
}
