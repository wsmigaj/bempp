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

#include "helmholtz_3d_hypersingular_boundary_operator.hpp"
#include "helmholtz_3d_boundary_operator_base_imp.hpp"

#include "../fiber/explicit_instantiation.hpp"

#include "../fiber/modified_helmholtz_3d_hypersingular_kernel_functor.hpp"
#include "../fiber/modified_helmholtz_3d_hypersingular_kernel_interpolated_functor.hpp"
#include "../fiber/modified_helmholtz_3d_hypersingular_transformation_functor.hpp"
#include "../fiber/modified_helmholtz_3d_hypersingular_integrand_functor_2.hpp"

#include "../fiber/default_collection_of_kernels.hpp"
#include "../fiber/default_collection_of_basis_transformations.hpp"
#include "../fiber/default_test_kernel_trial_integral.hpp"

#include "../common/boost_make_shared_fwd.hpp"

//    m_expressionList.addTerm(m_surfaceCurl);
//    m_expressionList.addTerm(m_valueTimesNormal, KernelType(0., 1.) * waveNumber);

namespace Bempp
{

/** \cond PRIVATE */
template <typename BasisFunctionType>
struct Helmholtz3dHypersingularBoundaryOperatorImpl
{
    typedef Helmholtz3dHypersingularBoundaryOperatorImpl<BasisFunctionType> This;
    typedef Helmholtz3dBoundaryOperatorBase<This, BasisFunctionType> BoundaryOperatorBase;
    typedef typename BoundaryOperatorBase::CoordinateType CoordinateType;
    typedef typename BoundaryOperatorBase::KernelType KernelType;
    typedef typename BoundaryOperatorBase::ResultType ResultType;

    typedef Fiber::ModifiedHelmholtz3dHypersingularKernelFunctor<KernelType>
    NoninterpolatedKernelFunctor;
    typedef Fiber::ModifiedHelmholtz3dHypersingularKernelInterpolatedFunctor<KernelType>
    InterpolatedKernelFunctor;
    typedef Fiber::ModifiedHelmholtz3dHypersingularTransformationFunctor<CoordinateType>
    TransformationFunctor;
    typedef Fiber::ModifiedHelmholtz3dHypersingularIntegrandFunctor2<
    BasisFunctionType, KernelType, ResultType> IntegrandFunctor;

    explicit Helmholtz3dHypersingularBoundaryOperatorImpl(
            KernelType waveNumber_) :
        waveNumber(waveNumber_),
        interpPtsPerWavelength(0),
        maxDistance(0.),
        kernels(new Fiber::DefaultCollectionOfKernels<NoninterpolatedKernelFunctor>(
                    NoninterpolatedKernelFunctor(waveNumber / KernelType(0., 1.)))),
        transformations(TransformationFunctor()),
        integral(IntegrandFunctor())
    {}

    Helmholtz3dHypersingularBoundaryOperatorImpl(
            KernelType waveNumber_,
            CoordinateType maxDistance_,
            int interpPtsPerWavelength_) :
        waveNumber(waveNumber_),
        interpPtsPerWavelength(interpPtsPerWavelength_),
        maxDistance(maxDistance_),
        kernels(new Fiber::DefaultCollectionOfKernels<InterpolatedKernelFunctor>(
                              InterpolatedKernelFunctor(waveNumber_ / KernelType(0., 1.),
                                                        maxDistance_,
                                                        interpPtsPerWavelength_))),
        transformations(TransformationFunctor()),
        integral(IntegrandFunctor())
    {}

    KernelType waveNumber;
    int interpPtsPerWavelength;
    CoordinateType maxDistance;
    boost::shared_ptr<Fiber::CollectionOfKernels<KernelType> > kernels;
    Fiber::DefaultCollectionOfBasisTransformations<TransformationFunctor>
    transformations;
    Fiber::DefaultTestKernelTrialIntegral<IntegrandFunctor> integral;
};
/** \endcond */

template <typename BasisFunctionType>
Helmholtz3dHypersingularBoundaryOperator<BasisFunctionType>::
Helmholtz3dHypersingularBoundaryOperator(
        const shared_ptr<const Space<BasisFunctionType> >& domain,
        const shared_ptr<const Space<BasisFunctionType> >& range,
        const shared_ptr<const Space<BasisFunctionType> >& dualToRange,
        KernelType waveNumber,
        const std::string& label,
        int symmetry,
        bool useInterpolation,
        int interpPtsPerWavelength) :
    Base(domain, range, dualToRange, waveNumber, label, symmetry,
         useInterpolation, interpPtsPerWavelength)
{
}

template <typename BasisFunctionType>
BoundaryOperator<BasisFunctionType,
typename Helmholtz3dHypersingularBoundaryOperator<BasisFunctionType>::ResultType>
helmholtz3dHypersingularBoundaryOperator(
        const shared_ptr<const Context<BasisFunctionType,
        typename Helmholtz3dHypersingularBoundaryOperator<BasisFunctionType>::ResultType> >& context,
        const shared_ptr<const Space<BasisFunctionType> >& domain,
        const shared_ptr<const Space<BasisFunctionType> >& range,
        const shared_ptr<const Space<BasisFunctionType> >& dualToRange,
        typename Helmholtz3dHypersingularBoundaryOperator<BasisFunctionType>::KernelType waveNumber,
        const std::string& label,
        int symmetry,
        bool useInterpolation,
        int interpPtsPerWavelength)
{
    typedef typename Helmholtz3dHypersingularBoundaryOperator<BasisFunctionType>::ResultType ResultType;
    typedef Helmholtz3dHypersingularBoundaryOperator<BasisFunctionType> Op;
    return BoundaryOperator<BasisFunctionType, ResultType>(
                context, boost::make_shared<Op>(domain, range, dualToRange,
                                                waveNumber, label, symmetry,
                                                useInterpolation,
                                                interpPtsPerWavelength));
}

#define INSTANTIATE_NONMEMBER_CONSTRUCTOR(BASIS) \
   template BoundaryOperator<BASIS, Helmholtz3dHypersingularBoundaryOperator<BASIS>::ResultType> \
   helmholtz3dHypersingularBoundaryOperator( \
       const shared_ptr<const Context<BASIS, Helmholtz3dHypersingularBoundaryOperator<BASIS>::ResultType> >&, \
       const shared_ptr<const Space<BASIS> >&, \
       const shared_ptr<const Space<BASIS> >&, \
       const shared_ptr<const Space<BASIS> >&, \
       Helmholtz3dHypersingularBoundaryOperator<BASIS>::KernelType, \
       const std::string&, int, bool, int)
FIBER_ITERATE_OVER_BASIS_TYPES(INSTANTIATE_NONMEMBER_CONSTRUCTOR);

#define INSTANTIATE_BASE(BASIS) \
    template class Helmholtz3dBoundaryOperatorBase< \
    Helmholtz3dHypersingularBoundaryOperatorImpl<BASIS>, BASIS>
FIBER_ITERATE_OVER_BASIS_TYPES(INSTANTIATE_BASE);
FIBER_INSTANTIATE_CLASS_TEMPLATED_ON_BASIS(Helmholtz3dHypersingularBoundaryOperator);

} // namespace Bempp
