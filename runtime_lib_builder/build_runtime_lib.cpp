#include "runtime_lib_builder/symbol_list_util.h"

int main(int argc, char** argv)
{
    // [update_sym_matches bin path] [all_needed_symbols] [generated_file_folder] [syms...]
    //
    ReleaseAssert(argc >= 5);

    std::string matcherBinPath = argv[1];
    std::string neededSymbolFile = argv[2];
    std::string generatedFileFolder = argv[3];

    std::vector<std::string> files;
    for (int i = 4; i < argc; i++)
    {
        std::string symFile = argv[i];
        ReleaseAssert(symFile.length() > 5);
        ReleaseAssert(symFile.substr(symFile.length() - 5, 5) == ".syms");
        std::string filenameBase = symFile.substr(0, symFile.length() - 5);
        files.push_back(filenameBase);
    }

    std::set<std::string> remainingSymbols = ReadSymbolListFileOrDie(neededSymbolFile);
    std::string remainingSymbolFile = neededSymbolFile + ".remaining";

    // First of all, go through each .syms.matches file and remove those matches from list
    // Since we clear the match file whenever the .bc file changes, the matches must be up-to-date.
    //
    for (const std::string& filenameBase : files)
    {
        std::string matchFile = filenameBase + ".syms.matches";
        // The file may be empty (which is allowed by ReadSymbolListFileOrDie), but must exist
        //
        std::set<std::string> matches = ReadSymbolListFileOrDie(matchFile);
        for (const std::string& symbol : matches)
        {
            // It's possible that 'symbol' is not in 'remainingSymbols',
            // for example, if we removed entries in pochivm_register_runtime.cpp.
            // But it's fine since by how we set up the build system any match is always up-to-date.
            //
            if (remainingSymbols.count(symbol))
            {
                remainingSymbols.erase(remainingSymbols.find(symbol));
            }
        }
    }

    // Now, go through each file and find new matches
    //
    for (const std::string& filenameBase : files)
    {
        if (remainingSymbols.size() == 0)
        {
            break;
        }

        WriteSymbolListFileOrDie(remainingSymbolFile, remainingSymbols);

        std::string cmd = matcherBinPath + " " + remainingSymbolFile + " " + filenameBase + " " + generatedFileFolder;
        int r = system(cmd.c_str());
        if (r != 0)
        {
            fprintf(stderr, "[ERROR] Command '%s' failed with return value %d\n",
                    cmd.c_str(), r);
            abort();
        }

        // remove newly found matches from list
        //
        std::string matchFile = filenameBase + ".syms.matches";
        std::set<std::string> matches = ReadSymbolListFileOrDie(matchFile);
        for (const std::string& symbol : matches)
        {
            if (remainingSymbols.count(symbol))
            {
                remainingSymbols.erase(remainingSymbols.find(symbol));
            }
        }
    }

    if (remainingSymbols.size() > 0)
    {
        fprintf(stderr, "[ERROR] The following %d symbols cannot be found in object files.\n",
                static_cast<int>(remainingSymbols.size()));
        fprintf(stderr, "A few possible reasons:\n");
        fprintf(stderr, "  (1) Its implementation is missing.\n");
        fprintf(stderr, "  (2) The CPP file containing its implementation is not listed in runtime/CMakeLists.txt. "
                        "Add the CPP file to CMakeLists.txt to fix the issue.\n");
        fprintf(stderr, "  (3) The implementation has internal linkage type. Remove the 'static' keyword to fix the issue.\n");
        fprintf(stderr, "List of missing symbols%s: (use 'c++filt -n [symbol]' to demangle)\n",
                (remainingSymbols.size() > 20 ? " [up to 20]" : ""));
        int cnt = 0;
        for (const std::string& symbol : remainingSymbols)
        {
            fprintf(stderr, "    %s\n", symbol.c_str());
            cnt++;
            if (cnt > 20) { break; }
        }
        abort();
    }

    // Generate runtime library cpp file
    //
    std::string cppFilename = generatedFileFolder + "/pochivm_runtime_library.generated.cpp";
    std::string cppFilenameTmp = cppFilename + ".tmp";
    {

        FILE* fp = fopen(cppFilenameTmp.c_str(), "w");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    cppFilenameTmp.c_str(), errno, strerror(errno));
            abort();
        }

        fprintf(fp, "// GENERATED FILE, DO NOT EDIT!\n//\n\n");

        std::set<std::string> allSymbols = ReadSymbolListFileOrDie(neededSymbolFile);
        for (const std::string& symbol : allSymbols)
        {
            std::string uniqueSymbolHash = GetUniqueSymbolHash(symbol);
            std::string headerFileName = std::string("bc.") + uniqueSymbolHash + ".data.h";
            {
                // Just to make sure the file exists
                //
                std::string realPath = generatedFileFolder + "/" + headerFileName;
                struct stat st;
                if (stat(realPath.c_str(), &st) != 0)
                {
                    fprintf(stderr, "[INTERNAL ERROR] Failed to access file '%s', errno = %d (%s)\n",
                            realPath.c_str(), errno, strerror(errno));
                    abort();
                }
            }
            fprintf(fp, "#include \"%s\"\n", headerFileName.c_str());
        }
        fprintf(fp, "\n");

        fclose(fp);
    }

    {
        std::string headerFilename = generatedFileFolder + "/pochivm_runtime_library_bitcodes.generated.h";
        FILE* fp = fopen(headerFilename.c_str(), "w");
        if (fp == nullptr)
        {
            fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n",
                    headerFilename.c_str(), errno, strerror(errno));
            abort();
        }

        fprintf(fp, "// GENERATED FILE, DO NOT EDIT!\n//\n\n");

        fprintf(fp, "#include \"src/bitcode_data.h\"\n\n");

        fprintf(fp, "namespace PochiVM {\n\n");

        std::set<std::string> allSymbols = ReadSymbolListFileOrDie(neededSymbolFile);
        for (const std::string& symbol : allSymbols)
        {
            std::string uniqueSymbolHash = GetUniqueSymbolHash(symbol);
            std::string varname = std::string("__pochivm_internal_bc_") + uniqueSymbolHash;
            fprintf(fp, "// Symbol: %s\n", symbol.c_str());
            fprintf(fp, "extern const BitcodeData %s;\n\n", varname.c_str());
        }

        fprintf(fp, "} // namespace PochiVM\n\n");

        fclose(fp);
    }

    // Give all-or-nothing guarantee by rename
    //
    int r = rename(cppFilenameTmp.c_str(), cppFilename.c_str());
    ReleaseAssert(r == 0 || r == -1);
    if (r == -1)
    {
        fprintf(stderr, "Failed to rename file '%s' into '%s', errno = %d (%s)\n",
                cppFilenameTmp.c_str(), cppFilename.c_str(), errno, strerror(errno));
        abort();
    }

    return 0;
}
