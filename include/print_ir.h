#pragma once

#ifndef _PRINT_IR_H_
#define _PRINT_IR_H_

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Module.h"
#include <fstream>
#include "triton/ir/print.h"

static int print_count = 0;

inline void print_llvm_ir(llvm::Module &llvm_module, std::string suffix)
{
    // Dump LLVM IR.
    std::string ir_path = llvm_module.getModuleIdentifier() + suffix + std::string(".ir");
    std::error_code ec;
    std::unique_ptr<llvm::raw_fd_ostream> ir_fs(
        new llvm::raw_fd_ostream(ir_path, ec, llvm::sys::fs::OF_None));
    llvm_module.print(*ir_fs, nullptr);
    ir_fs->flush();
}

inline void print_llvm_ir_tracked(llvm::Module &llvm_module, std::string suffix)
{
    
    // Dump LLVM IR.
    std::string ir_path = llvm_module.getModuleIdentifier() + suffix + "_" + std::to_string(print_count) + std::string(".ir");
    std::error_code ec;
    std::unique_ptr<llvm::raw_fd_ostream> ir_fs(
        new llvm::raw_fd_ostream(ir_path, ec, llvm::sys::fs::OF_None));
    llvm_module.print(*ir_fs, nullptr);
    ir_fs->flush();
    print_count += 1;
}

inline void print_triton_ir(triton::ir::module ir_ref, std::string name)
{
    std::ofstream ir_out(name);
    ir_out.flush();
    triton::ir::print(ir_ref, ir_out);
    ir_out.close();
}


#endif