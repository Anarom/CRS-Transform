// SPDX-License-Identifier: MIT
#include "crs-transform/CoordinateOperationFactory.hpp"

#include <algorithm>

using namespace osgeo::proj;
using namespace crs_transform;

CoordinateOperationFactory::CoordinateOperationFactory(DbContextPtr dbContext)
    : m_dbContext(std::move(dbContext))
    , m_operationFactory(operation::CoordinateOperationFactory::create())
{
}

Operation CoordinateOperationFactory::createOperation(const CRS& sourceCRS,
                                                      const CRS& targetCRS) const
{
    auto createJointCRS = [](const auto& crs) {
        return NN_NO_CHECK(std::dynamic_pointer_cast<crs::GeodeticCRS>(crs));
    };

    std::vector<Operation> operationSteps;
    operationSteps.reserve(5);

    operationSteps.push_back(createProjectionStep(sourceCRS)->inverse());
    operationSteps.push_back(
        createCartesianCSStep(createJointCRS(operationSteps.back()->targetCRS())));

    auto step5 = createProjectionStep(targetCRS);
    auto step4 = createCartesianCSStep(createJointCRS(step5->sourceCRS()))->inverse();

    operationSteps.push_back(createHelmertStep(createJointCRS(operationSteps.back()->targetCRS()),
                                               createJointCRS(step4->sourceCRS())));

    operationSteps.push_back(std::move(step4));
    operationSteps.push_back(std::move(step5));

    return operation::ConcatenatedOperation::create({}, operationSteps, {});
}

PJ* CoordinateOperationFactory::convertToCStyleOperation(const Operation& operation,
                                                         pj_ctx* ctx) const
{
    auto formatter = io::PROJStringFormatter::create(io::PROJStringFormatter::Convention::PROJ_5,
                                                     m_dbContext);
    auto string    = operation->exportToPROJString(formatter.get());
    return proj_create(ctx, string.data());
}

operation::CoordinateOperationNNPtr CoordinateOperationFactory::createProjectionStep(
    const crs::CRSNNPtr& targetCRS) const
{
    operation::CoordinateOperationPtr operation;
    if (const auto* projectedCRS = dynamic_cast<const crs::ProjectedCRS*>(targetCRS.get()))
        operation = m_operationFactory->createOperation(projectedCRS->baseCRS(), targetCRS);
    else
        operation = m_operationFactory->createOperation(targetCRS, targetCRS);

    return NN_CHECK_ASSERT(operation);
}

operation::CoordinateOperationNNPtr CoordinateOperationFactory::createCartesianCSStep(
    const crs::GeodeticCRSNNPtr& sourceCRS) const
{
    auto targetCS  = cs::CartesianCS::createGeocentric(common::UnitOfMeasure::METRE);
    auto datum     = extractDatum(sourceCRS);
    auto targetCRS = crs::GeodeticCRS::create({}, datum.first, datum.second, targetCS);
    auto operation = m_operationFactory->createOperation(sourceCRS, targetCRS);
    return NN_CHECK_ASSERT(operation);
}

operation::CoordinateOperationNNPtr CoordinateOperationFactory::createHelmertStep(
    const crs::GeodeticCRSNNPtr& sourceCRS, const crs::GeodeticCRSNNPtr& targetCRS) const
{
    auto srcDatumEPSG    = static_cast<Datum>(extractDatumEPSG(sourceCRS));
    auto targetDatumEPSG = static_cast<Datum>(extractDatumEPSG(targetCRS));

    // Noop if datums are equal
    if (srcDatumEPSG == targetDatumEPSG)
        return NN_NO_CHECK(m_operationFactory->createOperation(sourceCRS, targetCRS));

    // If hash has doesn't contain either of datums, try to create operation from proj.db.
    // It won't necessarily be Helmert transformation.
    if (!EPSG_DATUMS_TO_PZ90_11.contains(srcDatumEPSG)
        || !EPSG_DATUMS_TO_PZ90_11.contains(targetDatumEPSG))
        return NN_CHECK_THROW(m_operationFactory->createOperation(sourceCRS, targetCRS));

    auto srcHelmertParameters    = EPSG_DATUMS_TO_PZ90_11.at(srcDatumEPSG);
    auto targetHelmertParameters = EPSG_DATUMS_TO_PZ90_11.at(targetDatumEPSG);

    for (size_t i = 0; i < targetHelmertParameters.size(); ++i)
        srcHelmertParameters.at(i) -= targetHelmertParameters.at(i);

    return operation::Transformation::createCoordinateFrameRotation(
        {}, sourceCRS, targetCRS, srcHelmertParameters[0], srcHelmertParameters[1],
        srcHelmertParameters[2], srcHelmertParameters[3], srcHelmertParameters[4],
        srcHelmertParameters[5], srcHelmertParameters[6], {});
}

std::pair<datum::GeodeticReferenceFramePtr, datum::DatumEnsemblePtr>
CoordinateOperationFactory::extractDatum(const crs::GeodeticCRSNNPtr& crs)
{
    std::pair<datum::GeodeticReferenceFramePtr, datum::DatumEnsemblePtr> result;
    result.first  = crs->datum();
    result.second = crs->datumEnsemble();
    return result;
}

uint16_t CoordinateOperationFactory::extractDatumEPSG(const crs::GeodeticCRSNNPtr& crs)
{
    auto datum = extractDatum(crs);
    if (datum.first)
        return datum.first->getEPSGCode();
    if (datum.second)
        return datum.second->getEPSGCode();
    throw std::invalid_argument("Cannot extract datum EPSG");
}
