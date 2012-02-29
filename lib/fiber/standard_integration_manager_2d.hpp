// Copyright (C) 2011-2012 by the Fiber Authors
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

#ifndef fiber_standard_integration_manager_2d_hpp
#define fiber_standard_integration_manager_2d_hpp

#include "integration_manager.hpp"
#include "array_2d.hpp"
#include "element_pair_topology.hpp"
#include "opencl_options.hpp"
#include "separable_numerical_double_integrator.hpp"
#include "nonseparable_numerical_double_integrator.hpp"

// Hyena code
#include "quadrature/galerkinduffy.hpp"
#include "quadrature/quadrature.hpp"

#include <boost/static_assert.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <map>
#include <cstring>
#include <iostream>

namespace Fiber
{

template <typename ValueType, typename GeometryFactory>
class StandardIntegrationManager2D : public IntegrationManager<ValueType>
{    
private:
    struct DoubleIntegratorIndex
    {
        ElementPairTopology topology;
        int testOrder;
        int trialOrder;

        bool operator<(const DoubleIntegratorIndex& other) const {
            using boost::tuples::make_tuple;
            return make_tuple(topology, testOrder, trialOrder) <
                    make_tuple(other.topology, other.testOrder, other.trialOrder);
        }

        bool operator==(const DoubleIntegratorIndex& other) const {
            return topology == other.topology &&
                    testOrder == other.testOrder &&
                    trialOrder == other.trialOrder;
        }

        bool operator!=(const DoubleIntegratorIndex& other) const {
            return !operator==(other);
        }

        friend std::ostream&
        operator<< (std::ostream& dest, const DoubleIntegratorIndex& obj)
        {
            dest << obj.topology << " " << obj.testOrder << " " << obj.trialOrder;
            return dest;
        }
    };

    typedef boost::ptr_map<DoubleIntegratorIndex,
    DoubleIntegrator<ValueType> > IntegratorMap;

public:
    StandardIntegrationManager2D(
            const GeometryFactory& geometryFactory,
            const arma::Mat<ValueType>& vertices,
            const arma::Mat<int>& elementCornerIndices,
            const arma::Mat<char>& auxData,
            const Expression<ValueType>& testExpression,
            const Kernel<ValueType>& kernel,
            const Expression<ValueType>& trialExpression,
            const OpenClOptions& openClOptions) :
        m_geometryFactory(geometryFactory),
        m_vertices(vertices),
        m_elementCornerIndices(elementCornerIndices),
        m_auxData(auxData),
        m_testExpression(testExpression),
        m_kernel(kernel),
        m_trialExpression(trialExpression),
        m_openClOptions(openClOptions)
    {
        if (vertices.n_rows != 3)
            throw std::invalid_argument(
                    "StandardIntegrationManager2D::StandardIntegrationManager2D(): "
                    "vertex coordinates must be three-dimensional");
    }

    virtual const DoubleIntegrator<ValueType>& testKernelTrialIntegrator(
            int testElementIndex,
            int trialElementIndex,
            const Basis<ValueType>& testBasis,
            const Basis<ValueType>& trialBasis) {
        DoubleIntegratorIndex integratorIndex;

        // Get corner indices of the specified elements
        arma::Col<int> testElementCornerIndices(
                    m_elementCornerIndices.col(testElementIndex));
        if (testElementCornerIndices(3) < 0) // triangle
            testElementCornerIndices.shed_row(3);
        arma::Col<int> trialElementCornerIndices(
                    m_elementCornerIndices.col(trialElementIndex));
        if (trialElementCornerIndices(3) < 0) // triangle
            trialElementCornerIndices.shed_row(3);

        integratorIndex.topology = determineElementPairTopologyIn3D(
                    testElementCornerIndices, trialElementCornerIndices);

        integratorIndex.testOrder = testBasis.order();
        integratorIndex.trialOrder = trialBasis.order();

        if (integratorIndex.topology.type == ElementPairTopology::Disjoint)
        {
            integratorIndex.testOrder +=
                    regularOrderIncrement(testElementIndex, testBasis);
            integratorIndex.trialOrder +=
                    regularOrderIncrement(trialElementIndex, trialBasis);
        }
        else // singular integral
        {
            integratorIndex.testOrder +=
                    singularOrderIncrement(testElementIndex, testBasis);
            integratorIndex.trialOrder +=
                    singularOrderIncrement(trialElementIndex, trialBasis);
        }

        return getIntegrator(integratorIndex);
    }

private:
    int regularOrderIncrement(int elementIndex,
                              const Basis<ValueType>& basis) const
    {
        // Note: this function will make use of options supplied to the integrator
        // in its constructor

        // TODO:
        // 1. Check the size of elements and the distance between them
        //    and estimate the variability of the kernel
        // 2. Take into account the fact that elements might be isoparametric.

        // Make quadrature exact for a constant kernel and affine elements
        return 8;
    }

    int singularOrderIncrement(int elementIndex,
                               const Basis<ValueType>& basis) const
    {
        // Note: this function will make use of options supplied to the integrator
        // in its constructor

        // TODO:
        // 1. Check the size of elements and estimate the variability of the
        //    (Sauter-Schwab-transformed) kernel
        // 2. Take into account the fact that elements might be isoparametric.

        // Make quadrature exact for a constant kernel and affine elements
        return 8;
    }

    const DoubleIntegrator<ValueType>& getIntegrator(const DoubleIntegratorIndex& index)
    {
        const ElementPairTopology& topology = index.topology;

        typename IntegratorMap::iterator it = m_doubleIntegrators.find(index);
        if (it != m_doubleIntegrators.end())
            return *it->second;

        // Integrator doesn't exist yet and must be created.
        std::auto_ptr<DoubleIntegrator<ValueType> > integrator;
        if (topology.type == ElementPairTopology::Disjoint)
        {
            // Create a tensor rule
            arma::Mat<ValueType> testPoints, trialPoints;
            std::vector<ValueType> testWeights, trialWeights;

            fillPointsAndWeightsRegular(topology.testVertexCount,
                                        index.testOrder,
                                        testPoints, testWeights);
            fillPointsAndWeightsRegular(topology.trialVertexCount,
                                        index.trialOrder,
                                        trialPoints, trialWeights);
            typedef SeparableNumericalDoubleIntegrator<ValueType, GeometryFactory> Integrator;
            integrator = std::auto_ptr<DoubleIntegrator<ValueType> >(
                        new Integrator(
                            testPoints, trialPoints, testWeights, trialWeights,
                            m_geometryFactory,
                            m_vertices, m_elementCornerIndices, m_auxData,
                            m_testExpression, m_kernel, m_trialExpression,
                            m_openClOptions));
        }
        else
        {
            arma::Mat<ValueType> testPoints, trialPoints;
            std::vector<ValueType> weights;

            fillPointsAndWeightsSingular(index,
                                         testPoints, trialPoints, weights);
            typedef NonseparableNumericalDoubleIntegrator<ValueType, GeometryFactory> Integrator;
            integrator = std::auto_ptr<DoubleIntegrator<ValueType> >(
                        new Integrator(
                            testPoints, trialPoints, weights,
                            m_geometryFactory,
                            m_vertices, m_elementCornerIndices, m_auxData,
                            m_testExpression, m_kernel, m_trialExpression,
                            m_openClOptions));
        }
        return *m_doubleIntegrators.insert(index, integrator).first->second;
    }

    // Regular integration rules

    void fillPointsAndWeightsRegular(int vertexCount, int order,
                                     arma::Mat<ValueType>& points,
                                     std::vector<ValueType>& weights) const {
        if (vertexCount == 3)
            // Note that for triangles we want to have a different convention
            // for coordinates than Hyena does! TODO: implement this.
            reallyFillPointsAndWeightsRegular<TRIANGLE>(
                        order, points, weights);
        else
            return reallyFillPointsAndWeightsRegular<QUADRANGLE>(
                        order, points, weights);
    }

    template<ELEMENT_SHAPE SHAPE>
    void reallyFillPointsAndWeightsRegular(
            int order, arma::Mat<ValueType>& points,
            std::vector<ValueType>& weights) const {
        const int elementDim = 2;
        const QuadratureRule<SHAPE, GAUSS>& rule(order);
        const int pointCount = rule.getNumPoints();
        points.set_size(elementDim, pointCount);
        for (int i = 0; i < pointCount; ++i)
        {
            const Point2 point = rule.getPoint(i);
            for (int dim = 0; dim < elementDim; ++dim)
                points(dim, i) = point[dim];
            if (SHAPE == TRIANGLE)
                // Map points from
                // Hyena's reference triangle   (0,0)--(1,0)--(1,1)
                // to Dune's reference triangle (0,0)--(1,0)--(0,1).
                points(0, i) -= points(1, i);
        }
        weights.resize(pointCount);
        for (int i = 0; i < pointCount; ++i)
            weights[i] = rule.getWeight(i);
    }

    // Singular integration rules

    void fillPointsAndWeightsSingular(const DoubleIntegratorIndex& index,
                                      arma::Mat<ValueType>& testPoints,
                                      arma::Mat<ValueType>& trialPoints,
                                      std::vector<ValueType>& weights) {
        const ElementPairTopology& topology = index.topology;
        if (topology.testVertexCount == 3 && topology.trialVertexCount == 3)
        {
            // Note that for triangles we want to have a different convention
            // for coordinates than Hyena does! TODO: implement this.
            if (topology.type == ElementPairTopology::SharedVertex)
                reallyFillPointsAndWeightsSingular<TRIANGLE, VRTX_ADJACENT>(
                            index, testPoints, trialPoints, weights);
            else if (topology.type == ElementPairTopology::SharedEdge)
                reallyFillPointsAndWeightsSingular<TRIANGLE, EDGE_ADJACENT>(
                            index, testPoints, trialPoints, weights);
            else if (topology.type == ElementPairTopology::Coincident)
                reallyFillPointsAndWeightsSingular<TRIANGLE, COINCIDENT>(
                            index, testPoints, trialPoints, weights);
            else
                throw std::invalid_argument("StandardIntegrationManager2D::"
                                            "fillPointsAndWeightsSingular(): "
                                            "Invalid element configuration. "
                                            "This shouldn't happen!");
        }
        else if (topology.testVertexCount == 4 && topology.trialVertexCount == 4)
        {
            if (topology.type == ElementPairTopology::SharedVertex)
                reallyFillPointsAndWeightsSingular<QUADRANGLE, VRTX_ADJACENT>(
                            index, testPoints, trialPoints, weights);
            else if (topology.type == ElementPairTopology::SharedEdge)
                reallyFillPointsAndWeightsSingular<QUADRANGLE, EDGE_ADJACENT>(
                            index, testPoints, trialPoints, weights);
            else if (topology.type == ElementPairTopology::Coincident)
                reallyFillPointsAndWeightsSingular<QUADRANGLE, COINCIDENT>(
                            index, testPoints, trialPoints, weights);
            else
                throw std::invalid_argument("StandardIntegrationManager2D::"
                                            "fillPointsAndWeightsSingular(): "
                                            "Invalid element configuration. "
                                            "This shouldn't happen!");
        }
        else
            throw std::invalid_argument("StandardIntegrationManager2D::"
                                        "fillPointsAndWeightsSingular(): "
                                        "Singular quadrature rules for mixed "
                                        "meshes are not implemented yet.");
    }

    template<ELEMENT_SHAPE SHAPE, SING_INT SINGULARITY>
    void reallyFillPointsAndWeightsSingular(
            const DoubleIntegratorIndex& index,
            arma::Mat<ValueType>& testPoints,
            arma::Mat<ValueType>& trialPoints,
            std::vector<ValueType>& weights) {
        const int elementDim = 2;
        // Is there a more efficient way?
        const int order = std::max(index.testOrder, index.trialOrder);
        const QuadratureRule<QUADRANGLE, GAUSS> rule(order); // quadrangle regardless of SHAPE
        const GalerkinDuffyExpression<SHAPE, SINGULARITY> transform(rule);
        const int pointCount = rule.getNumPoints();
        const int regionCount = transform.getNumRegions();
        const int totalPointCount = regionCount * pointCount * pointCount;

        Point2 point;

        // Quadrature points
        testPoints.set_size(elementDim, totalPointCount);
        trialPoints.set_size(elementDim, totalPointCount);
        for (int region = 0; region < regionCount; ++region)
            for (int testIndex = 0; testIndex < pointCount; ++testIndex)
                for (int trialIndex = 0; trialIndex < pointCount; ++trialIndex)
                {
                    int col = testIndex + trialIndex * pointCount +
                            region * pointCount * pointCount;

                    // Test point
                    point = transform.getPointX(testIndex, trialIndex, region);
                    for (int dim = 0; dim < elementDim; ++dim)
                        testPoints(dim, col) = point[dim];
                    if (SHAPE == TRIANGLE)
                        // Map points from
                        // Hyena's reference triangle   (0,0)--(1,0)--(1,1)
                        // to Dune's reference triangle (0,0)--(1,0)--(0,1).
                        testPoints(0, col) -= testPoints(1, col);

                    // Trial point
                    point = transform.getPointY(testIndex, trialIndex, region);
                    for (int dim = 0; dim < elementDim; ++dim)
                        trialPoints(dim, col) = point[dim];
                    if (SHAPE == TRIANGLE)
                        // Map points from
                        // Hyena's reference triangle   (0,0)--(1,0)--(1,1)
                        // to Dune's reference triangle (0,0)--(1,0)--(0,1).
                        trialPoints(0, col) -= trialPoints(1, col);
                }

        // Weights
        weights.resize(totalPointCount);
        for (int region = 0; region < regionCount; ++region)
            for (int testIndex = 0; testIndex < pointCount; ++testIndex)
                for (int trialIndex = 0; trialIndex < pointCount; ++trialIndex)
                {
                    int col = testIndex + trialIndex * pointCount +
                            region * pointCount * pointCount;
                    weights[col] =
                            transform.getWeight(testIndex, trialIndex, region) *
                            rule.getWeight(testIndex) *
                            rule.getWeight(trialIndex);
                }

        if (SINGULARITY == VRTX_ADJACENT)
        {
            remapPointsSharedVertex<SHAPE>(index.topology.testSharedVertex0,
                                           testPoints);
            remapPointsSharedVertex<SHAPE>(index.topology.trialSharedVertex0,
                                           trialPoints);
        }
        else if (SINGULARITY == EDGE_ADJACENT)
        {
            remapPointsSharedEdge<SHAPE>(
                        index.topology.testSharedVertex0,
                        index.topology.testSharedVertex1,
                        testPoints);
            remapPointsSharedEdge<SHAPE>(
                        index.topology.trialSharedVertex0,
                        index.topology.trialSharedVertex1,
                        trialPoints);
        }
    }

    template <ELEMENT_SHAPE SHAPE>
    void remapPointsSharedVertex(
            int sharedVertex, arma::Mat<ValueType>& points) const
    {
        // compile-time dispatch; circumvent the rule that no specialisation
        // of member template functions is possible
        BOOST_STATIC_ASSERT(SHAPE == TRIANGLE || SHAPE == QUADRANGLE);
        if (SHAPE == TRIANGLE)
            remapPointsSharedVertexTriangle(sharedVertex, points);
        else
            remapPointsSharedVertexQuadrilateral(sharedVertex, points);
    }

    template <ELEMENT_SHAPE SHAPE>
    void remapPointsSharedEdge(
            int sharedVertex0, int sharedVertex1, arma::Mat<ValueType>& points) const
    {
        // compile-time dispatch; circumvent the rule that no specialisation
        // of member template functions is possible
        BOOST_STATIC_ASSERT(SHAPE == TRIANGLE || SHAPE == QUADRANGLE);
        if (SHAPE == TRIANGLE)
            remapPointsSharedEdgeTriangle(sharedVertex0, sharedVertex1, points);
        else
            remapPointsSharedEdgeQuadrilateral(sharedVertex0, sharedVertex1, points);
    }

    void remapPointsSharedVertexTriangle(
            int sharedVertex, arma::Mat<ValueType>& points) const
    {
        // Map vertex 0 to vertex #sharedVertex
        if (sharedVertex == 0)
            ; // do nothing
        else if (sharedVertex == 1)
        {
            const int pointCount = points.n_cols;
            arma::Mat<ValueType> oldPoints(points);
            for (int i = 0; i < pointCount; ++i)
            {
                points(0, i) = -oldPoints(0, i) - oldPoints(1, i) + 1.;
                points(1, i) = oldPoints(1, i);
            }
        }
        else if (sharedVertex == 2)
        {
            const int pointCount = points.n_cols;
            arma::Mat<ValueType> oldPoints(points);
            for (int i = 0; i < pointCount; ++i)
            {
                points(0, i) = oldPoints(0, i);
                points(1, i) = -oldPoints(0, i) - oldPoints(1, i) + 1.;
            }
        }
        else
            throw std::invalid_argument("StandardIntegrationManager2D::"
                                        "remapPointsSharedVertexTriangle(): "
                                        "sharedVertex must be 0, 1 or 2");
    }

    void remapPointsSharedVertexQuadrilateral(
            int sharedVertex, arma::Mat<ValueType>& points) const
    {
        throw std::runtime_error("StandardIntegrationManager2D::"
                                 "remapPointsSharedVertexQuadrilateral(): "
                                 "not implemented yet");
    }

    int nonsharedVertexTriangle(int sharedVertex0, int sharedVertex1) const
    {
//        if (sharedVertex0 + sharedVertex1 == 1)
//            return 2;
//        else if (sharedVertex0 + sharedVertex1 == 2)
//            return 1;
//        else if (sharedVertex0 + sharedVertex1 == 3)
//            return 0;
        return 3 - sharedVertex0 - sharedVertex1;
    }

    void remapPointsSharedEdgeTriangle(
            int sharedVertex0, int sharedVertex1, arma::Mat<ValueType>& points) const
    {
        assert(0 <= sharedVertex0 && sharedVertex0 <= 2);
        assert(0 <= sharedVertex1 && sharedVertex1 <= 2);
        assert(sharedVertex0 != sharedVertex1);
        assert(points.n_rows == 2);

        // Vertices of the reference triangle
        arma::Mat<ValueType> refVertices(2, 3);
        refVertices.fill(0.);
        refVertices(0, 1) = 1.;
        refVertices(1, 2) = 1.;

        arma::Mat<ValueType> newVertices(2, 3);
        newVertices.col(0) = refVertices.col(sharedVertex0);
        newVertices.col(1) = refVertices.col(sharedVertex1);
        newVertices.col(2) = refVertices.col(
                    nonsharedVertexTriangle(sharedVertex0, sharedVertex1));

        arma::Col<ValueType> b(newVertices.col(0));
        arma::Mat<ValueType> A(2, 2);
        A.col(0) = newVertices.col(1) - b;
        A.col(1) = newVertices.col(2) - b;

        arma::Mat<ValueType> oldPoints(points);

        // points := A * oldPoints + b[extended to pointCount columns)
        for (int col = 0; col < points.n_cols; ++col)
            for (int dim = 0; dim < 2; ++dim)
                points(dim, col) =
                        A(dim, 0) * oldPoints(0, col) +
                        A(dim, 1) * oldPoints(1, col) +
                        b(dim);
    }

    void remapPointsSharedEdgeQuadrilateral(
            int sharedVertex0, int sharedVertex1, arma::Mat<ValueType>& points) const
    {
        throw std::runtime_error("StandardIntegrationManager2D::"
                                 "remapPointsSharedEdgeQuadrilateral(): "
                                 "not implemented yet");
    }

private:
    const GeometryFactory& m_geometryFactory;
    const arma::Mat<ValueType>& m_vertices;
    const arma::Mat<int>& m_elementCornerIndices;
    const arma::Mat<char>& m_auxData;
    const Expression<ValueType>& m_testExpression;
    const Kernel<ValueType>& m_kernel;
    const Expression<ValueType>& m_trialExpression;
    OpenClOptions m_openClOptions;

    IntegratorMap m_doubleIntegrators;
};

} // namespace Fiber

#endif
