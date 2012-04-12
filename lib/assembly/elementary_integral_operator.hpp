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

#ifndef bempp_elementary_integral_operator_hpp
#define bempp_elementary_integral_operator_hpp

#include "elementary_linear_operator.hpp"
#include "../common/multidimensional_arrays.hpp"
#include "../common/types.hpp"
#include "../fiber/kernel.hpp"
#include "../fiber/types.hpp"

#include <vector>
#include <armadillo>

namespace Fiber
{

template <typename ValueType> class Expression;
template <typename ValueType> class LocalAssemblerForOperators;
template <typename ValueType> class EvaluatorForIntegralOperators;

} // namespace Fiber

namespace Bempp
{

class EvaluationOptions;
template <typename ValueType> class GridFunction;
template <typename ValueType> class InterpolatedFunction;
template <typename ValueType> class WeakFormAcaAssemblyHelper;

template <typename ValueType>
class ElementaryIntegralOperator : public ElementaryLinearOperator<ValueType>
{
public:
    typedef typename ElementaryLinearOperator<ValueType>::LocalAssemblerFactory
    LocalAssemblerFactory;
    typedef typename ElementaryLinearOperator<ValueType>::LocalAssembler LocalAssembler;
    typedef Fiber::EvaluatorForIntegralOperators<ValueType> Evaluator;

    virtual int trialComponentCount() const {
        return kernel().domainDimension();
    }

    virtual int testComponentCount() const {
        return kernel().codomainDimension();
    }

    virtual bool supportsRepresentation(AssemblyOptions::Representation repr) const;

    virtual bool isRegular() const = 0;

    virtual std::auto_ptr<LocalAssembler> makeAssembler(
            const LocalAssemblerFactory& assemblerFactory,
            const GeometryFactory& geometryFactory,
            const Fiber::RawGridGeometry<ValueType>& rawGeometry,
            const std::vector<const Fiber::Basis<ValueType>*>& testBases,
            const std::vector<const Fiber::Basis<ValueType>*>& trialBases,
            const Fiber::OpenClHandler<ValueType, int>& openClHandler,
            bool cacheSingularIntegrals) const;

    virtual std::auto_ptr<DiscreteLinearOperator<ValueType> >
    assembleWeakForm(
            const Space<ValueType>& testSpace,
            const Space<ValueType>& trialSpace,
            const LocalAssemblerFactory& factory,
            const AssemblyOptions& options) const;

    virtual std::auto_ptr<DiscreteLinearOperator<ValueType> >
    assembleWeakFormInternal(
            const Space<ValueType>& testSpace,
            const Space<ValueType>& trialSpace,
            LocalAssembler& assembler,
            const AssemblyOptions& options) const;

    // We might define a superclass IntegralOperator that might represent
    // a superposition of elementary linear operators (defined at points
    // off surface). Then the virtual attribute here would be useful.
    virtual std::auto_ptr<InterpolatedFunction<ValueType> > applyOffSurface(
            const GridFunction<ValueType>& argument,
            const Grid& evaluationGrid,
            const LocalAssemblerFactory& factory,
            const EvaluationOptions& options) const;

    // TODO: probably assembleOperator() should be replaced with applyOnSurface(),
    // defined for *any* operator (including Id).

private:
    virtual const Fiber::Kernel<ValueType>& kernel() const = 0;
    virtual const Fiber::Expression<ValueType>& testExpression() const = 0;
    virtual const Fiber::Expression<ValueType>& trialExpression() const = 0;

    /** @}
        \name Weak form assembly
        @{ */
    std::auto_ptr<DiscreteLinearOperator<ValueType> >
    assembleWeakFormInDenseMode(
            const Space<ValueType>& testSpace,
            const Space<ValueType>& trialSpace,
            LocalAssembler& assembler,
            const AssemblyOptions &options) const;
    std::auto_ptr<DiscreteLinearOperator<ValueType> >
    assembleWeakFormInAcaMode(
            const Space<ValueType>& testSpace,
            const Space<ValueType>& trialSpace,
            LocalAssembler& assembler,
            const AssemblyOptions& options) const;
    /** @} */

    std::auto_ptr<InterpolatedFunction<ValueType> >
    applyOffSurfaceWithKnownEvaluator(
            const Grid& evaluationGrid,
            const Evaluator& evaluator,
            const EvaluationOptions& options) const;
};

} // namespace Bempp

#endif
