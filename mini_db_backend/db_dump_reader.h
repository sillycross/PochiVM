#pragma once

#include "pochivm/common.h"
#include "base64_util.h"

namespace MiniDbBackend
{

class TpchDbDumpReader
{
public:
    static bool WARN_UNUSED LoadInt32(FILE* fp, int& out /*out*/)
    {
        int num = fscanf(fp, "%d", &out);
        return num == 1;
    }

    static bool WARN_UNUSED LoadUInt32(FILE* fp, uint32_t& out /*out*/)
    {
        int num = fscanf(fp, "%u", &out);
        return num == 1;
    }

    static bool WARN_UNUSED LoadInt64(FILE* fp, int64_t& out /*out*/)
    {
        int num = fscanf(fp, "%ld", &out);
        return num == 1;
    }

    static bool WARN_UNUSED LoadString(FILE* fp, char* out, int bufferSize)
    {
        int num = fscanf(fp, "%s", buf);
        if (num == 0)
        {
            return false;
        }

        std::string_view sv = buf;
        std::string result = base64_decode(sv);

        ReleaseAssert(result.length() < static_cast<size_t>(bufferSize));
        memcpy(out, result.c_str(), result.length());
        out[result.length()] = '\0';
        return true;
    }

    static bool WARN_UNUSED LoadDouble(FILE* fp, double& out /*out*/)
    {
        int num = fscanf(fp, "%lf", &out);
        return num == 1;
    }

private:
    inline static char buf[1000000];
};

}   // namespace MiniDbBackend
