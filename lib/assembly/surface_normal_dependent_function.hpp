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

#ifndef bempp_surface_normal_dependent_function_hpp
#define bempp_surface_normal_dependent_function_hpp

#include "../common/common.hpp"

#include "../fiber/surface_normal_dependent_function.hpp"

namespace Bempp
{

using Fiber::SurfaceNormalDependentFunction;

/** \ingroup assembly_functions
 *  \brief Construct a SurfaceNormalDependentFunction object from a given functor.
 *
 *  This helper function takes an instance \p functor of a class \p Functor
 *  providing an interface described in the documentation of
 *  SurfaceNormalDependentFunction and uses it to construct a
 *  SurfaceNormalDependentFunction object. The latter can subsequently be
 *  passed into a constructor of the GridFunction class. */
template <typename Functor>
inline SurfaceNormalDependentFunction<Functor> surfaceNormalDependentFunction(
        const Functor& functor)
{
    return SurfaceNormalDependentFunction<Functor>(functor);
}


} // namespace Bempp

#endif
