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

#ifndef bempp_abstract_boundary_operator_hpp
#define bempp_abstract_boundary_operator_hpp

#include "../common/common.hpp"

#include "assembly_options.hpp"
#include "symmetry.hpp"

#include "../common/scalar_traits.hpp"
#include "../common/shared_ptr.hpp"
#include "../fiber/local_assembler_factory.hpp"
#include "../space/space.hpp"

#include <memory>
#include <string>
#include <vector>

namespace arma
{

template <typename eT> class Mat;

}

namespace Bempp
{

class AbstractBoundaryOperatorId;
class Grid;
class GeometryFactory;
template <typename ValueType> class DiscreteBoundaryOperator;
template <typename BasisFunctionType, typename ResultType> class AbstractBoundaryOperatorSum;
template <typename BasisFunctionType, typename ResultType> class Context;
template <typename BasisFunctionType, typename ResultType> class ElementaryAbstractBoundaryOperator;
template <typename BasisFunctionType, typename ResultType> class GridFunction;
template <typename BasisFunctionType, typename ResultType> class ScaledAbstractBoundaryOperator;

/** \ingroup assembly
 *  \brief Abstract boundary operator.
 *
 *  An AbstractBoundaryOperator represents a linear mapping \f$L : X \to Y\f$ between
 *  two function spaces \f$X : S \to K^p\f$ (_domain_) and \f$Y : S \to K^q\f$ (_range_) defined on
 *  an \f$n\f$-dimensional surface \f$S\f$ embedded in an
 *  \f$(n+1)\f$-dimensional domain. \f$K\f$ denotes either the set of real or
 *  complex numbers.
 *
 *  The functions assembleWeakForm() and assembleDetachedWeakForm() can be used
 *  to calculate the weak form of the operator. The weak form constructed with
 *  the former function is stored internally in the AbstractBoundaryOperator object,
 *  which can subsequently be used as a typical linear operator (i.e. act on
 *  functions defined on the surface \f$S\f$, represented as GridFunction
 *  objects). The weak form constructed with the latter function is not stored
 *  in AbstractBoundaryOperator, but its ownership is passed directly to the caller.
 *
 *  \tparam BasisFunctionType
 *    Type used to represent components of the test and trial functions.
 *  \tparam ResultType
 *    Type used to represent elements of the weak form of this operator.
 *
 *  Both template parameters can take the following values: \c float, \c
 *  double, <tt>std::complex<float></tt> and <tt>std::complex<double></tt>.
 *  Both types must have the same precision: for instance, mixing \c float with
 *  <tt>std::complex<double></tt> is not allowed. If \p BasisFunctionType is
 *  set to a complex type, then \p ResultType must be set to the same type.
 */
template <typename BasisFunctionType_, typename ResultType_>
class AbstractBoundaryOperator
{
public:
    /** \brief Type used to represent components of the test and trial functions. */
    typedef BasisFunctionType_ BasisFunctionType;
    /** \brief Type used to represent elements of the operator's weak form. */
    typedef ResultType_ ResultType;
    /** \brief Type used to represent coordinates. */
    typedef typename ScalarTraits<ResultType>::RealType CoordinateType;
    /** \brief Type of the appropriate instantiation of Fiber::LocalAssemblerFactory. */
    typedef Fiber::LocalAssemblerFactory<BasisFunctionType, ResultType, GeometryFactory>
    LocalAssemblerFactory;

    /** @name Construction and destruction
     *  @{ */

    /** \brief Constructor.
     *
     *  \param[in] domain
     *    Function space being the domain of the operator.
     *
     *  \param[in] range
     *    Function space being the range of the operator.
     *
     *  \param[in] dualToRange
     *    Function space dual to the the range of the operator.
     *
     *  \param[in] label
     *    Textual label of the operator (optional, used for debugging).
     *
     *  The spaces \p range and \p dualToRange must be defined on
     *  the same grid.
     */
    AbstractBoundaryOperator(const shared_ptr<const Space<BasisFunctionType> >& domain,
                             const shared_ptr<const Space<BasisFunctionType> >& range,
                             const shared_ptr<const Space<BasisFunctionType> >& dualToRange,
                             const std::string& label = "",
                             const Symmetry symmetry = NO_SYMMETRY);

    // Default "shallow" copy constructor is used (thus the internal pointer
    // to the weak form is shared among all copies). To make a deep copy of
    // a AbstractBoundaryOperator, use the deepCopy() method (not yet written).
    // AbstractBoundaryOperator(const AbstractBoundaryOperator<BasisFunctionType, ResultType>& other);

    /** \brief Destructor. */
    virtual ~AbstractBoundaryOperator();

    /** \brief Identifier.
     *
     *  If the weak form of this operator is cacheable, return a shared pointer
     *  to a valid instance of a subclass of AbstractBoundaryOperatorId that
     *  is guaranteed to be different for all *logically different* abstract
     *  boundary operators.
     *
     *  If the weak form of this operator is not cacheable, return a null shared
     *  pointer. This is the default implementation.
     */
    virtual shared_ptr<const AbstractBoundaryOperatorId> id() const;

    /** @}
     *  @name Spaces
     *  @{ */

    /** \brief Domain.
     *
     *  Return a reference to the function space being the domain of the operator. */
    shared_ptr<const Space<BasisFunctionType> > domain() const;

    /** \brief Range.
     *
     *  Return a reference to the function space being the range of the operator. */
    shared_ptr<const Space<BasisFunctionType> > range() const;

    /** \brief Dual to range.
     *
     *  Return a reference to the function space dual to the range of the operator. */
    shared_ptr<const Space<BasisFunctionType> > dualToRange() const;

    /** @}
     *  @name Label
     *  @{ */

    /** \brief Return the label of the operator. */
    std::string label() const;

    Symmetry symmetry() const;

    /** @}
     *  @name Assembly
     *  @{ */

    /** \brief Return true if this operator supports the representation \p repr. */
    virtual bool supportsRepresentation(
            AssemblyOptions::Representation repr) const = 0;

    //*  \param[in] force
    //*    If true (default), the weak form will be reassembled even if an older one already exists. If false, any existing a weak form
    /** \brief Assemble and returns the operator's weak form.
     *
     *  This function constructs a discrete linear operator representing the
     *  matrix \f$L_{jk}\f$ with entries of the form
     *
     *  \f[L_{jk} = \int_S \phi_j L \psi_k,\f]
     *
     *  where \f$L\f$ is the linear operator represented by this object,
     *  \f$S\f$ denotes the surface on which the domain function space \f$X\f$
     *  is defined and which is represented by the grid returned by
     *  <tt>domain.grid()</tt>, \f$\phi_j\f$ is a _trial function_ from the
     *  space \f$Y'\f$ dual to the range of the operator, \f$Y\$, and
     *  \f$\psi_k\f$ is a _test function_ from the domain space \f$X\f$.
     */
    shared_ptr<DiscreteBoundaryOperator<ResultType> > assembleWeakForm(
            const Context<BasisFunctionType, ResultType>& context) const;

protected:
    /** @} */

    /** \brief Given an AssemblyOptions object, construct objects necessary for
     *  subsequent local assembler construction. */
    void collectDataForAssemblerConstruction(
            const AssemblyOptions& options,
            shared_ptr<Fiber::RawGridGeometry<CoordinateType> >& testRawGeometry,
            shared_ptr<Fiber::RawGridGeometry<CoordinateType> >& trialRawGeometry,
            shared_ptr<GeometryFactory>& testGeometryFactory,
            shared_ptr<GeometryFactory>& trialGeometryFactory,
            shared_ptr<std::vector<const Fiber::Basis<BasisFunctionType>*> >& testBases,
            shared_ptr<std::vector<const Fiber::Basis<BasisFunctionType>*> >& trialBases,
            shared_ptr<Fiber::OpenClHandler>& openClHandler,
            bool& cacheSingularIntegrals) const;

    // Probably can be made public
    /** \brief Implementation of the weak-form assembly.
     *
     *  Construct a discrete linear operator representing the matrix \f$L_{jk}\f$
     *  described in assembleWeakForm() and return a shared pointer to it.
     */
    virtual shared_ptr<DiscreteBoundaryOperator<ResultType> >
    assembleWeakFormImpl(const Context<BasisFunctionType, ResultType>& context) const = 0;

private:
    shared_ptr<const Space<BasisFunctionType> > m_domain;
    shared_ptr<const Space<BasisFunctionType> > m_range;
    shared_ptr<const Space<BasisFunctionType> > m_dualToRange;
    std::string m_label;
    Symmetry m_symmetry;
};

} // namespace Bempp

#endif