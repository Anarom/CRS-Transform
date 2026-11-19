// SPDX-License-Identifier: MIT
#include "crs-transform/CoordinateTransformer.hpp"

#include <utility>

using namespace osgeo::proj;
using namespace crs_transform;

CoordinateTransformer::CoordinateTransformer(DbContextPtr dbContext)
    : m_operationFactory(std::move(dbContext))
    , m_context(proj_context_create())
{
}

PJ_COORD CoordinateTransformer::transform(const CRS& sourceCRS, const CRS& targetCRS,
                                          const PJ_COORD& coords) const
{
    return performOperation(m_operationFactory.createOperation(sourceCRS, targetCRS), coords);
}

const CoordinateOperationFactory& CoordinateTransformer::operationFactory() const
{
    return m_operationFactory;
}

PJ_COORD CoordinateTransformer::performOperation(const Operation& operation,
                                                 const PJ_COORD& coords) const
{
#if PROJ_AT_LEAST_VERSION(9, 3, 0)
    return operation->coordinateTransformer(m_context)->transform(coords);
#else
    PJ* cStyleOperation = m_operationFactory.convertToCStyleOperation(operation, m_context);
    auto result         = proj_trans(cStyleOperation, PJ_FWD, coords);
    proj_destroy(cStyleOperation);
    return result;
#endif
}
