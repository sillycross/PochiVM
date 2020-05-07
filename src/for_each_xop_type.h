#ifndef CLASS
#error CLASS macro not defined!
#define CLASS(...)
#endif
#ifndef MEMBER
#error MEMBER macro not defined!
#define MEMBER(...)
#endif
#ifndef METHOD
#error METHOD macro not defined!
#define METHOD(...)
#endif
#ifndef ENDCLASS
#error ENDCLASS macro not defined!
#define ENDCLASS
#endif

/*
CLASS(test_classA)
    MEMBER(m_v1, int)
    METHOD(Method1, int, PARAM(int, int), OPTION(NOINLINE))
    METHOD(Method2, void, PARAM(int, double), OPTION())
ENDCLASS

CLASS(test_classB)
    MEMBER(m_v2, int)
    METHOD(Method3, int, PARAM(int, int), OPTION(NOINLINE))
    METHOD(Method4, void, PARAM(int, double), OPTION())
ENDCLASS
*/
