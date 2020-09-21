#include "check_file_md5.h"

// ****WARNING****
// If you changed this file, make sure you rebuild the whole project from scratch
// (delete 'build' folder and rebuild)
//

int main(int argc, char** argv)
{
    using namespace PochiVMHelper;

    // Params (in order):
    // [dump_symbols]
    // [update_symbol_matches]
    // [build_runtime_lib]
    // [generated_file_dir]
    // [tmp_file_dir]
    // [llc optimization level]
    // [runtime_lib_output]
    // [bc_files]
    // [pochivm_register_runtime bc file]
    //
    ReleaseAssert(argc == 10);
    std::string dump_symbols = argv[1];
    std::string update_symbol_matches = argv[2];
    std::string build_runtime_lib = argv[3];
    std::string generated_file_dir = argv[4];
    std::string tmp_file_dir = argv[5];
    std::string llc_opt_level = argv[6];
    std::string runtime_lib_output = argv[7];
    std::string bcfilesArg = argv[8];
    std::string pochivmBcFile = argv[9];

    ReleaseAssert(llc_opt_level.length() == 1);
    ReleaseAssert('0' <= llc_opt_level[0] && llc_opt_level[0] <= '3');

    // the obj files are actually .bc file despite CMake named them .o
    //
    std::vector<std::string> allBitcodefiles;
    {
        size_t curPos = 0;
        while (true)
        {
            size_t nextPos = bcfilesArg.find(";", curPos);
            if (nextPos == std::string::npos)
            {
                ReleaseAssert(curPos < bcfilesArg.length());
                allBitcodefiles.push_back(bcfilesArg.substr(curPos));
                break;
            }
            ReleaseAssert(curPos < nextPos);
            allBitcodefiles.push_back(bcfilesArg.substr(curPos, nextPos - curPos));
            curPos = nextPos + 1;
        }
    }

    ReleaseAssert(pochivmBcFile.find(";") == std::string::npos);
    allBitcodefiles.push_back(pochivmBcFile);

    for (const std::string& bcfile : allBitcodefiles)
    {
        ReleaseAssert(bcfile.find(" ") == std::string::npos);
        ReleaseAssert(bcfile.find("'") == std::string::npos);
        ReleaseAssert(bcfile.find("\"") == std::string::npos);
        ReleaseAssert(bcfile.find(";") == std::string::npos);
    }

    std::vector<std::string> allSymfiles;
    for (const std::string& bcfile: allBitcodefiles)
    {
        allSymfiles.push_back(bcfile + ".syms");
    }

    std::vector<std::string> allRealObjFiles;
    for (const std::string& bcfile: allBitcodefiles)
    {
        allRealObjFiles.push_back(bcfile + ".obj.o");
    }

    std::string all_needed_symbol_filepath = tmp_file_dir + "/__pochivm_all_needed_symbols__.txt";

    // if 'update_symbol_matches' changed, we need to clear all .sym.matches
    // For simplicity we just call dump_symbol to do this (it empties the .syms.matches
    // file corresponding to the input)
    //
    bool isUpdateSymbolMatchesChanged = !CheckMd5Match(update_symbol_matches);

    bool isDumpSymbolChanged = !CheckMd5Match(dump_symbols);

    // for each modified object file, call dump_symbols to update '.sym' and '.sym.matches'
    // The order is a bit tricky:
    //    For dump_symbols, there is a dependency on pochivm_register_runtime.cpp for every other bitcode,
    //    (to canonicalize the other bitcodes' type names), so we need to process pochivm_register_runtime.cpp first.
    //    But for update_symbol_matches, pochivm_register_runtime.cpp depends on every other bitcode
    //    (to get the wrapped functions' implementation), so we need to process pochivm_register_runtime.cpp last.
    //
    // We pushed pochivmBcFile at the end of allBitcodefiles vector. Here we special-case to process pochivmBcFile first.
    //
    std::vector<size_t> processOrder;
    processOrder.push_back(allBitcodefiles.size() - 1);
    for (size_t i = 0; i < allBitcodefiles.size() - 1; i++)
    {
        processOrder.push_back(i);
    }
    bool shouldRebuildLibrary = false;
    for (size_t i : processOrder)
    {
        std::string bcfile = allBitcodefiles[i];
        std::string symfile = allSymfiles[i];
        // important to call CheckMd5Match even if isUpdateSymbolMatchesChanged is true
        // since it computes the md5 checksum file
        //
        bool isUnchanged = CheckMd5Match(bcfile);
        // Always clear the .syms.matches file for pochivm_register_runtime.cpp
        // The file contains the wrappers for the impl, and if the impl changed,
        // the header files need to be regenerated. Of course there are better
        // ways so we only re-generate headers for the modified wrappers, but for now go simple
        //
        bool isPochiVMObj = (i == allBitcodefiles.size() - 1);
        if (!isUnchanged || isPochiVMObj || isUpdateSymbolMatchesChanged || isDumpSymbolChanged)
        {
            std::string cmd;
            if (!isPochiVMObj)
            {
                cmd = dump_symbols + " --dump " + bcfile + " " + symfile + " " + pochivmBcFile;
            }
            else
            {
                cmd = dump_symbols + " --dump-list " + bcfile + " " + symfile
                      + " " + all_needed_symbol_filepath + " " + generated_file_dir;
            }
            int r = system(cmd.c_str());
            if (r != 0)
            {
                fprintf(stderr, "Command '%s' failed with return value %d\n", cmd.c_str(), r);
                abort();
            }
        }
        // Invoke llc to create the real object file
        //
        if (!isUnchanged)
        {
            shouldRebuildLibrary = true;
            std::string realObjectFile = allRealObjFiles[i];
            std::string cmd;
            cmd = "llc -O=" + llc_opt_level + " -filetype=obj " + bcfile + " -o " + realObjectFile;
            int r = system(cmd.c_str());
            if (r != 0)
            {
                fprintf(stderr, "Command '%s' failed with return value %d\n", cmd.c_str(), r);
                abort();
            }
        }
        if (!isUnchanged)
        {
            UpdateMd5Checksum(bcfile);
        }
    }

    if (isDumpSymbolChanged)
    {
        UpdateMd5Checksum(dump_symbols);
    }

    // call build_runtime_lib to generate the header files
    //
    {
        std::string cmd = build_runtime_lib + " " + update_symbol_matches + " "
                          + all_needed_symbol_filepath + " " + generated_file_dir;
        for (const std::string& symfile : allSymfiles)
        {
            cmd += std::string(" ") + symfile;
        }

        int r = system(cmd.c_str());
        if (r != 0)
        {
            fprintf(stderr, "Command '%s' failed with return value %d\n", cmd.c_str(), r);
            abort();
        }
    }

    if (isUpdateSymbolMatchesChanged)
    {
        UpdateMd5Checksum(update_symbol_matches);
    }

    if (shouldRebuildLibrary)
    {
        std::string tmp_file = runtime_lib_output + ".tmp.a";
        int r = unlink(tmp_file.c_str());
        int err = errno;
        ReleaseAssert(r == 0 || r == -1);
        if (r == -1)
        {
            if (err != ENOENT)
            {
                fprintf(stderr, "Failed to delete file %s, errno = %d (%s)\n",
                        tmp_file.c_str(), err, strerror(err));
                abort();
            }
        }

        {
            struct stat st;
            ReleaseAssert(stat(tmp_file.c_str(), &st) == -1);
            ReleaseAssert(errno == ENOENT);
        }

        std::string cmd = "ar qc " + tmp_file;
        for (const std::string& realObjFile : allRealObjFiles)
        {
            // make sure file exists
            //
            {
                struct stat st;
                if (stat(realObjFile.c_str(), &st) != 0)
                {
                    fprintf(stderr, "Failed to access file '%s', errno = %d (%s)\n",
                            realObjFile.c_str(), errno, strerror(errno));
                    abort();
                }
            }

            cmd += std::string(" ") + realObjFile;
        }
        r = system(cmd.c_str());
        if (r != 0)
        {
            fprintf(stderr, "Command '%s' failed with return value %d\n", cmd.c_str(), r);
            abort();
        }

        r = rename(tmp_file.c_str(), runtime_lib_output.c_str());
        ReleaseAssert(r == 0 || r == -1);
        if (r == -1)
        {
            fprintf(stderr, "Failed to rename file '%s' into '%s', errno = %d (%s)\n",
                    tmp_file.c_str(), runtime_lib_output.c_str(), errno, strerror(errno));
            abort();
        }
    }
    return 0;
}
