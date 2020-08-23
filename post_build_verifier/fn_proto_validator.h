#pragma once

#include "common.h"
#include "bitcode_data.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/User.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/Transforms/Utils/Debugify.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Linker/Linker.h"

#include "generated/pochivm_runtime_cpp_types.generated.h"

namespace PochiVM
{

using namespace llvm;

template<typename T>
struct validate_type
{
    static_assert(sizeof(T) == 0, "Bad type T");
};

#define FOR_EACH_PRIMITIVE_TYPE_HANDLER \
    F(uint8_t, Type::getInt8Ty(type->getContext())) \
    F(int8_t, Type::getInt8Ty(type->getContext())) \
    F(uint16_t, Type::getInt16Ty(type->getContext())) \
    F(int16_t, Type::getInt16Ty(type->getContext())) \
    F(uint32_t, Type::getInt32Ty(type->getContext())) \
    F(int32_t, Type::getInt32Ty(type->getContext())) \
    F(uint64_t, Type::getInt64Ty(type->getContext())) \
    F(int64_t, Type::getInt64Ty(type->getContext())) \
    F(bool, Type::getInt1Ty(type->getContext())) \
    F(float, Type::getFloatTy(type->getContext())) \
    F(double, Type::getDoubleTy(type->getContext())) \
    F(void, Type::getVoidTy(type->getContext()))

#define F(t, expr)                              \
template<> struct validate_type<t>              \
{                                               \
    static bool WARN_UNUSED Check(Type* type)   \
    {                                           \
        CHECK(type == (expr));                  \
        return true;                            \
    }                                           \
};
FOR_EACH_PRIMITIVE_TYPE_HANDLER
#undef F
#undef FOR_EACH_PRIMITIVE_TYPE_HANDLER

template<> struct validate_type<bool*>
{
    static bool WARN_UNUSED Check(Type* type)
    {
        CHECK(type == Type::getInt8Ty(type->getContext())->getPointerTo());
        return true;
    }
};

template<> struct validate_type<void*>
{
    static bool WARN_UNUSED Check(Type* type)
    {
        CHECK(type == Type::getInt8Ty(type->getContext())->getPointerTo());
        return true;
    }
};

#define F(...)                                                                 \
template<> struct validate_type<__VA_ARGS__>                                   \
{                                                                              \
    static bool WARN_UNUSED Check(Type* type)                                  \
    {                                                                          \
        CHECK(type->isStructTy());                                             \
        StructType* s = dyn_cast<StructType>(type);                            \
        CHECK(s != nullptr && !s->isLiteral() && s->hasName());                \
        uint64_t ord = AstTypeHelper::cpp_type_ordinal<__VA_ARGS__>::ordinal;  \
        CHECK(ord < AstTypeHelper::x_num_cpp_class_types);                     \
        const char* const name = AstTypeHelper::AstCppTypeLLVMTypeName[ord];   \
        CHECK(std::string(name) == s->getName());                              \
        return true;                                                           \
    }                                                                          \
};
FOR_EACH_CPP_CLASS_TYPE
#undef F

template<typename T>
struct validate_type<T*>
{
    static bool WARN_UNUSED Check(Type* type)
    {
        CHECK(type->isPointerTy());
        CHECK(validate_type<T>::Check(type->getPointerElementType()));
        return true;
    }
};

struct validate_bc_code_internal
{
    template<size_t ord>
    static void Check(const char* /*symbolName*/, FunctionType* fnType)
    {
        ReleaseAssert(static_cast<size_t>(fnType->getNumParams()) == ord);
    }

    template<size_t ord, typename Arg, typename... Args>
    static void Check(const char* symbolName, FunctionType* fnType)
    {
        if (!validate_type<Arg>::Check(fnType->getParamType(static_cast<unsigned int>(ord))))
        {
            fprintf(stderr, "[INTERNAL ERROR] Bitcode for symbol '%s': function parameter %d does not have expected type. "
                            "Please report a bug.\n", symbolName, static_cast<int>(ord));
            abort();
        }
        Check<ord + 1, Args...>(symbolName, fnType);
    }
};

// validate_bc_code<R, Args...>::Check(bitcode)
// Check that the function in bitcode stub has prototype which matches the CPP function R(*)(Args...)
//
template<typename R, typename... Args>
struct validate_bc_code
{
    static void Check(const BitcodeData* bitcode)
    {
        ReleaseAssert(bitcode != nullptr);
        SMDiagnostic llvmErr;
        std::unique_ptr<LLVMContext> context(new LLVMContext);
        MemoryBufferRef mb(StringRef(reinterpret_cast<const char*>(bitcode->m_bitcode), bitcode->m_length),
                           StringRef(bitcode->m_symbolName));
        std::unique_ptr<Module> module = parseIR(mb, llvmErr, *context.get());
        if (module == nullptr)
        {
            fprintf(stderr, "[INTERNAL ERROR] Failed to parse IR module for symbol '%s'. Please report a bug.\n",
                    bitcode->m_symbolName);
            abort();
        }
        Function* func = module->getFunction(bitcode->m_symbolName);
        if (func == nullptr)
        {
            fprintf(stderr, "[INTERNAL ERROR] Bitcode for symbol '%s' does not contain the function. Please report a bug.\n",
                    bitcode->m_symbolName);
            abort();
        }
        FunctionType* fnType = func->getFunctionType();
        if (static_cast<size_t>(fnType->getNumParams()) != sizeof...(Args))
        {
            fprintf(stderr, "[INTERNAL ERROR] Bitcode for symbol '%s': function has wrong number of parameters, "
                            "expected %d, got %d. Please report a bug.\n",
                    bitcode->m_symbolName, static_cast<int>(sizeof...(Args)), static_cast<int>(fnType->getNumParams()));
            abort();
        }
        if (!validate_type<R>::Check(fnType->getReturnType()))
        {
            fprintf(stderr, "[INTERNAL ERROR] Bitcode for symbol '%s': function return type does not have expected type. "
                            "Please report a bug.\n", bitcode->m_symbolName);
            abort();
        }
        validate_bc_code_internal::Check<0, Args...>(bitcode->m_symbolName, fnType);
    }
};

// Implementation generated by dump_symbols.cpp
//
void ValidateAllBitcodeFnPrototypes();

}   // namespace PochiVM
