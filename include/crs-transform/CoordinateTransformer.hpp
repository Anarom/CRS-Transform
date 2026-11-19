// SPDX-License-Identifier: MIT
#pragma once

#include "CoordinateOperationFactory.hpp"

#include <proj/crs.hpp>

namespace crs_transform {
class CoordinateTransformer
{
public:
    explicit CoordinateTransformer(DbContextPtr dbContext);

    [[nodiscard]] PJ_COORD performOperation(const Operation& operation,
                                            const PJ_COORD& coords) const;
    [[nodiscard]] PJ_COORD transform(const CRS& sourceCRS, const CRS& targetCRS,
                                     const PJ_COORD& coords) const;

    [[nodiscard]] const CoordinateOperationFactory& operationFactory() const;

private:
    CoordinateOperationFactory m_operationFactory;
    PJ_CONTEXT* m_context;
};
} // namespace crs_transform
