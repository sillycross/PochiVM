#pragma once

#include "pochivm/common.h"

// Parse the result generated from __pochivm_stringify_type__ and __pochivm_stringify_value__
//
class ReflectiveStringifyParser
{
public:
    static std::string ParseTypeName(const char* data)
    {
        static const char* prefix = "const char *__pochivm_stringify_type__() [T = ";
        size_t len = strlen(data);
        size_t prefixLen = strlen(prefix);
        ReleaseAssert(len > prefixLen);
        ReleaseAssert(strncmp(data, prefix, prefixLen) == 0);
        return ParseNameInternal(data + prefixLen);
    }

    static std::string ParseValueName(const char* data)
    {
        static const char* prefix = "const char *__pochivm_stringify_value__() [v = &";
        size_t len = strlen(data);
        size_t prefixLen = strlen(prefix);
        ReleaseAssert(len > prefixLen);
        ReleaseAssert(strncmp(data, prefix, prefixLen) == 0);
        return ParseNameInternal(data + prefixLen);
    }

private:
    static std::string ParseNameInternal(const char* data)
    {
        int bracketDepth = 1;
        size_t i = 0;
        size_t len = strlen(data);
        while (i < len)
        {
            if (data[i] == ']')
            {
                bracketDepth--;
                if (bracketDepth == 0)
                {
                    break;
                }
            }
            else if (data[i] == '[')
            {
                bracketDepth++;
            }
            i++;
        }
        ReleaseAssert(bracketDepth == 0 && i < len);
        return std::string(data, i);
    }
};
