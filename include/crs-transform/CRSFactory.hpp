// SPDX-License-Identifier: MIT
#pragma once

#include "defines.hpp"

#include <proj/crs.hpp>

namespace crs_transform {
class CRSFactory
{
public:
    explicit CRSFactory(const DbContextPtr& dbContext);

    [[nodiscard]] CRS createFromEPSG(uint16_t crsEPSG) const;
    [[nodiscard]] CRS createLatLon(uint16_t datumEPSG) const;
    [[nodiscard]] CRS createGeocentric(uint16_t datumEPSG) const;
    [[nodiscard]] CRS createGaussKrugerSpecificZone(double zone, uint16_t datumEPSG) const;
    [[nodiscard]] CRS createGaussKrugerOptimalZone(double longitudeInDegrees,
                                                   uint16_t datumEPSG) const;

    [[nodiscard]] std::pair<osgeo::proj::datum::GeodeticReferenceFramePtr,
                            osgeo::proj::datum::DatumEnsemblePtr>
    createDatum(uint16_t datumEPSG) const;

private:
    osgeo::proj::io::DatabaseContextPtr m_dbContext;
};
} // namespace crs_transform
