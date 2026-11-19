// SPDX-License-Identifier: MIT
#include "crs-transform/CRSFactory.hpp"

#include <cmath>
#include <utility>

using namespace osgeo::proj;
using namespace crs_transform;

static constexpr uint8_t gaussKrugerZoneWidth  = 6;
static constexpr uint32_t gaussKrugerZoneShift = 500000;

CRSFactory::CRSFactory(const DbContextPtr& dbContext) : m_dbContext(dbContext.as_nullable()) {}

std::pair<datum::GeodeticReferenceFramePtr, datum::DatumEnsemblePtr> CRSFactory::createDatum(
    uint16_t datumEPSG) const
{
    auto epsgFactory = io::AuthorityFactory::create(NN_CHECK_THROW(m_dbContext), "EPSG");
    auto epsgString  = std::to_string(datumEPSG);

    if (epsgFactory->getAuthorityCodes(io::AuthorityFactory::ObjectType::DATUM_ENSEMBLE)
            .contains(epsgString))
        return {nullptr, epsgFactory->createDatumEnsemble(epsgString).as_nullable()};

    return {epsgFactory->createGeodeticDatum(epsgString).as_nullable(), nullptr};
}

crs::CRSNNPtr CRSFactory::createFromEPSG(uint16_t crsEPSG) const
{
    auto epsgFactory = io::AuthorityFactory::create(NN_CHECK_THROW(m_dbContext), "EPSG");
    auto epsgString  = std::to_string(crsEPSG);

    if (epsgFactory->getAuthorityCodes(io::AuthorityFactory::ObjectType::PROJECTED_CRS)
            .contains(epsgString))
        return util::nn_static_pointer_cast<crs::CRS>(epsgFactory->createProjectedCRS(epsgString));

    return util::nn_static_pointer_cast<crs::CRS>(epsgFactory->createGeodeticCRS(epsgString));
}

crs::CRSNNPtr CRSFactory::createLatLon(uint16_t datumEPSG) const
{
    auto datums = createDatum(datumEPSG);
    auto cs     = cs::EllipsoidalCS::createLatitudeLongitude(common::UnitOfMeasure::DEGREE);
    return crs::GeographicCRS::create({}, datums.first, datums.second, cs);
}

crs::CRSNNPtr CRSFactory::createGeocentric(uint16_t datumEPSG) const
{
    auto datums           = createDatum(datumEPSG);
    auto coordinateSystem = cs::CartesianCS::createGeocentric(common::UnitOfMeasure::METRE);
    return crs::GeodeticCRS::create({}, datums.first, datums.second, coordinateSystem);
}

crs::CRSNNPtr CRSFactory::createGaussKrugerSpecificZone(double zone, uint16_t datumEPSG) const
{
    common::Angle latitudeOfOrigin(0, common::UnitOfMeasure::DEGREE);
    common::Angle longitudeOfOrigin(gaussKrugerZoneWidth * (zone - 0.5),
                                    common::UnitOfMeasure::DEGREE);

    common::Length falseEasting(zone * 1000000 + gaussKrugerZoneShift,
                                common::UnitOfMeasure::METRE);

    common::Length falseNorthing(0, common::UnitOfMeasure::METRE);

    auto conversion = operation::Conversion::createTransverseMercator({}, latitudeOfOrigin,
                                                                      longitudeOfOrigin,
                                                                      common::Scale(1),
                                                                      falseEasting, falseNorthing);

    auto baseCRS = NN_CHECK_THROW(createLatLon(datumEPSG)->extractGeodeticCRS());
    return crs::ProjectedCRS::create(
        {}, baseCRS, conversion,
        cs::CartesianCS::createNorthingEasting(common::UnitOfMeasure::METRE));
}

crs::CRSNNPtr CRSFactory::createGaussKrugerOptimalZone(double longitudeInDegrees,
                                                       uint16_t datumEPSG) const
{
    longitudeInDegrees = std::min(longitudeInDegrees, 180.);
    longitudeInDegrees = std::max(longitudeInDegrees, -180.);

    if (longitudeInDegrees < 0)
        longitudeInDegrees = 360 + longitudeInDegrees;

    uint8_t zoneIndex = std::floor(longitudeInDegrees / static_cast<double>(gaussKrugerZoneWidth))
                        + 1;
    return createGaussKrugerSpecificZone(zoneIndex, datumEPSG);
}
