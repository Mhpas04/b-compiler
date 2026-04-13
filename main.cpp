#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/PassManager.h>

// Code Generation and Target headers
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/IR/LegacyPassManager.h>

#include "Compilation/LLVMEmitter.h"
#include "Parsing/Parser.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_source_file>\n";
        return 1;
    }

    const std::string filePath = argv[1];
    const std::string outputPath = argv[2];

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file '" << filePath << "'\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string sourceCode = buffer.str();

    file.close();

    toy::Parser parser(sourceCode);
    const auto programAST = parser.parse();

    llvm::LLVMContext context;
    toy::LLVMEmitter emitter(programAST, context);

    const auto module = emitter.buildModule();
    //module->print(llvm::outs(), nullptr);

    // Optimize the Code
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    llvm::PassBuilder PB;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // Does an O2 Level optimization for the code
    llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);

    MPM.run(*module, MAM);

    // Generate the Native Code
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();

    // Get the architecture of the current machine
    auto targetTriple = llvm::Triple(llvm::sys::getDefaultTargetTriple());
    module->setTargetTriple(targetTriple);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        std::cerr << "Target lookup failed: " << error << "\n";
        return 1;
    }

    auto CPU = "generic"; // TODO: Select Better Features
    auto Features = "";
    llvm::TargetOptions opt;
    auto targetMachine = target->createTargetMachine(targetTriple, CPU, Features, opt, llvm::Reloc::PIC_);

    module->setDataLayout(targetMachine->createDataLayout());

    std::error_code EC;
    llvm::raw_fd_ostream dest(outputPath, EC, llvm::sys::fs::OF_None);
    if (EC) {
        std::cerr << "Could not open output file: " << EC.message() << "\n";
        return 1;
    }

    //Now for the final .o file emitting
    llvm::legacy::PassManager pass;
    auto FileType = llvm::CodeGenFileType::ObjectFile;

    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        std::cerr << "TargetMachine can't emit a file of this type\n";
        return 1;
    }

    pass.run(*module);
    dest.flush();

    std::cout << "Successfully compiled to " << outputPath << "\n";

    return 0;
}
