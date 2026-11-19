// SPDX-License-Identifier: MIT
#pragma once

#include "defines.hpp"

#include <proj/crs.hpp>

namespace crs_transform {
class CoordinateOperationFactory
{
public:
    explicit CoordinateOperationFactory(DbContextPtr dbContext);

    [[nodiscard]] Operation createOperation(const CRS& sourceCRS, const CRS& targetCRS) const;
    [[nodiscard]] PJ* convertToCStyleOperation(const Operation& operation, pj_ctx* = nullptr) const;

private:
    [[nodiscard]] Operation createProjectionStep(const CRS& targetCRS) const;
    [[nodiscard]] Operation createCartesianCSStep(
        const osgeo::proj::crs::GeodeticCRSNNPtr& sourceCRS) const;
    [[nodiscard]] Operation createHelmertStep(
        const osgeo::proj::crs::GeodeticCRSNNPtr& sourceCRS,
        const osgeo::proj::crs::GeodeticCRSNNPtr& targetCRS) const;

    static std::pair<osgeo::proj::datum::GeodeticReferenceFramePtr,
                     osgeo::proj::datum::DatumEnsemblePtr>
    extractDatum(const osgeo::proj::crs::GeodeticCRSNNPtr& crs);

    static uint16_t extractDatumEPSG(const osgeo::proj::crs::GeodeticCRSNNPtr& crs);

    DbContextPtr m_dbContext;
    osgeo::proj::operation::CoordinateOperationFactoryNNPtr m_operationFactory;
};
} // namespace crs_transform
