target_sources(
    ${PROJECT_NAME}
    PUBLIC
    FILE_SET HEADERS
        FILES crs-transform/CoordinateOperationFactory.hpp crs-transform/CoordinateTransformer.hpp
              crs-transform/CRSFactory.hpp crs-transform/defines.hpp)
