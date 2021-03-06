// Copyright (C) 2011-2012 by the BEM++ Authors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "bempp/common/config_ahmed.hpp"
#ifdef WITH_AHMED

#include "ahmed_mblock_array_deleter.hpp"
#include "ahmed_aux.hpp"

#include "../fiber/explicit_instantiation.hpp"

namespace Bempp
{

template <typename ValueType>
void AhmedMblockArrayDeleter::operator () (mblock<ValueType>** blocks) const
{
    freembls(m_arraySize, blocks);
}

#define INSTANTIATE_OPERATOR_CALL(RESULT) \
    template void AhmedMblockArrayDeleter::operator () ( \
        mblock<AhmedTypeTraits<RESULT>::Type>** blocks) const

FIBER_ITERATE_OVER_VALUE_TYPES(INSTANTIATE_OPERATOR_CALL);

} // namespace Bempp

#endif // WITH_AHMED
