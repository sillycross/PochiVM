#include "gtest/gtest.h"

#include "fastinterp/metavar.h"
#include "pochivm.h"

using namespace PochiVM;

namespace TestMetaVar
{

enum class TestEnum1
{
    EV1,
    EV2,
    EV3,
    X_END_OF_ENUM
};

enum class TestEnum2
{
    EW1,
    EW2,
    X_END_OF_ENUM
};

struct Materializer1
{
    template<typename T1, typename T2, TestEnum1 v1, TestEnum2 v2, bool v3>
    static constexpr bool cond()
    {
        if (v1 == TestEnum1::EV1 && v2 == TestEnum2::EW2) { return false; }
        if (!std::is_same<T1, bool*>::value && !std::is_same<T1, int*>::value) { return false; }
        if (!std::is_same<T2, int>::value && !std::is_same<T2, uint64_t>::value) { return false; }
        return true;
    }

    template<typename T1, typename T2, TestEnum1 v1, TestEnum2 v2, bool v3>
    static void f(std::pair<T1, std::pair<T2, uint64_t>>* a) noexcept
    {
        a->second.second = static_cast<uint64_t>(*a->first) + static_cast<uint64_t>(a->second.first) +
                static_cast<uint64_t>(v1) + static_cast<uint64_t>(v2) + static_cast<uint64_t>(v3);
    }
};

struct Materializer2
{
    template<typename T1>
    static constexpr bool cond()
    {
        return true;
    }

    template<typename T1>
    static void f(T1*) noexcept
    { }
};

}   // namespace TestMetaVar

using namespace TestMetaVar;

TEST(MetaVarSanity, Test1)
{
    MetaVarMaterializedList list = CreateMetaVarList(
                CreateTypeMetaVar("a"),
                CreateTypeMetaVar("b"),
                CreateEnumMetaVar<TestEnum1::X_END_OF_ENUM>("c"),
                CreateEnumMetaVar<TestEnum2::X_END_OF_ENUM>("d"),
                CreateBoolMetaVar("e")
    ).Materialize<Materializer1>();

    ReleaseAssert(list.m_metavars.size() == 5);
    ReleaseAssert(list.m_metavars[0].m_name == std::string("a"));
    ReleaseAssert(list.m_metavars[0].m_type == MetaVarType::PRIMITIVE_TYPE);
    ReleaseAssert(list.m_metavars[1].m_name == std::string("b"));
    ReleaseAssert(list.m_metavars[1].m_type == MetaVarType::PRIMITIVE_TYPE);
    ReleaseAssert(list.m_metavars[2].m_name == std::string("c"));
    ReleaseAssert(list.m_metavars[2].m_type == MetaVarType::ENUM);
    ReleaseAssert(list.m_metavars[2].m_enum_typename == std::string("const char *__pochivm_stringify_type__() [T = TestMetaVar::TestEnum1]"));
    ReleaseAssert(list.m_metavars[2].m_enum_bound == 3);
    ReleaseAssert(list.m_metavars[3].m_name == std::string("d"));
    ReleaseAssert(list.m_metavars[3].m_type == MetaVarType::ENUM);
    ReleaseAssert(list.m_metavars[3].m_enum_typename == std::string("const char *__pochivm_stringify_type__() [T = TestMetaVar::TestEnum2]"));
    ReleaseAssert(list.m_metavars[3].m_enum_bound == 2);
    ReleaseAssert(list.m_metavars[4].m_name == std::string("e"));
    ReleaseAssert(list.m_metavars[4].m_type == MetaVarType::BOOL);

    ReleaseAssert(list.m_instances.size() == 5 * 4 * 2);
    {
        size_t cnt = 0;
        for (int i1 = 0; i1 < 2; i1++)
        {
            for (int i2 = 0; i2 < 2; i2++)
            {
                for (int i3 = 0; i3 < 3; i3++)
                {
                    for (int i4 = 0; i4 < 2; i4++)
                    {
                        if (i3 == 0 && i4 == 1) continue;
                        for (int i5 = 0; i5 < 2; i5++)
                        {
                            ReleaseAssert(list.m_instances[cnt].m_values.size() == 5);
                            if (i1 == 0)
                            {
                                ReleaseAssert(list.m_instances[cnt].m_values[0] == TypeId::Get<bool*>().value);
                            }
                            else
                            {
                                ReleaseAssert(list.m_instances[cnt].m_values[0] == TypeId::Get<int*>().value);
                            }
                            if (i2 == 0)
                            {
                                ReleaseAssert(list.m_instances[cnt].m_values[1] == TypeId::Get<int>().value);
                            }
                            else
                            {
                                ReleaseAssert(list.m_instances[cnt].m_values[1] == TypeId::Get<uint64_t>().value);
                            }
                            ReleaseAssert(list.m_instances[cnt].m_values[2] == static_cast<uint64_t>(i3));
                            ReleaseAssert(list.m_instances[cnt].m_values[3] == static_cast<uint64_t>(i4));
                            ReleaseAssert(list.m_instances[cnt].m_values[4] == static_cast<uint64_t>(i5));
                            cnt++;
                        }
                    }
                }
            }
        }
        ReleaseAssert(cnt == 40);
    }

    ReleaseAssert(list.m_instances[0].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV1, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[1].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV1, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[2].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV2, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[3].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV2, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[4].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV2, TestEnum2::EW2, false>));
    ReleaseAssert(list.m_instances[5].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV2, TestEnum2::EW2, true>));
    ReleaseAssert(list.m_instances[6].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV3, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[7].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV3, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[8].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV3, TestEnum2::EW2, false>));
    ReleaseAssert(list.m_instances[9].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, int, TestEnum1::EV3, TestEnum2::EW2, true>));

    ReleaseAssert(list.m_instances[10].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV1, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[11].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV1, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[12].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV2, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[13].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV2, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[14].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV2, TestEnum2::EW2, false>));
    ReleaseAssert(list.m_instances[15].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV2, TestEnum2::EW2, true>));
    ReleaseAssert(list.m_instances[16].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV3, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[17].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV3, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[18].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV3, TestEnum2::EW2, false>));
    ReleaseAssert(list.m_instances[19].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<bool*, uint64_t, TestEnum1::EV3, TestEnum2::EW2, true>));

    ReleaseAssert(list.m_instances[20].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV1, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[21].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV1, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[22].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV2, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[23].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV2, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[24].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV2, TestEnum2::EW2, false>));
    ReleaseAssert(list.m_instances[25].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV2, TestEnum2::EW2, true>));
    ReleaseAssert(list.m_instances[26].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV3, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[27].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV3, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[28].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV3, TestEnum2::EW2, false>));
    ReleaseAssert(list.m_instances[29].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, int, TestEnum1::EV3, TestEnum2::EW2, true>));

    ReleaseAssert(list.m_instances[30].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV1, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[31].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV1, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[32].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV2, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[33].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV2, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[34].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV2, TestEnum2::EW2, false>));
    ReleaseAssert(list.m_instances[35].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV2, TestEnum2::EW2, true>));
    ReleaseAssert(list.m_instances[36].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV3, TestEnum2::EW1, false>));
    ReleaseAssert(list.m_instances[37].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV3, TestEnum2::EW1, true>));
    ReleaseAssert(list.m_instances[38].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV3, TestEnum2::EW2, false>));
    ReleaseAssert(list.m_instances[39].m_fnPtr == reinterpret_cast<void*>(Materializer1::f<int*, uint64_t, TestEnum1::EV3, TestEnum2::EW2, true>));
}

TEST(MetaVarSanity, Test2)
{
    MetaVarMaterializedList list = CreateMetaVarList(
                CreateTypeMetaVar("a")
    ).Materialize<Materializer2>();

    ReleaseAssert(list.m_metavars.size() == 1);
    ReleaseAssert(list.m_metavars[0].m_name == std::string("a"));
    ReleaseAssert(list.m_metavars[0].m_type == MetaVarType::PRIMITIVE_TYPE);

    ReleaseAssert(list.m_instances.size() == x_num_primitive_types * 2 + 3);
    for (size_t i = 0; i < x_num_primitive_types * 2 + 3; i++)
    {
        size_t numPtr = i / (x_num_primitive_types + 1);
        size_t v = i % (x_num_primitive_types + 1);
        TypeId x; x.value = v;
        while (numPtr > 0) { x = x.AddPointer(); numPtr--; }
        ReleaseAssert(list.m_instances[i].m_values.size() == 1);
        ReleaseAssert(list.m_instances[i].m_values[0] == x.value);
    }
}
