# CRS Transform

C++ library for proj-based crs-to-crs transformations accoring to GOST 32453-2017.

## Building

```
find_package(PROJ REQUIRED)
target_link_libraries(your_target PRIVATE crs_transform PROJ::proj)
```

## Usage example

```c++

using namespace crs_transform;

// 1. Open the PROJ database once (read-only). Obtained context is NOT thread-safe.
auto dbContext = osgeo::proj::io::DatabaseContext::create();

// 2. Cosntruct factory objects
auto crsFactory = CRSFactory(dbContext);
auto transformer = CoordinateTransformer(dbContext);

// 3. Build source / target CRS.
auto srcCRS = factory.createLatLon(static_cast<uint16_t>(Datum::PZ90));
auto dstCRS = factory.createGaussKrugerOptimalZone(37, static_cast<uint16_t>(Datum::GSK2011));

// ... or load any EPSG code directly:
auto utm37n = crsFactory.createFromEPSG(32637);

// 4. Create source PJ_COORD tuple
auto inCoords = PJ_COORD{.lpz = {.lam = 55, .phi = 37, .z = 100,},};

// 5. Create an operation from srcCRS to dstCRS.
// If is it defined by GOST 32453‑2017, helmert coefficients from there will be used.
// Otherwise, operation is constructed from proj.db.
 auto operation = transformer.operationFactory().createOperation(srcCRS, dstCRS);

// 6. Peform an operation
auto outCoords = transformer.performOperation(operation, inCoords);

// ... or skip step 5 and create operation automatically
transformer.transform(srcCRS, dstCRS, inCoords);
```

Note that neither of 3 classes provided is thread-safe due to usage of proj contexts. However, their construction is
cheap, so separate instances can be used if needed.

Manual creation of coordinate operations is useful when batch of coordinate tuples has to be transformed via same
operation. Calling `CoordinateTransformer::transform` would be wasteful there, as same operation would be constructed
for
every `PJ_COORD` tuple. Instead, operation constructed manually can be safely resued.
