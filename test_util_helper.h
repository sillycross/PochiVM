#pragma once

#include "src/common.h"
#include "gtest/gtest.h"

// Whether we are in update-expected-output mode
//
extern bool g_is_update_expected_mode;

// a bunch of utility helpers for tests

// return if string 'a' starts with 'b'
//
inline bool WARN_UNUSED StartsWith(const char* a, const char* b)
{
    size_t la = strlen(a);
    size_t lb = strlen(b);
    CHECK(la >= lb);
    return strncmp(a, b, lb) == 0;
}

namespace {

const char* const x_expected_output_dir = "test_expected_output";

std::string inline GetExpectedOutputFileName()
{
    const char* namePart1 = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    const char* namePart2 = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    return std::string(x_expected_output_dir) + std::string("/") +
           std::string(namePart1) + std::string(".") + std::string(namePart2) + ".expected";
}

void inline CreateExpectedOutputFolderIfNeeded()
{
    struct stat st;
    if (stat(x_expected_output_dir, &st) == -1)
    {
        int ret = mkdir(x_expected_output_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (ret != 0)
        {
            printf("Creating expected output file directory failed with errno=%d (%s)\n", errno, strerror(errno));
            ReleaseAssert(false);
        }
    }
    ReleaseAssert(stat(x_expected_output_dir, &st) == 0);
}

size_t inline GetFileSize(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0)
    {
        return static_cast<size_t>(st.st_size);
    }
    else
    {
        printf("Failed to access expected output file %s, errno=%d (%s)\n", filename, errno, strerror(errno));
        ReleaseAssert(false);
    }
}

inline void TrimTrailingWhitespaces(std::string& s, const char* t = " \t\n\r\f\v")
{
    size_t pos = s.find_last_not_of(t);
    if (pos == std::string::npos)
    {
        s = "";
    }
    else
    {
        s.erase(pos + 1);
    }
}

__attribute__((noreturn))
inline void DumpExpectedAndActualAndFail(const std::string& actual, const std::string& expected)
{
    printf("!!! TEST FAILED !!!\n");
    printf("=============== Expected Output ===============\n");
    for (size_t i = 0; i < expected.length(); i++)
    {
        printf("%c", expected[i]);
    }
    printf("\n");
    printf("================ Actual Output ================\n");
    for (size_t i = 0; i < actual.length(); i++)
    {
        printf("%c", actual[i]);
    }
    printf("\n");
    printf("===============================================\n");
    ReleaseAssert(false);
}

}   // anonymous namespace

// Check the output against the expected output file of this test
//
inline void AssertIsExpectedOutput(std::string out)
{
    if (g_is_update_expected_mode)
    {
        CreateExpectedOutputFolderIfNeeded();
        std::string filename = GetExpectedOutputFileName();
        FILE* pFile = fopen(filename.c_str(), "w");
        if (pFile == nullptr)
        {
            printf("Failed to open file %s for write, errno=%d (%s)\n", filename.c_str(), errno, strerror(errno));
            ReleaseAssert(false);
        }
        ReleaseAssert(fwrite(out.data(), sizeof(char), out.length(), pFile) == out.length());
        fclose(pFile);
        printf("[UPDATE_EXPECTED_OUTPUT] Succesfully updated expected output file.\n");
    }
    else
    {
        std::string filename = GetExpectedOutputFileName();
        FILE* pFile = fopen(filename.c_str(), "r");
        if (pFile == nullptr)
        {
            printf("Failed to open expected output file %s, errno=%d (%s)\n", filename.c_str(), errno, strerror(errno));
            ReleaseAssert(false);
        }
        Auto(fclose(pFile));
        size_t fileSize = GetFileSize(filename.c_str());
        std::string expected(fileSize, ' ');
        ReleaseAssert(fread(expected.data(), sizeof(char), fileSize, pFile) == fileSize);

        TrimTrailingWhitespaces(expected);
        TrimTrailingWhitespaces(out);

        if (expected != out)
        {
            DumpExpectedAndActualAndFail(out, expected);
        }
    }
}
