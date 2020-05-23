#pragma once

#include "pochivm/common.h"

// LLVM debug metadata info could be really huge.
// This option denotes whether we should keep debug info. For now we just strip them all.
//
const static bool OPTION_KEEP_DEBUG_INFO = false;

// A symbol list file is just in the following straightforward format:
//    First line contains an integer n, denoting the number of symbols.
//    Each of the next n lines contain one symbol name.
//
// Since those files are only used in project build steps, we simply abort if any error happened
//
std::set<std::string> ReadSymbolListFileOrDie(const std::string& filename);

void WriteSymbolListFileOrDie(const std::string& filename,
                              const std::set<std::string>& data);

// Unique hash of symbol
// This is the first 24 byte of its SHA1 hex value, so safe to assume to be collision-less
//
std::string GetUniqueSymbolHash(const std::string& symbol);
