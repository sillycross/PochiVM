#pragma once

namespace llvm { namespace orc {
class LLJIT;
} }

// LLVM is compiled with -fno-rtti by default, while our code is compiled with rtti
// The fake symbol resolver has to inherit a LLVM class,
// so we have to put it in a separate CPP file and compile it with -fno-rtti,
// and only expose the interface here.
//
void AddFakeSymbolResolverGenerator(llvm::orc::LLJIT* jit);
