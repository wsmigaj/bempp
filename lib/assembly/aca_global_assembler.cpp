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

#include "config_ahmed.hpp"

#include "aca_global_assembler.hpp"

#include "assembly_options.hpp"
#include "index_permutation.hpp"

#include "../common/auto_timer.hpp"
#include "../fiber/explicit_instantiation.hpp"
#include "../fiber/local_assembler_for_operators.hpp"
#include "../fiber/scalar_traits.hpp"
#include "../grid/entity_iterator.hpp"
#include "../grid/grid.hpp"
#include "../grid/grid_view.hpp"
#include "../space/space.hpp"

#include <armadillo>
#include <boost/shared_array.hpp>
#include <stdexcept>
#include <iostream>

#include <tbb/atomic.h>
#include <tbb/parallel_for.h>
#include <tbb/task_scheduler_init.h>

#ifdef WITH_AHMED
#include "ahmed_aux.hpp"
#include "discrete_aca_linear_operator.hpp"
#include "weak_form_aca_assembly_helper.hpp"
#endif

namespace Bempp
{

// Body of parallel loop
namespace
{

#ifdef WITH_AHMED
template <typename BasisFunctionType, typename ResultType>
class AcaWeakFormAssemblerLoopBody
{
    typedef typename Fiber::ScalarTraits<ResultType>::RealType CoordinateType;
    typedef AhmedDofWrapper<CoordinateType> AhmedDofType;
    typedef bemblcluster<AhmedDofType, AhmedDofType> DoubleCluster;
    typedef mblock<typename AhmedTypeTraits<ResultType>::Type> AhmedMblock;

public:
    AcaWeakFormAssemblerLoopBody(
            WeakFormAcaAssemblyHelper<BasisFunctionType, ResultType>& helper,
            AhmedLeafClusterArray& leafClusters,
            boost::shared_array<AhmedMblock*> blocks,
            const AcaOptions& options,
            tbb::atomic<size_t>& done) :
        m_helper(helper),
        m_leafClusters(leafClusters), m_blocks(blocks),
        m_options(options), m_done(done)
    {
    }

    void operator() (const tbb::blocked_range<size_t>& r) const {
        const char* TEXT = "Approximating ... ";
        for (size_t i = r.begin(); i != r.end(); ++i)
        {
            DoubleCluster* cluster = dynamic_cast<DoubleCluster*>(m_leafClusters[i]);
            apprx_unsym(m_helper, m_blocks[cluster->getidx()],
                        cluster, m_options.eps, m_options.maximumRank);
            // TODO: recompress
            const int HASH_COUNT = 20;
            progressbar(std::cout, TEXT, (++m_done) - 1,
                        m_leafClusters.size(), HASH_COUNT, true);
        }
    }

private:
    mutable WeakFormAcaAssemblyHelper<BasisFunctionType, ResultType>& m_helper;
    size_t m_leafClusterCount;
    AhmedLeafClusterArray& m_leafClusters;
    boost::shared_array<AhmedMblock*> m_blocks;
    const AcaOptions& m_options;
    mutable tbb::atomic<size_t>& m_done;
};
#endif

} // namespace

template <typename BasisFunctionType, typename ResultType>
std::auto_ptr<DiscreteLinearOperator<ResultType> >
AcaGlobalAssembler<BasisFunctionType, ResultType>::assembleWeakForm(
        const Space<BasisFunctionType>& testSpace,
        const Space<BasisFunctionType>& trialSpace,
        const std::vector<LocalAssembler*>& localAssemblers,
        const std::vector<const DiscreteLinOp*>& sparseTermsToAdd,
        const std::vector<ResultType>& denseTermsMultipliers,
        const std::vector<ResultType>& sparseTermsMultipliers,
        const AssemblyOptions& options)
{
#ifdef WITH_AHMED
    typedef AhmedDofWrapper<CoordinateType> AhmedDofType;
    typedef DiscreteAcaLinearOperator<ResultType> DiscreteAcaLinOp;

    const AcaOptions& acaOptions = options.acaOptions();

    // Get the grid's leaf view so that we can iterate over elements
    std::auto_ptr<GridView> view = trialSpace.grid().leafView();

    // const int elementCount = view.entityCount(0);
    const int trialDofCount = trialSpace.globalDofCount();
    const int testDofCount = testSpace.globalDofCount();

#ifndef NDEBUG
    std::cout << "Generating cluster trees... " << std::endl;
#endif
    // o2p: map of original indices to permuted indices
    // p2o: map of permuted indices to original indices
    std::vector<unsigned int> o2pTestDofs(testDofCount);
    std::vector<unsigned int> p2oTestDofs(testDofCount);
    std::vector<unsigned int> o2pTrialDofs(trialDofCount);
    std::vector<unsigned int> p2oTrialDofs(trialDofCount);
    for (unsigned int i = 0; i < testDofCount; ++i)
        o2pTestDofs[i] = i;
    for (unsigned int i = 0; i < testDofCount; ++i)
        p2oTestDofs[i] = i;
    for (unsigned int i = 0; i < trialDofCount; ++i)
        o2pTrialDofs[i] = i;
    for (unsigned int i = 0; i < trialDofCount; ++i)
        p2oTrialDofs[i] = i;

    std::vector<Point3D<CoordinateType> > trialDofCenters, testDofCenters;
    trialSpace.globalDofPositions(trialDofCenters);
    testSpace.globalDofPositions(testDofCenters);

    // Use static_cast to convert from a pointer to Point3D to a pointer to its
    // descendant AhmedDofWrapper, which does not contain any new data members,
    // but just one additional method (the two structs should therefore be
    // binary compatible)
    const AhmedDofType* ahmedTrialDofCenters =
            static_cast<AhmedDofType*>(&trialDofCenters[0]);
    const AhmedDofType* ahmedTestDofCenters =
            static_cast<AhmedDofType*>(&testDofCenters[0]);

    bemcluster<const AhmedDofType> testClusterTree(
                ahmedTestDofCenters, &o2pTestDofs[0],
                0, testDofCount);
    testClusterTree.createClusterTree(
                acaOptions.minimumBlockSize,
                &o2pTestDofs[0], &p2oTestDofs[0]);
    bemcluster<const AhmedDofType> trialClusterTree(
                ahmedTrialDofCenters, &o2pTrialDofs[0],
                0, trialDofCount);
    trialClusterTree.createClusterTree(
                acaOptions.minimumBlockSize,
                &o2pTrialDofs[0], &p2oTrialDofs[0]);

#ifndef NDEBUG
    std::cout << "Test cluster count: " << testClusterTree.getncl()
              << "\nTrial cluster count: " << trialClusterTree.getncl()
              << std::endl;
//    std::cout << "o2pTest:\n" << o2pTestDofs << std::endl;
//    std::cout << "p2oTest:\n" << p2oTestDofs << std::endl;
#endif

    typedef bemblcluster<AhmedDofType, AhmedDofType> DoubleCluster;
    std::auto_ptr<DoubleCluster> doubleClusterTree(
                new DoubleCluster(0, 0, testDofCount, trialDofCount));
    unsigned int blockCount = 0;
    doubleClusterTree->subdivide(&testClusterTree, &trialClusterTree,
                                 acaOptions.eta * acaOptions.eta,
                                 blockCount);

#ifndef NDEBUG
    std::cout << "Double cluster count: " << blockCount << std::endl;
#endif

    std::auto_ptr<DiscreteLinOp> result;

    WeakFormAcaAssemblyHelper<BasisFunctionType, ResultType>
            helper(testSpace, trialSpace, p2oTestDofs, p2oTrialDofs,
                   localAssemblers, sparseTermsToAdd,
                   denseTermsMultipliers, sparseTermsMultipliers, options);

    typedef mblock<typename AhmedTypeTraits<ResultType>::Type> AhmedMblock;
    boost::shared_array<AhmedMblock*> blocks =
            allocateAhmedMblockArray<ResultType>(doubleClusterTree.get());

//    matgen_sqntl(helper, doubleClusterTree.get(), doubleClusterTree.get(),
//                 acaOptions.recompress, acaOptions.eps,
//                 acaOptions.maximumRank, blocks.get());

    matgen_omp(helper, blockCount, doubleClusterTree.get(),
               acaOptions.eps, acaOptions.maximumRank, blocks.get());

//    AhmedLeafClusterArray leafClusters(doubleClusterTree.get());
//    const size_t leafClusterCount = leafClusters.size();

//    int maxThreadCount = 1;
//    if (options.parallelism() == AssemblyOptions::TBB)
//    {
//        if (options.maxThreadCount() == AssemblyOptions::AUTO)
//            maxThreadCount = tbb::task_scheduler_init::automatic;
//        else
//            maxThreadCount = options.maxThreadCount();
//    }
//    tbb::task_scheduler_init scheduler(maxThreadCount);
//    tbb::atomic<size_t> done;
//    done = 0;

//    typedef AcaWeakFormAssemblerLoopBody<BasisFunctionType, ResultType> Body;
//    tbb::parallel_for(tbb::blocked_range<size_t>(0, leafClusterCount),
//                      Body(helper, leafClusters, blocks, acaOptions, done));

    {
        size_t origMemory = sizeof(ResultType) * testDofCount * trialDofCount;
        size_t ahmedMemory = sizeH(doubleClusterTree.get(), blocks.get());
        std::cout << "\nNeeded storage: " << ahmedMemory / 1024. / 1024. << " MB.\n"
                  << "Without approximation: " << origMemory / 1024. / 1024. << " MB.\n"
                  << "Compressed to " << (100. * ahmedMemory) / origMemory << "%.\n"
                  << std::endl;

        if (acaOptions.outputPostscript){
            std::cout << "Writing matrix partition ..." << std::flush;
            std::ofstream os(acaOptions.outputFname.c_str());
            psoutputH(os, doubleClusterTree.get(), testDofCount, blocks.get());
            os.close();
            std::cout << " done." << std::endl;
        }
    }

    result = std::auto_ptr<DiscreteLinOp>(
                new DiscreteAcaLinOp(testDofCount, trialDofCount,
                                     acaOptions.maximumRank,
                                     doubleClusterTree, blocks,
                                     IndexPermutation(o2pTrialDofs),
                                     IndexPermutation(o2pTestDofs)));
    return result;
#else // without Ahmed
    throw std::runtime_error("To enable assembly in ACA mode, recompile BEM++ "
                             "with the symbol WITH_AHMED defined.");
#endif
}

template <typename BasisFunctionType, typename ResultType>
std::auto_ptr<DiscreteLinearOperator<ResultType> >
AcaGlobalAssembler<BasisFunctionType, ResultType>::assembleWeakForm(
        const Space<BasisFunctionType>& testSpace,
        const Space<BasisFunctionType>& trialSpace,
        LocalAssembler& localAssembler,
        const AssemblyOptions& options)
{
    std::vector<LocalAssembler*> localAssemblers(1, &localAssembler);
    std::vector<const DiscreteLinOp*> sparseTermsToAdd;
    std::vector<ResultType> denseTermsMultipliers(1, 1.0);
    std::vector<ResultType> sparseTermsMultipliers;

    return assembleWeakForm(testSpace, trialSpace, localAssemblers,
                            sparseTermsToAdd,
                            denseTermsMultipliers,
                            sparseTermsMultipliers,
                            options);
}

FIBER_INSTANTIATE_CLASS_TEMPLATED_ON_BASIS_AND_RESULT(AcaGlobalAssembler);

} // namespace Bempp
