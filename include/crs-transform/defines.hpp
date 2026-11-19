// SPDX-License-Identifier: MIT
#pragma once

#include <proj/crs.hpp>

namespace crs_transform {
using CRS          = osgeo::proj::crs::CRSNNPtr;
using Operation    = osgeo::proj::operation::CoordinateOperationNNPtr;
using DbContext    = osgeo::proj::io::DatabaseContext;
using DbContextPtr = osgeo::proj::io::DatabaseContextNNPtr;

enum class Datum : uint16_t {
    PZ90_02   = 1157,
    PZ90_11   = 1158,
    GSK2011   = 1159,
    Pulkovo95 = 6200,
    Pulkovo42 = 6284,
    WGS84     = 6326,
    PZ90      = 6740,
};

// ГОСТ 32453-2017
static const std::unordered_map<Datum, std::array<double, 7>> EPSG_DATUMS_TO_PZ90_11 = {
    {Datum::PZ90_02,   {-0.373, 0.186, 0.202, -0.0023, 0.00354, -0.00421, -0.008}      },
    {Datum::PZ90_11,   {0, 0, 0, 0, 0, 0, 0}                                           },
    {Datum::GSK2011,   {0, 0.014, -0.008, -0.000562, -0.000019, 0.000053, -0.0006}     },
    {Datum::WGS84,     {-0.013, 0.106, 0.022, -0.0023, 0.00354, -0.00421, -0.008}      },
    {Datum::Pulkovo95, {24.457, -130.784, -81.538, -0.0023, 0.00354, -0.13421, -0.228} },
    {Datum::Pulkovo42, {23.557, -140.844, -79.778, -0.0023, -0.34646, -0.79421, -0.228}},
    {Datum::PZ90,      {-1.443, 0.156, 0.222, -0.0023, 0.00354, -0.13421, -0.228}      },
};
} // namespace crs_transform
