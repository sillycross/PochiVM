#pragma once

#include "common.h"

namespace PochiVM {
namespace AstTypeHelper {

// Get the function address for a class method.
// The returned function address may be used to call the function as if it were a free function.
//
// It only works for non-virtual method and when the adjustment to 'this' pointer is zero.
// If not, an assertion is fired.
//
// From Itanium C++ ABI Section 2.3: [https://itanium-cxx-abi.github.io/cxx-abi/abi.html#member-pointers]
//     A pointer to data member is an offset from the base address of the class object containing it, represented as a ptrdiff_t.
//     It has the size and alignment attributes of a ptrdiff_t. A NULL pointer is represented as -1.
//
//     A pointer to member function is a pair as follows:
//     ptr:
//         For a non-virtual function, this field is a simple function pointer. (Under current base Itanium psABI conventions,
//         that is a pointer to a GP/function address pair.) For a virtual function, it is 1 plus the virtual table offset (in bytes)
//         of the function, represented as a ptrdiff_t. The value zero represents a NULL pointer, independent of the adjustment
//         field value below.
//
//     adj:
//          The required adjustment to this, represented as a ptrdiff_t.
//
//     It has the size, data size, and alignment of a class containing those two members, in that order.
//     (For 64-bit Itanium, that will be 16, 16, and 8 bytes respectively.)
//
// The above specification allows a tricky way to check virtual-ness:
//     If the function is non-virtual, the ptr is function address (which is apparently at least aligned to 2 bytes).
//     Otherwise, the ptr is vtable offset (also apparently at least aligned to 2 bytes) + 1.
//     So we can distinguish between the two cases using the value of 'ptr' modulo 2.
//
template<typename MethPtr>
void* GetClassMethodPtr(MethPtr p, bool fireReleaseAssertOnError = false)
{
    static_assert(sizeof(MethPtr) == sizeof(ptrdiff_t) * 2, "Itanium ABI member function pointer has unexpected size!");
    ptrdiff_t* raw = reinterpret_cast<ptrdiff_t*>(&p);
    if (fireReleaseAssertOnError)
    {
        ReleaseAssert(raw[0] % 2 == 0 && "The member function is virtual!");
        ReleaseAssert(raw[1] == 0 && "The member function has non-zero this-pointer adjustment!");
    }
    else
    {
        TestAssert(raw[0] % 2 == 0 && "The member function is virtual!");
        TestAssert(raw[1] == 0 && "The member function has non-zero this-pointer adjustment!");
    }
    return reinterpret_cast<void*>(raw[0]);
}

}   // namespace AstTypeHelper
}   // namespace PochiVM
