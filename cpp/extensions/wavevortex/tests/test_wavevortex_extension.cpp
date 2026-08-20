#include "alongtrack/portable_core.hpp"
#include "alongtrack/wavevortex_extension.hpp"

#include "WaveVortexRuntime/WVCheckpointReader.hpp"
#include "WaveVortexRuntime/WVFieldEvaluationService.hpp"
#include "WaveVortexRuntime/WVObserverOutputEvaluationService.hpp"
#include "WaveVortexRuntime/WVOutputOrchestration.hpp"
#include "WaveVortexRuntime/WVRungeKutta.hpp"

#include "WVReferenceFFTEngine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef ATS_WAVEVORTEX_CHECKPOINT_PATH
#error "ATS_WAVEVORTEX_CHECKPOINT_PATH must identify root-hydrostatic.nc"
#endif

#ifndef ATS_WAVEVORTEX_MATLAB_REFERENCE_PATH
#error "ATS_WAVEVORTEX_MATLAB_REFERENCE_PATH must identify the MATLAB oracle"
#endif

using namespace wavevortex;
using namespace wavevortex::runtime;

namespace atwv = alongtrack::wavevortex_extension;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireStatus(const WVKernelStatus& status, const std::string& context) {
    require(static_cast<bool>(status), context + ": " + status.message);
}

void requireClose(double actual,
                  double expected,
                  double absoluteTolerance,
                  double relativeTolerance,
                  const std::string& message) {
    const double difference = std::abs(actual - expected);
    const double tolerance = absoluteTolerance +
                             relativeTolerance * std::abs(expected);
    if (difference > tolerance) {
        std::ostringstream detail;
        detail << std::setprecision(17) << message << " expected " << expected
               << ", got " << actual << ", difference " << difference
               << ", tolerance " << tolerance;
        require(false, detail.str());
    }
}

WVPortableNamedValue realScalar(const char* name, double value) {
    return {name, {}, std::vector<double>{value}};
}

WVPortableNamedValue integerScalar(const char* name, std::int64_t value) {
    return {name, {}, std::vector<std::int64_t>{value}};
}

WVPortableNamedValue booleanScalar(const char* name, bool value) {
    return {name, {},
            std::vector<std::uint8_t>{static_cast<std::uint8_t>(value)}};
}

WVPortableNamedValue textScalar(const char* name, std::string value) {
    return {name, {}, std::vector<std::string>{std::move(value)}};
}

void addResolvedMissionConfiguration(
    WVPortableTypedRecord& record,
    const alongtrack::ResolvedMission& mission,
    const alongtrack::ProjectionWindow& window,
    double sampleIntervalSeconds,
    double missionEpochSeconds = 0.0) {
    const auto& orbit = mission.orbit();
    record.values = {
        textScalar(atwv::missionKeyField, mission.key()),
        realScalar(atwv::semiMajorAxisKmField, orbit.semiMajorAxisKm()),
        realScalar(atwv::resolvedSemiMajorAxisKmField,
                   orbit.semiMajorAxisKm()),
        realScalar(atwv::eccentricityField, orbit.eccentricity()),
        realScalar(atwv::inclinationDegreesField,
                   orbit.inclinationDegrees()),
        realScalar(atwv::ascendingNodeLongitudeDegreesField,
                   orbit.ascendingNodeLongitudeDegrees()),
        realScalar(atwv::passesPerCycleField,
                   mission.isRepeating() ? mission.passesPerCycle() : 0.0),
        realScalar(atwv::missionEpochSecondsField, missionEpochSeconds),
        realScalar(atwv::windowWidthMetersField, window.widthMeters()),
        realScalar(atwv::windowHeightMetersField, window.heightMeters()),
        realScalar(atwv::centerLatitudeDegreesField,
                   window.centerLatitudeDegrees()),
        realScalar(atwv::centerLongitudeDegreesField,
                   window.centerLongitudeDegrees()),
        booleanScalar(atwv::lowerLeftOriginField,
                      window.origin() == alongtrack::WindowOrigin::lowerLeft),
        realScalar(atwv::sampleIntervalSecondsField,
                   sampleIntervalSeconds),
    };
}

WVOutputScheduleRecord scheduleRecord(
    const alongtrack::ResolvedMission& mission,
    const alongtrack::ProjectionWindow& window,
    double sampleIntervalSeconds,
    double initialTime,
    double finalTime,
    double missionEpochSeconds = 0.0) {
    WVOutputScheduleRecord record;
    record.initialTime = initialTime;
    record.finalTime = finalTime;
    record.typeIdentifier = atwv::scheduleTypeIdentifier;
    record.contractVersion = atwv::contractVersion;
    record.configuration.schemaIdentifier =
        atwv::scheduleConfigurationSchemaIdentifier;
    record.configuration.schemaVersion = atwv::contractVersion;
    addResolvedMissionConfiguration(record.configuration, mission, window,
                                    sampleIntervalSeconds,
                                    missionEpochSeconds);
    return record;
}

WVObserverRecord observerRecord(
    std::string identifier,
    const alongtrack::ResolvedMission& mission,
    const alongtrack::ProjectionWindow& window,
    double sampleIntervalSeconds,
    std::vector<std::string> fields,
    WVPositionInterpolation interpolation = WVPositionInterpolation::linear,
    double missionEpochSeconds = 0.0) {
    WVObserverRecord observer;
    observer.identifier = std::move(identifier);
    observer.name = observer.identifier;
    observer.typeIdentifier = atwv::observingSystemTypeIdentifier;
    observer.contractVersion = atwv::contractVersion;
    observer.fieldNames = fields;
    observer.configuration.schemaIdentifier =
        atwv::observerConfigurationSchemaIdentifier;
    observer.configuration.schemaVersion = atwv::contractVersion;
    addResolvedMissionConfiguration(observer.configuration, mission, window,
                                    sampleIntervalSeconds,
                                    missionEpochSeconds);
    observer.configuration.values.push_back(
        {atwv::fieldNamesField, {fields.size()}, std::move(fields)});
    observer.configuration.values.push_back(integerScalar(
        atwv::interpolationField,
        interpolation == WVPositionInterpolation::linear ? 0 : 1));
    return observer;
}

WVObserverRecord coefficientObserver() {
    WVObserverRecord observer;
    observer.identifier = "coefficients";
    observer.name = "Wave-vortex coefficients";
    observer.typeIdentifier = "WVCoefficients";
    observer.stateBlockIdentifiers = {"Ap", "Am", "A0"};
    return observer;
}

void addCoefficientStateBlocks(WVPortableObserverRecord& record,
                               WVShape2D shape) {
    for (const char* identifier : {"Ap", "Am", "A0"}) {
        record.stateBlocks.push_back(
            {identifier,
             WVStateScalarType::complex64,
             {shape.rows, shape.columns},
             WVToleranceKind::coefficientEnergyScaled,
             0.0,
             WVStateOwnership::integratorOwned,
             WVRestartRequirement::requiredDynamicState});
    }
}

WVOutputFileRecord outputFile(std::string fileIdentifier,
                              std::string destination,
                              std::string groupIdentifier,
                              const WVOutputScheduleRecord& schedule,
                              const std::vector<std::string>& observers) {
    WVOutputGroupRecord restart;
    restart.identifier = groupIdentifier + "-restart";
    restart.name = restart.identifier;
    restart.schedule.outputInterval = 1.0;
    restart.schedule.initialTime = schedule.finalTime + 1.0;
    restart.schedule.finalTime = restart.schedule.initialTime;
    restart.observerIdentifiers = {"coefficients"};
    restart.containsCompleteCoefficientRestart = true;

    WVOutputGroupRecord group;
    group.identifier = std::move(groupIdentifier);
    group.name = group.identifier;
    group.schedule = schedule;
    group.observerIdentifiers = observers;
    WVOutputFileRecord file;
    file.identifier = std::move(fileIdentifier);
    file.destination = std::move(destination);
    file.groups.push_back(std::move(restart));
    file.groups.push_back(std::move(group));
    return file;
}

WVPortableObserverRecord portableRecord(
    WVShape2D shape,
    std::vector<WVObserverRecord> alongTrackObservers,
    const WVOutputScheduleRecord& schedule,
    bool coincidentDestinations = false) {
    WVPortableObserverRecord record;
    addCoefficientStateBlocks(record, shape);
    record.observers.push_back(coefficientObserver());
    std::vector<std::string> identifiers;
    identifiers.reserve(alongTrackObservers.size());
    for (auto& observer : alongTrackObservers) {
        identifiers.push_back(observer.identifier);
        record.observers.push_back(std::move(observer));
    }
    record.outputFiles.push_back(outputFile(
        "primary", "primary.memory", "along-track", schedule, identifiers));
    if (coincidentDestinations) {
        record.outputFiles.push_back(outputFile(
            "secondary", "secondary.memory", "along-track", schedule,
            identifiers));
    }
    return record;
}

std::shared_ptr<const WVExtensionCatalog> catalog(bool extended) {
    WVExtensionCatalogBuilder builder;
    auto status = addBuiltInExtensions(builder);
    if (status && extended) {
        status = atwv::registerAlongTrackExtensions(builder);
    }
    std::shared_ptr<const WVExtensionCatalog> result;
    if (status) {
        status = builder.freeze(result);
    }
    requireStatus(status, "extension catalog construction");
    return result;
}

struct MatlabSample final {
    double time = 0.0;
    double x = 0.0;
    double y = 0.0;
    double fieldX = 0.0;
    double fieldY = 0.0;
    double ssh = 0.0;
    double ssu = 0.0;
    double ssv = 0.0;
};

struct MatlabReference final {
    double triggerTime = 0.0;
    double widthMeters = 0.0;
    double heightMeters = 0.0;
    double centerLatitudeDegrees = 0.0;
    double centerLongitudeDegrees = 0.0;
    double sampleIntervalSeconds = 0.0;
    std::vector<MatlabSample> samples;
};

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

MatlabReference matlabReference() {
    std::ifstream stream(ATS_WAVEVORTEX_MATLAB_REFERENCE_PATH);
    require(static_cast<bool>(stream),
            "unable to open the WaveVortex MATLAB reference fixture");
    std::string line;
    require(static_cast<bool>(std::getline(stream, line)),
            "MATLAB reference fixture is empty");
    MatlabReference result;
    while (std::getline(stream, line)) {
        const auto fields = splitCsvLine(line);
        require(fields.size() == 9,
                "MATLAB reference fixture row has the wrong field count");
        if (fields[0] == "meta") {
            result.triggerTime = std::stod(fields[1]);
            result.widthMeters = std::stod(fields[2]);
            result.heightMeters = std::stod(fields[3]);
            result.centerLatitudeDegrees = std::stod(fields[4]);
            result.centerLongitudeDegrees = std::stod(fields[5]);
            result.sampleIntervalSeconds = std::stod(fields[6]);
        } else {
            require(fields[0] == "sample",
                    "MATLAB reference fixture row kind is unknown");
            result.samples.push_back(
                {std::stod(fields[1]), std::stod(fields[2]),
                 std::stod(fields[3]), std::stod(fields[4]),
                 std::stod(fields[5]), std::stod(fields[6]),
                 std::stod(fields[7]), std::stod(fields[8])});
        }
    }
    require(result.samples.size() == 3,
            "MATLAB reference fixture did not contain one three-sample pass");
    return result;
}

const WVObservationValue& batchValue(const WVObservationBatch& batch,
                                     std::string_view identifier) {
    const auto found = std::find_if(
        batch.values.begin(), batch.values.end(),
        [&](const auto& value) {
            return value.variableIdentifier == identifier;
        });
    require(found != batch.values.end(),
            "observation batch is missing " + std::string(identifier));
    return *found;
}

const WVOutputObserverView& alongTrackObserverView(
    const WVOutputRouteView& route) {
    for (std::size_t index = 0; index < route.observerCount; ++index) {
        const auto& observer = route.observers[index];
        if (observer.record != nullptr &&
            observer.record->typeIdentifier ==
                atwv::observingSystemTypeIdentifier) {
            return observer;
        }
    }
    throw std::runtime_error("output route has no AlongTrack observer");
}

alongtrack::MissionPassDescriptor payloadDescriptor(
    const WVOutputSchedulePayloadSchema& schema,
    const WVOutputSchedulePayload& payload) {
    WVOutputSchedulePayloadIntegerView repeatCycle;
    WVOutputSchedulePayloadIntegerView firstSample;
    WVOutputSchedulePayloadIntegerView sampleCount;
    auto status = payload.integer(schema, atwv::repeatCycleIndexSlot,
                                  repeatCycle);
    if (status) {
        status = payload.integer(schema, atwv::firstSampleIndexSlot,
                                 firstSample);
    }
    if (status) {
        status = payload.integer(schema, atwv::sampleCountSlot, sampleCount);
    }
    requireStatus(status, "AlongTrack payload decoding");
    require(repeatCycle.count == 1 && firstSample.count == 1 &&
                sampleCount.count == 1,
            "AlongTrack payload slots are not scalar integers");
    return {repeatCycle.data[0], firstSample.data[0], sampleCount.data[0]};
}

void requireSameDescriptor(const alongtrack::MissionPassDescriptor& actual,
                           const alongtrack::MissionPassDescriptor& expected,
                           const std::string& context) {
    require(actual.repeatCycleIndex == expected.repeatCycleIndex &&
                actual.firstSampleIndex == expected.firstSampleIndex &&
                actual.sampleCount == expected.sampleCount,
            context);
}

void testCatalogPreflightAndIndependentResolvedObservers() {
    const auto mission = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow window(15000.0, 12000.0, -13.5, 3.3);
    const auto schedule = scheduleRecord(mission, window, 1.0, 1450.0, 1570.0);
    const WVShape2D shape{1, 1};
    auto first = observerRecord("first-along-track", mission, window, 1.0,
                                {"ssh"});
    auto second = observerRecord("second-along-track", mission, window, 1.0,
                                 {"ssu", "ssv"});
    auto source = portableRecord(shape, {first, second}, schedule);

    atwv::resetCounters();
    const auto builtIns = catalog(false);
    require(builtIns->outputSchedules().registration(
                atwv::scheduleTypeIdentifier, atwv::contractVersion) == nullptr &&
                builtIns->observers().registration(
                    atwv::observingSystemTypeIdentifier,
                    atwv::contractVersion) == nullptr,
            "built-in catalog unexpectedly exposes the AlongTrack pair");
    WVPortableObserverDescriptor rejected;
    const auto rejection = WVPortableObserverDescriptor::create(
        source, builtIns, rejected);
    const auto rejectedCounters = atwv::counters();
    require(!rejection && rejected.record().observers.empty() &&
                rejectedCounters.scheduleConstructionCount == 0 &&
                rejectedCounters.observerConfigurationResolutionCount == 0 &&
                rejectedCounters.observerConstructionCount == 0 &&
                rejectedCounters.outputPlanResolutionCount == 0,
            "built-in preflight reached an AlongTrack provider");

    const auto extended = catalog(true);
    WVPortableObserverDescriptor descriptor;
    auto status = WVPortableObserverDescriptor::create(
        source, extended, descriptor);
    requireStatus(status, "extended AlongTrack descriptor construction");
    const auto constructed = atwv::counters();
    require(constructed.scheduleConstructionCount == 0 &&
                constructed.observerConfigurationResolutionCount == 2 &&
                constructed.observerConstructionCount == 2,
            "AlongTrack records were not resolved once during construction");
    const auto* firstResolved =
        descriptor.resolvedObserver(descriptor.observers().at(1));
    const auto* secondResolved =
        descriptor.resolvedObserver(descriptor.observers().at(2));
    require(firstResolved != nullptr && secondResolved != nullptr &&
                firstResolved->implementationHandle() !=
                    secondResolved->implementationHandle(),
            "two AlongTrack records shared one resolved implementation");

    source.observers[1].configuration.values.clear();
    source.observers[2].configuration.values.clear();
    require(descriptor.resolvedObserver(descriptor.observers().at(1))
                    ->configuration()
                    .values.size() == 16 &&
                descriptor.resolvedObserver(descriptor.observers().at(2))
                    ->configuration()
                    .values.size() == 16,
            "later source-record mutation changed resolved observer configuration");
}

void testScheduleCursorRestartAndBoundedStorage() {
    const auto reference = matlabReference();
    const auto geodetic = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow window(
        reference.widthMeters, reference.heightMeters,
        reference.centerLatitudeDegrees, reference.centerLongitudeDegrees);
    auto record = scheduleRecord(geodetic, window,
                                 reference.sampleIntervalSeconds,
                                 0.0, 30000.0);
    const auto immutableRecord = record;
    const auto extended = catalog(true);
    atwv::resetCounters();
    std::shared_ptr<const WVOutputSchedule> schedule;
    auto status = extended->outputSchedules().resolve(record, schedule);
    requireStatus(status, "geodetic AlongTrack schedule construction");
    require(schedule != nullptr &&
                schedule->payloadSchema().identifier() ==
                    atwv::schedulePayloadSchemaIdentifier &&
                schedule->payloadSchema().slotCount() == 4 &&
                schedule->payloadSchema().payloadBytes() ==
                    4 * sizeof(std::int64_t),
            "AlongTrack schedule did not resolve its compact payload schema");
    const auto retainedBefore = schedule->persistentBytes();

    WVOutputScheduleOccurrence first;
    bool available = false;
    status = schedule->peek({}, 0.0, 500.0, first, available);
    requireStatus(status, "first geodetic AlongTrack schedule peek");
    require(available && first.scheduledTime == reference.triggerTime &&
                first.ordinal == 0 &&
                first.proposedCursor.committedOrdinal == 0 &&
                first.proposedCursor.values.schemaIdentifier ==
                    atwv::scheduleCursorSchemaIdentifier,
            "geodetic schedule did not emit the MATLAB pass occurrence");
    const auto& cursorValues = first.proposedCursor.values.values;
    require(cursorValues.size() == 1 &&
                cursorValues.front().name == atwv::cursorDescriptorField &&
                cursorValues.front().valueType() ==
                    WVPortableValueType::integer &&
                cursorValues.front().valueCount() == 4 &&
                first.proposedCursor.values.encodedBytes() <=
                    WVMaximumOutputScheduleCursorBytes,
            "AlongTrack continuation is not one bounded four-integer cursor");
    const auto firstDescriptor =
        payloadDescriptor(schedule->payloadSchema(), first.payload);
    requireSameDescriptor(firstDescriptor, {0, 434, 3},
                          "geodetic occurrence payload changed pass identity");
    const alongtrack::MissionPassSource source(
        geodetic, window, reference.sampleIntervalSeconds);
    const auto expected = source.nextPass(std::nullopt, 0.0, 500.0);
    require(expected.has_value(), "portable pass source omitted MATLAB pass");
    requireSameDescriptor(firstDescriptor, *expected,
                          "schedule payload differs from portable pass source");
    require(schedule->persistentBytes() == retainedBefore,
            "schedule peek retained pass geometry or future occurrences");

    record.configuration.values.clear();
    record.initialTime = -1000.0;
    record.finalTime = -500.0;
    WVOutputScheduleOccurrence repeated;
    status = schedule->peek({}, 0.0, 500.0, repeated, available);
    requireStatus(status, "schedule peek after source mutation");
    require(available && repeated.scheduledTime == first.scheduledTime &&
                repeated.payload.sameValue(first.payload) &&
                repeated.cursorIdentity == first.cursorIdentity,
            "resolved schedule retained mutable source-record references");

    WVOutputScheduleOccurrence second;
    status = schedule->peek(first.proposedCursor, first.scheduledTime,
                            immutableRecord.finalTime, second, available);
    requireStatus(status, "continued geodetic schedule peek");
    require(!available,
            "geodetic schedule invented a second pass in the MATLAB window");

    std::shared_ptr<const WVOutputSchedule> restarted;
    status = extended->outputSchedules().resolve(immutableRecord, restarted);
    requireStatus(status, "restarted geodetic schedule construction");
    requireStatus(restarted->validateCursor(first.proposedCursor),
                  "restarted geodetic cursor validation");
    WVOutputScheduleOccurrence restartedSecond;
    status = restarted->peek(first.proposedCursor, first.scheduledTime,
                             immutableRecord.finalTime, restartedSecond,
                             available);
    requireStatus(status, "restarted geodetic schedule peek");
    require(!available,
            "restart changed the empty geodetic continuation");

    const auto shortRecord = scheduleRecord(
        geodetic, window, 60.0, 0.0, 86400.0);
    const auto longRecord = scheduleRecord(
        geodetic, window, 60.0, 0.0, 180.0 * 86400.0);
    std::shared_ptr<const WVOutputSchedule> shortSchedule;
    std::shared_ptr<const WVOutputSchedule> longSchedule;
    status = extended->outputSchedules().resolve(shortRecord, shortSchedule);
    if (status) {
        status = extended->outputSchedules().resolve(longRecord, longSchedule);
    }
    requireStatus(status, "short/long schedule construction");
    require(shortSchedule->persistentBytes() == longSchedule->persistentBytes() &&
                shortSchedule->persistentBytes() == retainedBefore,
            "schedule retained storage grew with integration-window duration");

    const alongtrack::ProjectionWindow emptyWindow(
        1000.0, 1000.0, 0.0, 0.0);
    const auto emptyRecord = scheduleRecord(
        geodetic, emptyWindow, 60.0, 0.0, 180.0 * 86400.0);
    std::shared_ptr<const WVOutputSchedule> emptySchedule;
    status = extended->outputSchedules().resolve(emptyRecord, emptySchedule);
    requireStatus(status, "empty long-window schedule construction");
    WVOutputScheduleOccurrence emptyOccurrence;
    status = emptySchedule->peek({}, emptyRecord.initialTime,
                                 emptyRecord.finalTime, emptyOccurrence,
                                 available);
    requireStatus(status, "empty long-window schedule discovery");
    require(!available &&
                emptySchedule->persistentBytes() == retainedBefore,
            "empty 180-day schedule retained a trajectory or emitted a pass");

    const auto datelineMission =
        alongtrack::ResolvedMission::fromCatalog("s6a");
    const alongtrack::ProjectionWindow datelineWindow(
        100000.0, 100000.0, 66.0102829, 179.9);
    const auto datelineRecord = scheduleRecord(
        datelineMission, datelineWindow, 1.0, 3300.0, 3400.0);
    std::shared_ptr<const WVOutputSchedule> datelineSchedule;
    status = extended->outputSchedules().resolve(datelineRecord,
                                                 datelineSchedule);
    requireStatus(status, "antimeridian AlongTrack schedule construction");
    WVOutputScheduleOccurrence datelineOccurrence;
    status = datelineSchedule->peek({}, datelineRecord.initialTime,
                                    datelineRecord.finalTime,
                                    datelineOccurrence, available);
    requireStatus(status, "antimeridian AlongTrack schedule discovery");
    const auto datelineDescriptor = payloadDescriptor(
        datelineSchedule->payloadSchema(), datelineOccurrence.payload);
    require(available && datelineOccurrence.scheduledTime == 3341.0 &&
                datelineDescriptor.repeatCycleIndex == 0 &&
                datelineDescriptor.firstSampleIndex == 3341 &&
                datelineDescriptor.sampleCount == 18,
            "antimeridian occurrence or compact payload differs from MATLAB");
    const alongtrack::MissionPassSource datelineSource(
        datelineMission, datelineWindow, 1.0);
    const auto datelineGeometry =
        datelineSource.reconstructPass(datelineDescriptor);
    const auto negativeLongitudeCount = std::count_if(
        datelineGeometry.begin(), datelineGeometry.end(),
        [&](const auto& point) {
            return alongtrack::missionGroundTrackPoint(
                       datelineMission, point.timeSeconds)
                       .longitudeDegrees < 0.0;
        });
    require(negativeLongitudeCount == 8,
            "antimeridian payload did not preserve wrapped pass geometry");

    const auto repeating = alongtrack::ResolvedMission::fromCatalog("j3");
    const alongtrack::ProjectionWindow repeatingWindow(
        3000000.0, 3000000.0, 0.0, 0.0);
    const auto repeatingRecord = scheduleRecord(
        repeating, repeatingWindow, 1.0, 0.0, 30000.0);
    std::shared_ptr<const WVOutputSchedule> repeatingSchedule;
    status = extended->outputSchedules().resolve(repeatingRecord,
                                                 repeatingSchedule);
    requireStatus(status, "repeating AlongTrack schedule construction");
    WVOutputScheduleOccurrence repeatingOccurrence;
    status = repeatingSchedule->peek({}, 0.0, 30000.0,
                                     repeatingOccurrence, available);
    requireStatus(status, "repeating AlongTrack schedule peek");
    const alongtrack::MissionPassSource repeatingSource(
        repeating, repeatingWindow, 1.0);
    const auto repeatingExpected =
        repeatingSource.nextPass(std::nullopt, 0.0, 30000.0);
    require(available && repeatingExpected.has_value(),
            "repeating schedule omitted its first pass");
    requireSameDescriptor(
        payloadDescriptor(repeatingSchedule->payloadSchema(),
                          repeatingOccurrence.payload),
        *repeatingExpected,
        "repeating schedule payload differs from portable pass source");

    WVOutputScheduleOccurrence repeatingSecond;
    status = repeatingSchedule->peek(
        repeatingOccurrence.proposedCursor,
        repeatingOccurrence.scheduledTime, repeatingRecord.finalTime,
        repeatingSecond, available);
    requireStatus(status, "second repeating AlongTrack schedule peek");
    const auto repeatingSecondExpected = repeatingSource.nextPass(
        repeatingExpected, 0.0, repeatingRecord.finalTime);
    require(available && repeatingSecondExpected.has_value() &&
                repeatingSecond.ordinal == 1 &&
                repeatingSecond.scheduledTime >
                    repeatingOccurrence.scheduledTime,
            "repeating schedule did not lazily discover multiple passes");
    requireSameDescriptor(
        payloadDescriptor(repeatingSchedule->payloadSchema(),
                          repeatingSecond.payload),
        *repeatingSecondExpected,
        "second repeating schedule payload differs from portable pass source");

    std::shared_ptr<const WVOutputSchedule> repeatingRestart;
    status = extended->outputSchedules().resolve(repeatingRecord,
                                                 repeatingRestart);
    if (status) {
        status = repeatingRestart->validateCursor(
            repeatingOccurrence.proposedCursor);
    }
    requireStatus(status, "repeating schedule restart cursor validation");
    WVOutputScheduleOccurrence restartedRepeatingSecond;
    status = repeatingRestart->peek(
        repeatingOccurrence.proposedCursor,
        repeatingOccurrence.scheduledTime, repeatingRecord.finalTime,
        restartedRepeatingSecond, available);
    requireStatus(status, "restarted repeating schedule peek");
    require(available &&
                restartedRepeatingSecond.scheduledTime ==
                    repeatingSecond.scheduledTime &&
                restartedRepeatingSecond.ordinal == repeatingSecond.ordinal &&
                restartedRepeatingSecond.cursorIdentity ==
                    repeatingSecond.cursorIdentity &&
                restartedRepeatingSecond.payload.sameValue(
                    repeatingSecond.payload) &&
                samePortableTypedRecordValue(
                    restartedRepeatingSecond.proposedCursor.values,
                    repeatingSecond.proposedCursor.values),
            "restart changed the next repeating occurrence or complete cursor");
    std::cout << "METRIC schedule-persistent-bytes short="
              << shortSchedule->persistentBytes() << " long="
              << longSchedule->persistentBytes() << " cursor="
              << first.proposedCursor.values.encodedBytes() << '\n';
}

WVCheckpoint loadCheckpoint(const WVExtensionCatalog& extensionCatalog) {
    WVCheckpoint checkpoint;
    const auto status = WVCheckpointReader::read(
        ATS_WAVEVORTEX_CHECKPOINT_PATH, extensionCatalog, checkpoint);
    require(static_cast<bool>(status),
            "unable to read the WaveVortex compatibility checkpoint: " +
                status.message);
    return checkpoint;
}

void requireRealValues(const WVObservationValue& value,
                       const std::vector<double>& expected,
                       double absoluteCeiling,
                       double normalizedTolerance,
                       const std::string& context) {
    require(value.scalarType == WVObservationScalarType::real64 &&
                value.elementCount() == expected.size() &&
                (expected.empty() || value.real64Data() != nullptr),
            context + " has the wrong type, extent, or storage");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const double difference =
            std::abs(value.real64Data()[index] - expected[index]);
        const double normalizedDifference =
            difference / std::max(1.0, std::abs(expected[index]));
        if (difference > absoluteCeiling ||
            normalizedDifference > normalizedTolerance) {
            std::ostringstream detail;
            detail << std::setprecision(17) << context << " sample " << index
                   << " expected " << expected[index] << ", got "
                   << value.real64Data()[index] << ", absolute difference "
                   << difference << " (ceiling " << absoluteCeiling
                   << "), normalized difference " << normalizedDifference
                   << " (tolerance " << normalizedTolerance << ')';
            require(false, detail.str());
        }
    }
}

struct DifferenceMetrics final {
    double maximumAbsolute = 0.0;
    double maximumNormalized = 0.0;
};

DifferenceMetrics differenceMetrics(const WVObservationValue& value,
                                    const std::vector<double>& expected) {
    DifferenceMetrics result;
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const double difference =
            std::abs(value.real64Data()[index] - expected[index]);
        result.maximumAbsolute =
            std::max(result.maximumAbsolute, difference);
        result.maximumNormalized = std::max(
            result.maximumNormalized,
            difference / std::max(1.0, std::abs(expected[index])));
    }
    return result;
}

void requireIntegerScalar(const WVObservationBatch& batch,
                          const std::string& identifier,
                          std::int64_t expected) {
    const auto& value = batchValue(batch, identifier);
    require(value.scalarType == WVObservationScalarType::integer64 &&
                value.elementCount() == 1 &&
                value.integer64Data() != nullptr &&
                value.integer64Data()[0] == expected,
            "observation batch has the wrong " + identifier);
}

WVObservationBatch preparedBatch(
    WVObserverOutputEvaluationService& service,
    const WVOutputRouteView& route,
    const WVOutputObserverView& observer) {
    WVObservationOccurrenceIdentity identity;
    auto status = service.preparedOccurrenceIdentity(route, observer, identity);
    requireStatus(status, "prepared AlongTrack occurrence identity");
    WVObservationBatch batch;
    status = service.observationBatch(identity, *observer.record, batch);
    requireStatus(status, "AlongTrack occurrence batch construction");
    WVObservationSchema schema;
    status = service.observationSchema(*observer.record, schema);
    if (status) {
        status = validateObservationBatch(schema, batch);
    }
    requireStatus(status, "AlongTrack occurrence batch validation");
    return batch;
}

void compareWithCentralFieldService(
    const WVObservationBatch& batch,
    const WVState& state,
    const WVTransformConstantStratificationConfiguration& configuration) {
    const auto& x = batchValue(batch, "x");
    const auto& y = batchValue(batch, "y");
    require(x.scalarType == WVObservationScalarType::real64 &&
                y.scalarType == WVObservationScalarType::real64 &&
                x.elementCount() == y.elementCount(),
            "AlongTrack coordinate values cannot be centrally sampled");
    WVFieldSamplingRequest sampling;
    sampling.kind = WVFieldSamplingKind::positions;
    sampling.x.assign(x.real64Data(), x.real64Data() + x.elementCount());
    sampling.y.assign(y.real64Data(), y.real64Data() + y.elementCount());
    sampling.interpolation = WVPositionInterpolation::linear;
    std::vector<WVFieldRequest> requests;
    for (const char* field : {"ssh", "ssu", "ssv"}) {
        requests.push_back({field, field, sampling});
    }
    std::unique_ptr<WVFieldEvaluationService> fields;
    auto status = WVFieldEvaluationService::create(
        configuration, std::make_unique<WVReferenceFFTEngine>(), fields);
    requireStatus(status, "comparison field-service construction");
    WVFieldEvaluationPlan plan;
    status = fields->createPlan(requests, plan);
    requireStatus(status, "comparison field-plan construction");
    std::array<std::vector<double>, 3> storage;
    std::array<WVFieldOutputView, 3> outputs;
    for (std::size_t index = 0; index < outputs.size(); ++index) {
        storage[index].resize(x.elementCount());
        outputs[index] = {storage[index].data(), storage[index].size()};
    }
    status = fields->evaluate(plan, state, outputs.data(), outputs.size());
    requireStatus(status, "central comparison field evaluation");
    for (std::size_t field = 0; field < storage.size(); ++field) {
        const auto& observed = batchValue(
            batch, std::array<const char*, 3>{"ssh", "ssu", "ssv"}[field]);
        require(observed.elementCount() == storage[field].size(),
                "AlongTrack and central field-service extents differ");
        for (std::size_t sample = 0; sample < storage[field].size(); ++sample) {
            requireClose(observed.real64Data()[sample], storage[field][sample],
                         1.0e-12, 0.0,
                         "AlongTrack field differs from central field service");
        }
    }
}

struct ParityContext final {
    MatlabReference reference;
    std::shared_ptr<const WVExtensionCatalog> extensionCatalog;
    WVCheckpoint checkpoint;
    WVOutputScheduleRecord schedule;
    WVPortableObserverDescriptor descriptor;
    WVOutputPlan plan;
    std::unique_ptr<WVObserverOutputEvaluationService> service;
};

ParityContext parityContext(bool coincidentDestinations = false,
                            double finalTime = 1570.0) {
    ParityContext context;
    context.reference = matlabReference();
    context.extensionCatalog = catalog(true);
    context.checkpoint = loadCheckpoint(*context.extensionCatalog);
    const auto mission = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow window(
        context.reference.widthMeters, context.reference.heightMeters,
        context.reference.centerLatitudeDegrees,
        context.reference.centerLongitudeDegrees);
    context.schedule = scheduleRecord(
        mission, window, context.reference.sampleIntervalSeconds,
        0.0, finalTime);
    auto observer = observerRecord(
        "along-track", mission, window,
        context.reference.sampleIntervalSeconds, {"ssh", "ssu", "ssv"});
    auto sourceRecord = portableRecord(
        context.checkpoint.state.coefficients.shape, {std::move(observer)},
        context.schedule, coincidentDestinations);

    auto status = WVPortableObserverDescriptor::create(
        sourceRecord, context.extensionCatalog, context.descriptor);
    requireStatus(status, "AlongTrack parity descriptor construction");
    // A resolved descriptor and implementation must not borrow construction
    // records that MATLAB/catalog authoring code can mutate later.
    sourceRecord.observers.at(1).configuration.values.clear();
    sourceRecord.outputFiles.at(0).groups.at(0).schedule.configuration.values.clear();

    status = WVOutputPlan::create(
        context.descriptor, context.extensionCatalog, 0.0, finalTime, {},
        context.plan);
    requireStatus(status, "AlongTrack parity output planning");
    status = WVObserverOutputEvaluationService::create(
        context.checkpoint.configuration, false, context.descriptor,
        std::make_unique<WVReferenceFFTEngine>(), context.service);
    requireStatus(status, "AlongTrack parity evaluation-service construction");
    return context;
}

WVOutputEvent plannedEvent(const WVOutputPlannedEventView& planned,
                           const WVCheckpoint& checkpoint) {
    WVOutputEvent event;
    event.eventOrdinal = planned.eventOrdinal;
    event.scheduledTime = planned.scheduledTime;
    event.kind = WVOutputEventKind::interpolated;
    event.state.waveVortex = checkpoint.state.view();
    event.state.waveVortex.t = planned.scheduledTime;
    event.routes = planned.routes;
    event.routeCount = planned.routeCount;
    return event;
}

void testMatlabGeometryFieldsAndBatch() {
    atwv::resetCounters();
    auto context = parityContext();
    require(context.plan.eventCount() == 1,
            "MATLAB parity window did not produce exactly one occurrence");
    const auto planned = context.plan.event(0);
    require(planned.routeCount == 1 &&
                planned.scheduledTime == context.reference.triggerTime,
            "MATLAB parity occurrence has the wrong time or route count");
    auto event = plannedEvent(planned, context.checkpoint);
    auto status = context.service->preflight(context.plan);
    if (status) {
        status = context.service->prepare(event);
    }
    requireStatus(status, "AlongTrack MATLAB occurrence preparation");
    const auto& alongTrack = alongTrackObserverView(planned.routes[0]);
    const auto batch = preparedBatch(
        *context.service, planned.routes[0], alongTrack);

    std::vector<double> time;
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> fieldX;
    std::vector<double> fieldY;
    std::vector<double> ssh;
    std::vector<double> ssu;
    std::vector<double> ssv;
    for (const auto& sample : context.reference.samples) {
        time.push_back(sample.time);
        x.push_back(sample.x);
        y.push_back(sample.y);
        fieldX.push_back(sample.fieldX);
        fieldY.push_back(sample.fieldY);
        ssh.push_back(sample.ssh);
        ssu.push_back(sample.ssu);
        ssv.push_back(sample.ssv);
    }
    requireRealValues(batchValue(batch, "sample-time"), time, 0.0, 0.0,
                      "MATLAB sample time");
    requireRealValues(batchValue(batch, "x"), x, 1.0e-9, 1.0e-12,
                      "MATLAB projected x");
    requireRealValues(batchValue(batch, "y"), y, 1.0e-9, 1.0e-12,
                      "MATLAB projected y");
    requireRealValues(batchValue(batch, "x"), fieldX, 0.0, 0.0,
                      "portable field-sampling x");
    requireRealValues(batchValue(batch, "y"), fieldY, 0.0, 0.0,
                      "portable field-sampling y");
    requireRealValues(batchValue(batch, "ssh"), ssh, 1.0e-9, 1.0e-12,
                      "MATLAB sampled ssh");
    requireRealValues(batchValue(batch, "ssu"), ssu, 1.0e-9, 1.0e-12,
                      "MATLAB sampled ssu");
    requireRealValues(batchValue(batch, "ssv"), ssv, 1.0e-9, 1.0e-12,
                      "MATLAB sampled ssv");
    const auto xDifference = differenceMetrics(batchValue(batch, "x"), x);
    const auto yDifference = differenceMetrics(batchValue(batch, "y"), y);
    const auto sshDifference =
        differenceMetrics(batchValue(batch, "ssh"), ssh);
    const auto ssuDifference =
        differenceMetrics(batchValue(batch, "ssu"), ssu);
    const auto ssvDifference =
        differenceMetrics(batchValue(batch, "ssv"), ssv);
    std::cout << std::setprecision(17)
              << "METRIC matlab-parity x-abs="
              << xDifference.maximumAbsolute << " x-norm="
              << xDifference.maximumNormalized << " y-abs="
              << yDifference.maximumAbsolute << " y-norm="
              << yDifference.maximumNormalized << " ssh-abs="
              << sshDifference.maximumAbsolute << " ssh-norm="
              << sshDifference.maximumNormalized << " ssu-abs="
              << ssuDifference.maximumAbsolute << " ssu-norm="
              << ssuDifference.maximumNormalized << " ssv-abs="
              << ssvDifference.maximumAbsolute << " ssv-norm="
              << ssvDifference.maximumNormalized << '\n';
    requireIntegerScalar(batch, "repeat-cycle-index", 0);
    requireIntegerScalar(batch, "first-sample-index", 434);
    requireIntegerScalar(batch, "sample-count", 3);
    compareWithCentralFieldService(batch, event.state.waveVortex,
                                   context.checkpoint.configuration);

    const auto counts = atwv::counters();
    require(counts.observerConfigurationResolutionCount == 1 &&
                counts.observerConstructionCount == 1 &&
                counts.scheduleConstructionCount == 1 &&
                counts.outputPlanResolutionCount >= 1 &&
                counts.occurrencePreparationCount == 1 &&
                counts.batchBuildCount == 1,
            "AlongTrack work was not resolved at observer/event granularity");
    require(context.service->metrics().fieldEvaluationCount == 1 &&
                context.service->metrics().occurrencePreparationCount == 1 &&
                context.service->metrics().occurrenceWorkspaceLiveBytes > 0,
            "central field evaluation or evaluator workspace was bypassed");
}

WVOutputScheduleOccurrence firstOccurrence(
    const std::shared_ptr<const WVExtensionCatalog>& extensionCatalog,
    const WVOutputScheduleRecord& record,
    std::shared_ptr<const WVOutputSchedule>& schedule) {
    auto status = extensionCatalog->outputSchedules().resolve(record, schedule);
    requireStatus(status, "AlongTrack occurrence schedule resolution");
    WVOutputScheduleOccurrence occurrence;
    bool available = false;
    status = schedule->peek({}, record.initialTime, record.finalTime,
                            occurrence, available);
    requireStatus(status, "AlongTrack occurrence discovery");
    require(available, "AlongTrack test schedule emitted no occurrence");
    return occurrence;
}

WVOutputGroupRecord occurrenceGroup(
    std::string identifier,
    const WVOutputScheduleRecord& schedule) {
    WVOutputGroupRecord group;
    group.identifier = std::move(identifier);
    group.name = group.identifier;
    group.schedule = schedule;
    group.observerIdentifiers = {"along-track"};
    return group;
}

WVOutputRouteView occurrenceRoute(
    std::size_t fileOrdinal,
    std::size_t groupOrdinal,
    std::size_t semanticScheduleOrdinal,
    const WVOutputObserverView& observer,
    const WVOutputGroupRecord& group,
    const WVOutputSchedulePayloadSchema& payloadSchema,
    const WVOutputScheduleOccurrence& occurrence) {
    WVOutputRouteView route;
    route.fileOrdinal = fileOrdinal;
    route.groupOrdinal = groupOrdinal;
    route.scheduleOrdinal = occurrence.ordinal;
    route.observers = &observer;
    route.observerCount = 1;
    route.proposedScheduleCursor = &occurrence.proposedCursor.values;
    route.semanticScheduleOrdinal = semanticScheduleOrdinal;
    route.schedulePayloadSchema = &payloadSchema;
    route.schedulePayload = &occurrence.payload;
    route.scheduleCursorIdentity = occurrence.cursorIdentity;
    route.semanticScheduleRecord = &group;
    return route;
}

void testSemanticReuseIsolationAndIdempotentRetry() {
    atwv::resetCounters();
    auto context = parityContext();
    auto status = context.service->preflight(context.plan);
    requireStatus(status, "semantic-identity service preflight");
    std::shared_ptr<const WVOutputSchedule> schedule;
    const auto occurrence = firstOccurrence(
        context.extensionCatalog, context.schedule, schedule);
    const auto& observerRecordValue = context.descriptor.observers().at(1);
    const auto* resolved =
        context.descriptor.resolvedObserver(observerRecordValue);
    require(resolved != nullptr,
            "semantic-identity AlongTrack observer is unresolved");
    WVOutputObserverView observer{1, &observerRecordValue, resolved};

    const auto sameFirst = occurrenceGroup("same-logical-pass", context.schedule);
    const auto sameSecond = occurrenceGroup("same-logical-pass", context.schedule);
    const auto otherLogical = occurrenceGroup("other-logical-pass", context.schedule);
    auto changedSchedule = context.schedule;
    auto centerLongitude = std::find_if(
        changedSchedule.configuration.values.begin(),
        changedSchedule.configuration.values.end(), [](const auto& value) {
            return value.name == atwv::centerLongitudeDegreesField;
        });
    require(centerLongitude != changedSchedule.configuration.values.end(),
            "test schedule has no center-longitude configuration");
    std::get<std::vector<double>>(centerLongitude->storage)[0] += 0.25;
    const auto otherConfiguration =
        occurrenceGroup("same-logical-pass", changedSchedule);

    std::array<WVOutputRouteView, 4> routes{
        occurrenceRoute(0, 0, 10, observer, sameFirst,
                        schedule->payloadSchema(), occurrence),
        occurrenceRoute(1, 0, 10, observer, sameSecond,
                        schedule->payloadSchema(), occurrence),
        occurrenceRoute(2, 0, 11, observer, otherLogical,
                        schedule->payloadSchema(), occurrence),
        occurrenceRoute(3, 0, 12, observer, otherConfiguration,
                        schedule->payloadSchema(), occurrence),
    };
    WVOutputEvent event;
    event.eventOrdinal = 7;
    event.scheduledTime = occurrence.scheduledTime;
    event.kind = WVOutputEventKind::acceptedEndpoint;
    event.state.waveVortex = context.checkpoint.state.view();
    event.state.waveVortex.t = occurrence.scheduledTime;
    event.routes = routes.data();
    event.routeCount = routes.size();

    const auto metricsBefore = context.service->metrics();
    const auto countersBefore = atwv::counters();
    status = context.service->prepare(event);
    requireStatus(status, "coincident AlongTrack occurrence preparation");
    std::array<WVObservationOccurrenceIdentity, 4> identities;
    for (std::size_t index = 0; index < routes.size(); ++index) {
        status = context.service->preparedOccurrenceIdentity(
            routes[index], observer, identities[index]);
        requireStatus(status, "coincident AlongTrack identity lookup");
    }
    require(sameObservationOccurrenceIdentity(identities[0], identities[1]) &&
                samePreparedObservationOccurrenceIdentity(
                    identities[0], identities[1]),
            "coincident destinations did not share one complete occurrence");
    require(!sameObservationOccurrenceIdentity(
                identities[0], identities[2]) &&
                !samePreparedObservationOccurrenceIdentity(
                    identities[0], identities[2]),
            "distinct logical schedule instances shared an occurrence");
    require(!sameObservationOccurrenceIdentity(
                identities[0], identities[3]) &&
                !samePreparedObservationOccurrenceIdentity(
                    identities[0], identities[3]),
            "different construction configuration shared an occurrence");
    const auto metricsAfterPrepare = context.service->metrics();
    const auto countersAfterPrepare = atwv::counters();
    require(metricsAfterPrepare.occurrencePreparationCount -
                    metricsBefore.occurrencePreparationCount ==
                3 &&
                metricsAfterPrepare.occurrenceReuseCount -
                    metricsBefore.occurrenceReuseCount ==
                1 &&
                countersAfterPrepare.occurrencePreparationCount -
                    countersBefore.occurrencePreparationCount ==
                3,
            "occurrences were not reused only under complete key compatibility");

    std::array<WVObservationBatch, 4> batches;
    for (const std::size_t index : std::array<std::size_t, 3>{0, 2, 3}) {
        status = context.service->observationBatch(
            identities[index], observerRecordValue, batches[index]);
        requireStatus(status, "coincident AlongTrack batch construction");
    }
    const auto metricsAfterBatches = context.service->metrics();
    const auto countersAfterBatches = atwv::counters();
    const auto metricBatchDelta =
        metricsAfterBatches.occurrenceBatchBuildCount -
        metricsBefore.occurrenceBatchBuildCount;
    const auto providerBatchDelta = countersAfterBatches.batchBuildCount -
                                    countersBefore.batchBuildCount;
    require(metricBatchDelta == 3 && providerBatchDelta == 3,
            "compatible occurrences rebuilt a data-only batch (service " +
                std::to_string(metricBatchDelta) + ", provider " +
                std::to_string(providerBatchDelta) + ")");

    // Preparing the same in-flight event is idempotent. The driver/sink test
    // below additionally proves that a cached batch survives a route retry.
    status = context.service->prepare(event);
    requireStatus(status, "idempotent AlongTrack retry preparation");
    const auto metricsAfterRetry = context.service->metrics();
    const auto countersAfterRetry = atwv::counters();
    require(metricsAfterRetry.occurrencePreparationCount ==
                metricsAfterBatches.occurrencePreparationCount &&
                metricsAfterRetry.occurrenceBatchBuildCount ==
                    metricsAfterBatches.occurrenceBatchBuildCount &&
                countersAfterRetry.occurrencePreparationCount ==
                    countersAfterBatches.occurrencePreparationCount &&
                countersAfterRetry.batchBuildCount ==
                    countersAfterBatches.batchBuildCount,
            "retry rediscovered geometry, reevaluated fields, or rebuilt a batch");
}

WVOutputScheduleOccurrence syntheticOccurrence(
    const WVPortableTypedRecord& constructionConfiguration,
    const WVOutputSchedulePayloadSchema& schema,
    std::int64_t repeatCycleIndex,
    std::int64_t firstSampleIndex,
    std::int64_t sampleCount,
    std::optional<std::int64_t> identityOverride = std::nullopt) {
    std::int64_t configurationIdentity = 0;
    auto status = atwv::constructionConfigurationIdentity(
        constructionConfiguration, configurationIdentity);
    requireStatus(status, "AlongTrack construction-configuration identity");
    if (identityOverride.has_value()) {
        configurationIdentity = *identityOverride;
    }
    WVOutputScheduleOccurrence occurrence;
    occurrence.scheduledTime =
        static_cast<double>(firstSampleIndex);
    occurrence.ordinal = 0;
    occurrence.proposedCursor.committedOrdinal = 0;
    occurrence.proposedCursor.values.schemaIdentifier =
        atwv::scheduleCursorSchemaIdentifier;
    occurrence.proposedCursor.values.schemaVersion = atwv::contractVersion;
    occurrence.proposedCursor.values.values = {
        {atwv::cursorDescriptorField,
         {4},
         std::vector<std::int64_t>{repeatCycleIndex, firstSampleIndex,
                                   sampleCount, configurationIdentity}}};
    status = occurrence.payload.reset(schema);
    if (status) {
        status = occurrence.payload.setInteger(
            schema, atwv::repeatCycleIndexSlot, &repeatCycleIndex, 1);
    }
    if (status) {
        status = occurrence.payload.setInteger(
            schema, atwv::firstSampleIndexSlot, &firstSampleIndex, 1);
    }
    if (status) {
        status = occurrence.payload.setInteger(
            schema, atwv::sampleCountSlot, &sampleCount, 1);
    }
    if (status) {
        status = occurrence.payload.setInteger(
            schema, atwv::constructionConfigurationIdentitySlot,
            &configurationIdentity, 1);
    }
    requireStatus(status, "synthetic AlongTrack occurrence payload");
    occurrence.cursorIdentity = occurrence.payload.valueFingerprint();
    return occurrence;
}

void testZeroLengthAndPayloadCompatibility() {
    atwv::resetCounters();
    auto context = parityContext();
    auto status = context.service->preflight(context.plan);
    requireStatus(status, "zero-length service preflight");
    const auto metricsBeforeZero = context.service->metrics();
    const auto& record = context.descriptor.observers().at(1);
    const auto* resolved = context.descriptor.resolvedObserver(record);
    require(resolved != nullptr, "zero-length AlongTrack observer is unresolved");
    WVOutputObserverView observer{1, &record, resolved};
    WVOutputSchedulePayloadSchema schema;
    status = atwv::createSchedulePayloadSchema(schema);
    requireStatus(status, "zero-length payload schema construction");
    auto occurrence = syntheticOccurrence(
        record.configuration, schema, 0, 434, 0);
    const auto group = occurrenceGroup("zero-length", context.schedule);
    auto route = occurrenceRoute(0, 0, 20, observer, group, schema, occurrence);
    WVOutputEvent event;
    event.eventOrdinal = 20;
    event.scheduledTime = occurrence.scheduledTime;
    event.kind = WVOutputEventKind::acceptedEndpoint;
    event.state.waveVortex = context.checkpoint.state.view();
    event.state.waveVortex.t = occurrence.scheduledTime;
    event.routes = &route;
    event.routeCount = 1;
    status = context.service->prepare(event);
    requireStatus(status, "zero-length AlongTrack occurrence preparation");
    const auto batch = preparedBatch(*context.service, route, observer);
    for (const char* identifier : {"sample-time", "x", "y", "ssh", "ssu",
                                   "ssv"}) {
        require(batchValue(batch, identifier).elementCount() == 0,
                std::string("zero-length occurrence retained phantom ") +
                    identifier + " values");
    }
    requireIntegerScalar(batch, "repeat-cycle-index", 0);
    requireIntegerScalar(batch, "first-sample-index", 434);
    requireIntegerScalar(batch, "sample-count", 0);
    require(context.service->metrics().fieldEvaluationCount ==
                metricsBeforeZero.fieldEvaluationCount + 1,
            "zero-length occurrence did not stay within one coarse central "
            "field-service call");

    const auto countersAfterZero = atwv::counters();
    context.service->complete(event);
    auto incompatible = syntheticOccurrence(
        record.configuration, schema, 0, 434, 0,
        static_cast<std::int64_t>(
            std::numeric_limits<std::int64_t>::max()));
    route = occurrenceRoute(0, 0, 20, observer, group, schema, incompatible);
    event.eventOrdinal = 21;
    event.routes = &route;
    const auto rejected = context.service->prepare(event);
    const auto countersAfterRejection = atwv::counters();
    require(!rejected &&
                countersAfterRejection.batchBuildCount ==
                    countersAfterZero.batchBuildCount,
            "payload/configuration mismatch reached batch construction");
}

void testEvaluatorStorageDoesNotScaleWithWindowDuration() {
    auto shortContext = parityContext(false, 1570.0);
    auto longContext = parityContext(false, 180.0 * 86400.0);
    require(shortContext.plan.persistentBytes() ==
                longContext.plan.persistentBytes() &&
                shortContext.service->persistentBytes() ==
                    longContext.service->persistentBytes(),
            "immutable plan/service storage grew with integration duration");
    const auto shortPlanned = shortContext.plan.event(0);
    const auto longPlanned = longContext.plan.event(0);
    require(shortPlanned.scheduledTime == longPlanned.scheduledTime,
            "long-window planning changed the first pass");
    auto shortEvent = plannedEvent(shortPlanned, shortContext.checkpoint);
    auto longEvent = plannedEvent(longPlanned, longContext.checkpoint);
    auto status = shortContext.service->preflight(shortContext.plan);
    if (status) {
        status = longContext.service->preflight(longContext.plan);
    }
    if (status) {
        status = shortContext.service->prepare(shortEvent);
    }
    if (status) {
        status = longContext.service->prepare(longEvent);
    }
    requireStatus(status, "short/long occurrence preparation");
    require(shortContext.service->occurrenceWorkspaceRetainedBytes() ==
                longContext.service->occurrenceWorkspaceRetainedBytes() &&
                shortContext.service->occurrenceWorkspaceLiveBytes() ==
                    longContext.service->occurrenceWorkspaceLiveBytes() &&
                shortContext.service->metrics()
                        .occurrenceWorkspaceMaximumLiveBytes ==
                    longContext.service->metrics()
                        .occurrenceWorkspaceMaximumLiveBytes,
            "current-event evaluator storage grew with integration duration");
    std::cout << "METRIC evaluator-storage short-plan="
              << shortContext.plan.persistentBytes() << " long-plan="
              << longContext.plan.persistentBytes() << " short-service="
              << shortContext.service->persistentBytes() << " long-service="
              << longContext.service->persistentBytes() << " retained="
              << shortContext.service->occurrenceWorkspaceRetainedBytes()
              << " live="
              << shortContext.service->occurrenceWorkspaceLiveBytes()
              << " max-live="
              << shortContext.service->metrics()
                     .occurrenceWorkspaceMaximumLiveBytes
              << '\n';
}

class ZeroErrorPolicy final : public WVIntegrationErrorPolicy {
public:
    explicit ZeroErrorPolicy(const WVIntegrationStateLayout& layout)
        : layout_(layout) {}

    std::size_t componentCount() const noexcept override {
        return 3 + layout_.additionalBlocks().size();
    }

    std::size_t elementCount(std::size_t component) const noexcept override {
        return component < 3
                   ? layout_.coefficientShape().elementCount()
                   : layout_.additionalBlocks()[component - 3].elementCount;
    }

    double absoluteTolerance(std::size_t,
                             std::size_t) const noexcept override {
        return 1.0e-10;
    }

    std::size_t persistentBytes() const noexcept override {
        return sizeof(*this);
    }

private:
    const WVIntegrationStateLayout& layout_;
};

class ZeroIntegrationSystem final : public WVIntegrationSystem {
public:
    explicit ZeroIntegrationSystem(WVIntegrationStateLayout layout)
        : layout_(std::move(layout)) {}

    const WVIntegrationStateLayout& stateLayout() const noexcept override {
        return layout_;
    }

    WVKernelStatus evaluateRightHandSide(
        const WVIntegrationState&,
        WVIntegrationFlux& rightHandSide) override {
        const auto coefficientCount =
            layout_.coefficientShape().elementCount();
        for (auto output : {rightHandSide.waveVortex.Fp,
                            rightHandSide.waveVortex.Fm,
                            rightHandSide.waveVortex.F0}) {
            std::fill_n(output.data, coefficientCount, WVComplex64{});
        }
        for (std::size_t block = 0;
             block < rightHandSide.additionalBlockCount; ++block) {
            const auto& metadata = layout_.additionalBlocks()[block];
            if (metadata.scalarType == WVStateScalarType::real64) {
                std::fill_n(rightHandSide.additionalBlocks[block].realData,
                            metadata.elementCount, 0.0);
            } else {
                std::fill_n(rightHandSide.additionalBlocks[block].complexData,
                            metadata.elementCount, WVComplex64{});
            }
        }
        return WVKernelStatus::ok();
    }

    WVStateConstraintResult enforceStateConstraints(
        WVMutableIntegrationState&) override {
        return {WVKernelStatus::ok(), 0, true};
    }

    WVKernelStatus createErrorPolicy(
        double,
        std::unique_ptr<WVIntegrationErrorPolicy>& policy) const override {
        policy = std::make_unique<ZeroErrorPolicy>(layout_);
        return WVKernelStatus::ok();
    }

private:
    WVIntegrationStateLayout layout_;
};

struct MutableState final {
    WVShape2D shape;
    std::vector<WVComplex64> Ap;
    std::vector<WVComplex64> Am;
    std::vector<WVComplex64> A0;
    WVAdditionalStateStorage additional;
    WVMutableIntegrationState view;

    MutableState(const WVIntegrationStateLayout& layout,
                 const WVCheckpointState& checkpoint,
                 double initialTime)
        : shape(layout.coefficientShape()),
          Ap(checkpoint.coefficients.Ap),
          Am(checkpoint.coefficients.Am),
          A0(checkpoint.coefficients.A0) {
        require(shape.rows == checkpoint.coefficients.shape.rows &&
                    shape.columns == checkpoint.coefficients.shape.columns &&
                    Ap.size() == shape.elementCount() &&
                    Am.size() == shape.elementCount() &&
                    A0.size() == shape.elementCount(),
                "retry state shape differs from checkpoint coefficients");
        requireStatus(additional.initialize(layout),
                      "retry additional-state allocation");
        view = {{initialTime,
                 checkpoint.t0,
                 {{Ap.data(), shape}, {Am.data(), shape},
                  {A0.data(), shape}}},
                additional.mutableBlocks(), additional.blockCount()};
    }
};

class InjectedRetrySink final : public WVOutputSink {
public:
    explicit InjectedRetrySink(WVObserverOutputEvaluationService& source)
        : source_(source) {}

    WVKernelStatus preflight(const WVOutputPlan& plan) override {
        return source_.preflight(plan);
    }

    WVKernelStatus deliver(const WVOutputEvent& event,
                           const WVOutputRouteView& route,
                           WVOutputDeliveryResult& result) override {
        auto status = source_.prepare(event);
        if (!status) {
            return status;
        }
        const auto& observer = alongTrackObserverView(route);
        WVObservationOccurrenceIdentity identity;
        status = source_.preparedOccurrenceIdentity(route, observer, identity);
        if (!status) {
            return status;
        }
        if (!cachedBatch_.has_value() ||
            !samePreparedObservationOccurrenceIdentity(identity,
                                                       cachedIdentity_)) {
            WVObservationBatch batch;
            status = source_.observationBatch(identity, *observer.record,
                                              batch);
            if (!status) {
                return status;
            }
            cachedIdentity_ = identity;
            cachedBatch_ = std::move(batch);
        }
        if (route.fileOrdinal >= attempts_.size()) {
            return {WVKernelStatusCode::invalidConfiguration,
                    "unexpected retry route ordinal"};
        }
        ++attempts_[route.fileOrdinal];
        if (failureEnabled_ && route.fileOrdinal == 1 &&
            attempts_[route.fileOrdinal] == 1) {
            return {WVKernelStatusCode::numericalFailure,
                    "injected AlongTrack route failure"};
        }
        ++successfulRoutes_;
        result.writeCount = route.observerCount;
        result.writtenBytes = cachedBatch_->metrics().liveBytes;
        return WVKernelStatus::ok();
    }

    void disableFailure() noexcept { failureEnabled_ = false; }

    const std::array<std::size_t, 2>& attempts() const noexcept {
        return attempts_;
    }

    std::size_t successfulRoutes() const noexcept {
        return successfulRoutes_;
    }

private:
    WVObserverOutputEvaluationService& source_;
    WVObservationOccurrenceIdentity cachedIdentity_;
    std::optional<WVObservationBatch> cachedBatch_;
    std::array<std::size_t, 2> attempts_{};
    std::size_t successfulRoutes_ = 0;
    bool failureEnabled_ = true;
};

void testOutputDriverFailureRetryAndCommitOwnership() {
    atwv::resetCounters();
    auto context = parityContext(true);
    ZeroIntegrationSystem system(context.plan.stateLayout());
    MutableState state(system.stateLayout(), context.checkpoint.state, 0.0);
    WVFixedStepRK4 integrator(system, WVFixedStepRK4Options{true});
    auto status = integrator.prepareStateAfterRestart(state.view);
    requireStatus(status, "retry integrator restart preparation");
    InjectedRetrySink sink(*context.service);
    WVOutputDriver driver(integrator, context.plan);
    status = driver.advanceToTime(state.view, 1570.0, 60.0, sink);
    require(!status && driver.hasPendingDelivery() &&
                sink.attempts() == std::array<std::size_t, 2>{1, 1},
            "later-route failure did not retain one pending occurrence");
    const auto countersAtFailure = atwv::counters();
    const auto metricsAtFailure = context.service->metrics();
    sink.disableFailure();
    status = driver.advanceToTime(state.view, 1570.0, 60.0, sink);
    requireStatus(status, "AlongTrack route retry");
    require(!driver.hasPendingDelivery() && state.view.waveVortex.t == 1570.0 &&
                sink.attempts() == std::array<std::size_t, 2>{1, 2} &&
                sink.successfulRoutes() == 2,
            "retry repeated a successful destination or failed to commit");
    const auto countersAfterRetry = atwv::counters();
    const auto metricsAfterRetry = context.service->metrics();
    require(countersAfterRetry.occurrencePreparationCount ==
                countersAtFailure.occurrencePreparationCount &&
                countersAfterRetry.batchBuildCount ==
                    countersAtFailure.batchBuildCount &&
                metricsAfterRetry.occurrencePreparationCount ==
                    metricsAtFailure.occurrencePreparationCount &&
                metricsAfterRetry.occurrenceBatchBuildCount ==
                    metricsAtFailure.occurrenceBatchBuildCount,
            "retry regenerated occurrence geometry, fields, or batch");
    require(driver.metrics().generatedSemanticOccurrenceCount == 1 &&
                driver.metrics().deliveryAttemptCount == 3 &&
                driver.metrics().committedDeliveryCount == 2 &&
                driver.metrics().failureCount == 1,
            "driver semantic occurrence or route-commit metrics are wrong");
    const auto committedAlongTrack = std::count_if(
        driver.committedContinuations().begin(),
        driver.committedContinuations().end(), [](const auto& continuation) {
            return continuation.cursor.committedOrdinal == 0 &&
                   continuation.cursor.values.schemaIdentifier ==
                       atwv::scheduleCursorSchemaIdentifier;
        });
    require(committedAlongTrack == 2,
            "successful retry did not commit the complete schedule cursor");
}

void testMalformedConstructionRejectedBeforeImplementation() {
    const auto mission = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow window(15000.0, 12000.0, -13.5, 3.3);
    auto malformedSchedule =
        scheduleRecord(mission, window, 1.0, 1450.0, 1570.0);
    const auto cadence = std::find_if(
        malformedSchedule.configuration.values.begin(),
        malformedSchedule.configuration.values.end(), [](const auto& value) {
            return value.name == atwv::sampleIntervalSecondsField;
        });
    require(cadence != malformedSchedule.configuration.values.end(),
            "malformed schedule fixture has no cadence");
    std::get<std::vector<double>>(cadence->storage)[0] = 0.0;
    const auto extended = catalog(true);
    atwv::resetCounters();
    std::shared_ptr<const WVOutputSchedule> schedule;
    auto status = extended->outputSchedules().resolve(malformedSchedule,
                                                      schedule);
    require(!status && schedule == nullptr &&
                atwv::counters().scheduleConstructionCount == 0,
            "invalid cadence reached schedule implementation construction");

    auto malformedObserver = observerRecord(
        "malformed-along-track", mission, window, 1.0, {"ssh"});
    auto interpolation = std::find_if(
        malformedObserver.configuration.values.begin(),
        malformedObserver.configuration.values.end(), [](const auto& value) {
            return value.name == atwv::interpolationField;
        });
    require(interpolation != malformedObserver.configuration.values.end(),
            "malformed observer fixture has no interpolation");
    std::get<std::vector<std::int64_t>>(interpolation->storage)[0] = 2;
    const auto validSchedule =
        scheduleRecord(mission, window, 1.0, 1450.0, 1570.0);
    auto record = portableRecord({1, 1}, {std::move(malformedObserver)},
                                 validSchedule);
    WVPortableObserverDescriptor descriptor;
    status = WVPortableObserverDescriptor::create(record, extended,
                                                   descriptor);
    require(!status && atwv::counters().observerConstructionCount == 0,
            "invalid observer configuration reached implementation construction");
}

} // namespace

int main() {
    try {
        testCatalogPreflightAndIndependentResolvedObservers();
        testScheduleCursorRestartAndBoundedStorage();
        testMatlabGeometryFieldsAndBatch();
        testSemanticReuseIsolationAndIdempotentRetry();
        testZeroLengthAndPayloadCompatibility();
        testEvaluatorStorageDoesNotScaleWithWindowDuration();
        testOutputDriverFailureRetryAndCommitOwnership();
        testMalformedConstructionRejectedBeforeImplementation();
        std::cout << "PASS: AlongTrack WaveVortex extension contracts\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
