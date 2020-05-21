#include "symbol_list_util.h"
#include "sha1.h"

static bool isWhitespace(char c)
{
    const char* whitespaces = " \t\n\r\f\v";
    for (size_t i = 0; i < strlen(whitespaces); i++)
    {
        if (whitespaces[i] == c)
        {
            return true;
        }
    }
    return false;
}

std::set<std::string> ReadSymbolListFileOrDie(const std::string& filename)
{
    FILE* fp = fopen(filename.c_str(), "r");
    if (fp == nullptr)
    {
        fprintf(stderr, "Failed to open file '%s' for read, errno = %d (%s)\n", filename.c_str(), errno, strerror(errno));
        abort();
    }
    Auto(fclose(fp));

    std::set<std::string> r;
    int n;
    if (fscanf(fp, "%d", &n) != 1)
    {
        ReleaseAssert(feof(fp));
        return r;
    }

    ReleaseAssert(n >= 0);
    char *buf = new char[1000000];
    Auto(delete[] buf);
    for (int i = 0; i < n; i++)
    {
        ReleaseAssert(fscanf(fp, "%s", buf) == 1);
        std::string s = std::string(buf);
        ReleaseAssert(!r.count(s));
        r.insert(s);
    }
    ReleaseAssert(fscanf(fp, "%s", buf) != 1);
    ReleaseAssert(feof(fp));

    return r;
}

void WriteSymbolListFileOrDie(const std::string& filename,
                              const std::set<std::string>& data)
{
    FILE* fp = fopen(filename.c_str(), "w");
    if (fp == nullptr)
    {
        fprintf(stderr, "Failed to open file '%s' for write, errno = %d (%s)\n", filename.c_str(), errno, strerror(errno));
        abort();
    }
    fprintf(fp, "%d\n", static_cast<int>(data.size()));
    for (const std::string& value : data)
    {
        for (size_t i = 0; i < value.size(); i++)
        {
            ReleaseAssert(!isWhitespace(value[i]));
        }
        fprintf(fp, "%s\n", value.c_str());
    }
    fclose(fp);
}

static std::string SHA1HexWrapper(const std::string& value)
{
    char result[21];
    char hexresult[41];

    SHA1(result /*out*/, value.c_str(), static_cast<int>(value.length()));

    for (size_t offset = 0; offset < 20; offset++)
    {
        sprintf((hexresult + (2 * offset)), "%02x", (result[offset] & 0xff));
    }
    hexresult[40] = '\0';

    std::string ret(hexresult);
    return ret;
}

std::string GetUniqueSymbolHash(const std::string& symbol)
{
    std::string r = SHA1HexWrapper(symbol);
    return r.substr(0, 24);
}
