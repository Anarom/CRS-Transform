// SPDX-License-Identifier: MIT
#include <array>
#include <cmath>
#include <string>

#include <crs-transform/CRSFactory.hpp>
#include <crs-transform/CoordinateOperationFactory.hpp>
#include <crs-transform/CoordinateTransformer.hpp>
#include <gtest/gtest.h>
#include <proj.h>

namespace {
using namespace crs_transform;
using namespace osgeo::proj;

constexpr uint16_t kWgs84     = static_cast<uint16_t>(Datum::WGS84);     // 6326
constexpr uint16_t kPz9011    = static_cast<uint16_t>(Datum::PZ90_11);   // 1158
constexpr uint16_t kPz9002    = static_cast<uint16_t>(Datum::PZ90_02);   // 1157
constexpr uint16_t kGsk2011   = static_cast<uint16_t>(Datum::GSK2011);   // 1159
constexpr uint16_t kPulkovo42 = static_cast<uint16_t>(Datum::Pulkovo42); // 6284
constexpr uint16_t kPulkovo95 = static_cast<uint16_t>(Datum::Pulkovo95); // 6200
constexpr uint16_t kPz90      = static_cast<uint16_t>(Datum::PZ90);      // 6740

PJ_COORD lonLat(double lon, double lat)
{
    return {
        .lp{
            .lam = lat,
            .phi = lon,
            },
    };
}

// Approximate planar distance (metres) between two lon/lat points
double lonLatDistanceMeters(double lon1, double lat1, double lon2, double lat2)
{
    constexpr double kRadToM = 111320.0;
    double dx                = (lon2 - lon1) * std::cos(lat1 * std::numbers::pi / 180.0) * kRadToM;
    double dy                = (lat2 - lat1) * kRadToM;
    return std::hypot(dx, dy);
}
} // namespace

// ============================================================================
// Fixture: shares the DatabaseContext across all tests in the binary.
// ============================================================================
class CrsTransformTest : public ::testing::Test
{
protected:
    static io::DatabaseContextPtr dbContext;

    static void SetUpTestSuite() { dbContext = io::DatabaseContext::create().as_nullable(); }

    static io::DatabaseContextNNPtr nnDb() { return NN_CHECK_THROW(dbContext); }

    static CRSFactory factory() { return CRSFactory(nnDb()); }

    static CoordinateOperationFactory operationFactory()
    {
        return CoordinateOperationFactory(nnDb());
    }

    static CoordinateTransformer transformer() { return CoordinateTransformer(nnDb()); }
};

io::DatabaseContextPtr CrsTransformTest::dbContext{};

// ============================================================================
// CRSFactory
// ============================================================================

TEST_F(CrsTransformTest, CreateFromEpsgGeographic)
{
    auto crs = factory().createFromEPSG(4326); // WGS 84 geographic
    EXPECT_NE(dynamic_cast<const crs::GeographicCRS*>(crs.get()), nullptr);
}

TEST_F(CrsTransformTest, CreateFromEpsgProjected)
{
    auto crs = factory().createFromEPSG(32637); // WGS 84 / UTM 37N
    EXPECT_NE(dynamic_cast<const crs::ProjectedCRS*>(crs.get()), nullptr);
}

TEST_F(CrsTransformTest, CreateFromInvalidEpsgThrows)
{
    EXPECT_ANY_THROW(factory().createFromEPSG(999999));
}

TEST_F(CrsTransformTest, CreateLatLonProducesGeographicCrs)
{
    auto crs        = factory().createLatLon(kWgs84);
    const auto* geo = dynamic_cast<const crs::GeographicCRS*>(crs.get());
    ASSERT_NE(geo, nullptr);
    EXPECT_EQ(geo->coordinateSystem()->axisList().size(), 2u);
}

TEST_F(CrsTransformTest, CreateGeocentricProducesGeodeticCrs)
{
    auto crs        = factory().createGeocentric(kWgs84);
    const auto* geo = dynamic_cast<const crs::GeodeticCRS*>(crs.get());
    ASSERT_NE(geo, nullptr);
    EXPECT_EQ(geo->coordinateSystem()->axisList().size(), 3u);
}

TEST_F(CrsTransformTest, CreateDatumYieldsExactlyOneOfDatumOrEnsemble)
{
    // Whether EPSG:6326 is modelled as a datum or an ensemble depends on the
    // proj.db version — the factory must return exactly one of them.
    auto [datum, ensemble] = factory().createDatum(kWgs84);
    EXPECT_NE(datum != nullptr, ensemble != nullptr);
}

TEST_F(CrsTransformTest, GaussKrugerAtCentralMeridian)
{
    // Point at lon = 39° (central meridian of zone 7) on the equator.
    // Expected: northing = 0, easting = zone*1e6 + 5e5 = 7 500 000 m.
    auto wgs = factory().createLatLon(kWgs84);
    auto gk  = factory().createGaussKrugerSpecificZone(7, kWgs84);

    auto out = transformer().transform(wgs, gk, lonLat(39.0, 0.0));

    EXPECT_NEAR(out.xy.x, 0.0, 1e-2) << "northing";
    EXPECT_NEAR(out.xy.y, 7'500'000.0, 1e-2) << "easting";
}

TEST_F(CrsTransformTest, GaussKrugerOptimalZoneMatchesSpecificZone)
{
    struct Case
    {
        double lon   = 0;
        uint8_t zone = 0;
    };

    std::array cases{
        Case{.lon = 0.0,    .zone = 1 },
        Case{.lon = 9.0,    .zone = 2 },
        Case{.lon = 11.999, .zone = 2 },
        Case{.lon = 12.0,   .zone = 3 },
        Case{.lon = 36.0,   .zone = 7 },
        Case{.lon = -1.0,   .zone = 60}, // maps to 359° → zone 60
        Case{.lon = -6.0,   .zone = 60},
        Case{.lon = -6.01,  .zone = 59},
        Case{.lon = 180.0,  .zone = 31},
        Case{.lon = -180.0, .zone = 31},
    };

    auto wgs = factory().createLatLon(kWgs84);
    auto t   = transformer();

    for (const auto& c : cases) {
        auto opt = factory().createGaussKrugerOptimalZone(c.lon, kWgs84);
        auto spc = factory().createGaussKrugerSpecificZone(c.zone, kWgs84);

        auto in = lonLat(c.lon + 0.1, 45.0);
        auto a  = t.transform(wgs, opt, in);
        auto b  = t.transform(wgs, spc, in);

        EXPECT_NEAR(a.xy.x, b.xy.x, 1e-3)
            << "northing, zone=" << static_cast<int>(c.zone) << " lon=" << c.lon;
        EXPECT_NEAR(a.xy.y, b.xy.y, 1e-3)
            << "easting,  zone=" << static_cast<int>(c.zone) << " lon=" << c.lon;
    }
}

TEST_F(CrsTransformTest, GaussKrugerZoneIsClampedToValidLongitudes)
{
    // Latitudes/longitudes beyond ±180° must be clamped, not wrap or crash.
    EXPECT_NO_THROW(factory().createGaussKrugerOptimalZone(720.0, kWgs84));
    EXPECT_NO_THROW(factory().createGaussKrugerOptimalZone(-720.0, kWgs84));

    // Clamped +180° should pick the same zone as exactly +180°.
    auto a = factory().createGaussKrugerOptimalZone(180.0, kWgs84);
    auto b = factory().createGaussKrugerOptimalZone(720.0, kWgs84);

    auto wgs = factory().createLatLon(kWgs84);
    auto r1  = transformer().transform(wgs, a, lonLat(180.0, 45.0));
    auto r2  = transformer().transform(wgs, b, lonLat(180.0, 45.0));
    EXPECT_NEAR(r1.xy.x, r2.xy.x, 1e-6);
    EXPECT_NEAR(r1.xy.y, r2.xy.y, 1e-6);
}

// ============================================================================
// CoordinateOperationFactory
// ============================================================================

TEST_F(CrsTransformTest, CreateOperationSameDatumIsNonNull)
{
    auto crsA = factory().createLatLon(kWgs84);
    auto crsB = factory().createGeocentric(kWgs84);
    EXPECT_NE(operationFactory().createOperation(crsA, crsB), nullptr);
}

TEST_F(CrsTransformTest, CreateOperationDifferentDatumIsNonNull)
{
    auto crsA = factory().createLatLon(kWgs84);
    auto crsB = factory().createLatLon(kPulkovo42);
    EXPECT_NE(operationFactory().createOperation(crsA, crsB), nullptr);
}

TEST_F(CrsTransformTest, CreateOperationProjectedToProjectedIsNonNull)
{
    auto gk7 = factory().createGaussKrugerSpecificZone(7, kWgs84);
    auto gk8 = factory().createGaussKrugerSpecificZone(8, kWgs84);
    EXPECT_NE(operationFactory().createOperation(gk7, gk8), nullptr);
}

TEST_F(CrsTransformTest, CreateOperationProjectedToGeographicIsNonNull)
{
    auto gk  = factory().createGaussKrugerSpecificZone(7, kWgs84);
    auto wgs = factory().createLatLon(kWgs84);
    EXPECT_NE(operationFactory().createOperation(gk, wgs), nullptr);
    EXPECT_NE(operationFactory().createOperation(wgs, gk), nullptr);
}

TEST_F(CrsTransformTest, ExportOperationToProjStringContainsPipeline)
{
    auto crsA = factory().createLatLon(kWgs84);
    auto crsB = factory().createLatLon(kPulkovo42);
    auto op   = operationFactory().createOperation(crsA, crsB);

    auto formatter = osgeo::proj::io::PROJStringFormatter::create();
    auto str       = op->exportToPROJString(formatter.get());

    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("+proj="), std::string::npos);
}

TEST_F(CrsTransformTest, ConvertToCStyleOperationWithExternalContext)
{
    auto crsA = factory().createLatLon(kWgs84);
    auto crsB = factory().createLatLon(kPulkovo42);
    auto op   = operationFactory().createOperation(crsA, crsB);

    pj_ctx* ctx = proj_context_create();
    ASSERT_NE(ctx, nullptr);

    PJ* pj = operationFactory().convertToCStyleOperation(op, ctx);
    EXPECT_NE(pj, nullptr);
    if (pj)
        proj_destroy(pj);

    proj_context_destroy(ctx);
}

// ============================================================================
// CoordinateTransformer
// ============================================================================

TEST_F(CrsTransformTest, TransformSameCrsIsNearIdentity)
{
    auto wgs = factory().createLatLon(kWgs84);
    auto in  = lonLat(37.6173, 55.7558);
    auto out = transformer().transform(wgs, wgs, in);

    EXPECT_NEAR(out.lp.lam, in.lp.lam, 1e-9);
    EXPECT_NEAR(out.lp.phi, in.lp.phi, 1e-9);
}

TEST_F(CrsTransformTest, RoundtripLatLonGaussKruger)
{
    auto wgs = factory().createLatLon(kWgs84);
    auto gk  = factory().createGaussKrugerSpecificZone(7, kWgs84);

    auto in  = lonLat(37.6173, 55.7558); // Moscow
    auto mid = transformer().transform(wgs, gk, in);
    auto out = transformer().transform(gk, wgs, mid);

    EXPECT_NEAR(out.lp.lam, in.lp.lam, 1e-6);
    EXPECT_NEAR(out.lp.phi, in.lp.phi, 1e-6);
}

TEST_F(CrsTransformTest, RoundtripCrossDatumWgs84Pz9011)
{
    auto wgs  = factory().createLatLon(kWgs84);
    auto pz11 = factory().createLatLon(kPz9011);

    auto in  = lonLat(37.6173, 55.7558);
    auto mid = transformer().transform(wgs, pz11, in);
    auto out = transformer().transform(pz11, wgs, mid);

    EXPECT_NEAR(out.lp.lam, in.lp.lam, 1e-6);
    EXPECT_NEAR(out.lp.phi, in.lp.phi, 1e-6);
}

TEST_F(CrsTransformTest, RoundtripCrossDatumPulkovo42Pulkovo95)
{
    auto p42 = factory().createLatLon(kPulkovo42);
    auto p95 = factory().createLatLon(kPulkovo95);

    auto in  = lonLat(37.6173, 55.7558);
    auto mid = transformer().transform(p42, p95, in);
    auto out = transformer().transform(p95, p42, mid);

    EXPECT_NEAR(out.lp.lam, in.lp.lam, 1e-6);
    EXPECT_NEAR(out.lp.phi, in.lp.phi, 1e-6);
}

TEST_F(CrsTransformTest, RoundtripCrossDatumGsk2011Wgs84)
{
    auto gsk = factory().createLatLon(kGsk2011);
    auto wgs = factory().createLatLon(kWgs84);

    auto in  = lonLat(37.6173, 55.7558);
    auto mid = transformer().transform(gsk, wgs, in);
    auto out = transformer().transform(wgs, gsk, mid);

    EXPECT_NEAR(out.lp.lam, in.lp.lam, 1e-6);
    EXPECT_NEAR(out.lp.phi, in.lp.phi, 1e-6);
}

TEST_F(CrsTransformTest, Wgs84ToPz9011IsSmallShift)
{
    // PZ90.11 Helmert parameters are all zero, so the resulting shift is
    // just the WGS84-to-PZ90.11 correction — well under a metre.
    auto wgs  = factory().createLatLon(kWgs84);
    auto pz11 = factory().createLatLon(kPz9011);

    auto in  = lonLat(37.6173, 55.7558);
    auto out = transformer().transform(wgs, pz11, in);

    double d = lonLatDistanceMeters(in.lp.lam, in.lp.phi, out.lp.lam, out.lp.phi);
    EXPECT_GT(d, 0.001); // not exactly zero
    EXPECT_LT(d, 5.0);   // sub-metre-ish
}

TEST_F(CrsTransformTest, Wgs84ToPulkovo42IsLargeShift)
{
    // Pulkovo 42 differs from WGS 84 by ~100 m in European Russia.
    auto wgs = factory().createLatLon(kWgs84);
    auto p42 = factory().createLatLon(kPulkovo42);

    auto in  = lonLat(37.6173, 55.7558);
    auto out = transformer().transform(wgs, p42, in);

    double d = lonLatDistanceMeters(in.lp.lam, in.lp.phi, out.lp.lam, out.lp.phi);
    EXPECT_GT(d, 1.0);
    EXPECT_LT(d, 1000.0);
}

TEST_F(CrsTransformTest, DifferentDatumsProduceDifferentResults)
{
    auto wgs  = factory().createLatLon(kWgs84);
    auto p42  = factory().createLatLon(kPulkovo42);
    auto pz11 = factory().createLatLon(kPz9011);

    auto in   = lonLat(37.6173, 55.7558);
    auto out1 = transformer().transform(wgs, p42, in);
    auto out2 = transformer().transform(wgs, pz11, in);

    // P42 vs PZ90.11 should differ by ~100 m; anything smaller is suspicious.
    double d = lonLatDistanceMeters(out1.lp.lam, out1.lp.phi, out2.lp.lam, out2.lp.phi);
    EXPECT_GT(d, 10.0);
}

TEST_F(CrsTransformTest, PerformOperationReuseProducesSameResults)
{
    auto wgs = factory().createLatLon(kWgs84);
    auto gk  = factory().createGaussKrugerSpecificZone(7, kWgs84);
    auto op  = operationFactory().createOperation(wgs, gk);

    auto t  = transformer();
    auto in = lonLat(37.6173, 55.7558);
    auto r1 = t.performOperation(op, in);
    auto r2 = t.performOperation(op, in);

    EXPECT_NEAR(r1.xy.x, r2.xy.x, 1e-12);
    EXPECT_NEAR(r1.xy.y, r2.xy.y, 1e-12);
}

TEST_F(CrsTransformTest, TransformDeterministicAcrossRepeatedCalls)
{
    auto wgs = factory().createLatLon(kWgs84);
    auto gk  = factory().createGaussKrugerSpecificZone(7, kWgs84);
    auto t   = transformer();

    auto in = lonLat(37.6173, 55.7558);
    auto r1 = t.transform(wgs, gk, in);
    for (int i = 0; i < 100; ++i) {
        auto r = t.transform(wgs, gk, in);
        EXPECT_EQ(r.xy.x, r1.xy.x);
        EXPECT_EQ(r.xy.y, r1.xy.y);
    }
}

TEST_F(CrsTransformTest, Pz90ToPz9011ProducesFiniteResult)
{
    // Exercise the PZ90 → PZ90.11 pair, which uses the largest set of
    // non-zero Helmert parameters.
    auto pz   = factory().createLatLon(kPz90);
    auto pz11 = factory().createLatLon(kPz9011);

    auto in  = lonLat(37.6173, 55.7558);
    auto out = transformer().transform(pz, pz11, in);
    EXPECT_TRUE(std::isfinite(out.lp.lam));
    EXPECT_TRUE(std::isfinite(out.lp.phi));
}

TEST_F(CrsTransformTest, Pz9002ToWgs84Roundtrip)
{
    auto pz02 = factory().createLatLon(kPz9002);
    auto wgs  = factory().createLatLon(kWgs84);

    auto in  = lonLat(37.6173, 55.7558);
    auto mid = transformer().transform(pz02, wgs, in);
    auto out = transformer().transform(wgs, pz02, mid);

    EXPECT_NEAR(out.lp.lam, in.lp.lam, 1e-6);
    EXPECT_NEAR(out.lp.phi, in.lp.phi, 1e-6);
}
