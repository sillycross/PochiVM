#include <cstdio>
#include <cstdlib> 
#include "gtest/gtest.h"

namespace {

void PrintInformation()
{
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
	printf("--------------- General Information ---------------\n");
	printf("Host:         ");
	fflush(stdout);
	std::ignore = system("whoami | tr -d '\\n' && printf '@' && cat /etc/hostname");
	printf("Build flavor: %s\n", TOSTRING(BUILD_FLAVOR));
	printf("---------------------------------------------------\n");
#undef TOSTRING
#undef STRINGIFY
}

}	// annoymous namespace

int main(int argc, char **argv)
{
	PrintInformation();
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

