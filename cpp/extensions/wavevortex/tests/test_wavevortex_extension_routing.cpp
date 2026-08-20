#include "alongtrack/portable_core.hpp"
#include "alongtrack/wavevortex_extension.hpp"

#include "WaveVortexRuntime/WVModelOutputConfiguration.hpp"
#include "WaveVortexRuntime/WVObserverOutputEvaluationService.hpp"
#include "WaveVortexRuntime/WVRungeKutta.hpp"

#include "WVReferenceFFTEngine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

WVTransformConstantStratificationConfiguration modelConfiguration() {
    WVTransformConstantStratificationConfiguration configuration;
    configuration.Nx = 8;
    configuration.Ny = 6;
    configuration.Nz = 7;
    configuration.Nj = 6;
    configuration.Lx = 15000.0;
    configuration.Ly = 12000.0;
    configuration.Lz = 1200.0;
    configuration.N0 = 5.2e-3;
    configuration.rho0 = 1025.0;
    configuration.g = 9.81;
    configuration.planetaryRadius = 6.371e6;
    configuration.rotationRate = 7.2921e-5;
    configuration.latitude = -13.5;
    configuration.shouldAntialias = true;
    return configuration;
}

WVShape2D coefficientShape(
    const WVTransformConstantStratificationConfiguration& configuration) {
    WVTransformConstantStratificationDescriptor descriptor;
    requireStatus(WVTransformConstantStratificationDescriptor::create(
                      configuration, descriptor),
                  "routing transform descriptor construction");
    return descriptor.spectralShape();
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
                      window.origin() ==
                          alongtrack::WindowOrigin::lowerLeft),
        realScalar(atwv::sampleIntervalSecondsField,
                   sampleIntervalSeconds),
    };
}

WVOutputScheduleRecord scheduleRecord(
    const alongtrack::ResolvedMission& mission,
    const alongtrack::ProjectionWindow& window,
    double sampleIntervalSeconds,
    double initialTime,
    double finalTime) {
    WVOutputScheduleRecord record;
    record.initialTime = initialTime;
    record.finalTime = finalTime;
    record.typeIdentifier = atwv::scheduleTypeIdentifier;
    record.contractVersion = atwv::contractVersion;
    record.configuration.schemaIdentifier =
        atwv::scheduleConfigurationSchemaIdentifier;
    record.configuration.schemaVersion = atwv::contractVersion;
    addResolvedMissionConfiguration(record.configuration, mission, window,
                                    sampleIntervalSeconds);
    return record;
}

WVObserverRecord observerRecord(
    std::string identifier,
    const alongtrack::ResolvedMission& mission,
    const alongtrack::ProjectionWindow& window,
    double sampleIntervalSeconds,
    std::vector<std::string> fields) {
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
                                    sampleIntervalSeconds);
    observer.configuration.values.push_back(
        {atwv::fieldNamesField, {fields.size()}, std::move(fields)});
    observer.configuration.values.push_back(
        integerScalar(atwv::interpolationField, 0));
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

WVOutputGroupRecord outputGroup(
    std::string identifier,
    const WVOutputScheduleRecord& schedule,
    std::vector<std::string> observers) {
    WVOutputGroupRecord group;
    group.identifier = std::move(identifier);
    group.name = group.identifier;
    group.schedule = schedule;
    group.observerIdentifiers = std::move(observers);
    return group;
}

WVOutputFileRecord outputFile(std::string identifier,
                              std::string destination,
                              WVOutputGroupRecord group) {
    WVOutputFileRecord file;
    file.identifier = std::move(identifier);
    file.destination = std::move(destination);
    WVOutputGroupRecord restart;
    restart.identifier = "restart";
    restart.name = "restart";
    restart.schedule = {1.0, 2000.0, 2000.0};
    restart.observerIdentifiers = {"coefficients"};
    restart.containsCompleteCoefficientRestart = true;
    file.groups.push_back(std::move(restart));
    file.groups.push_back(std::move(group));
    return file;
}

WVPortableObserverRecord portableRecord(
    WVShape2D shape,
    std::vector<WVObserverRecord> observers,
    std::vector<WVOutputFileRecord> files) {
    WVPortableObserverRecord record;
    addCoefficientStateBlocks(record, shape);
    record.observers.push_back(coefficientObserver());
    for (auto& observer : observers) {
        record.observers.push_back(std::move(observer));
    }
    record.outputFiles = std::move(files);
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
    requireStatus(status, "routing extension catalog construction");
    return result;
}

std::string memoryDestination(const char* stem) {
    static const int identity = 0;
    const auto suffix = static_cast<unsigned long long>(
        reinterpret_cast<std::uintptr_t>(&identity));
    return (std::filesystem::temp_directory_path() /
            (std::string("ats3-") + stem + "-" +
             std::to_string(suffix) + ".memory"))
        .string();
}

bool noExtensionConstruction(const atwv::ExtensionCounters& counters) {
    return counters.scheduleConstructionCount == 0 &&
           counters.schedulePeekCount == 0 &&
           counters.observerConfigurationResolutionCount == 0 &&
           counters.observerConstructionCount == 0 &&
           counters.outputPlanResolutionCount == 0 &&
           counters.occurrencePreparationCount == 0 &&
           counters.batchBuildCount == 0;
}

struct PipelineProbe final {
    std::size_t destinationConstructionCount = 0;
    std::size_t stateSizedAllocationCount = 0;
    std::size_t integrationAdvanceCount = 0;
};

void testModelOutputCatalogPreflight() {
    const auto configuration = modelConfiguration();
    const auto shape = coefficientShape(configuration);
    const auto mission = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow window(
        configuration.Lx, configuration.Ly, configuration.latitude, 3.3);
    const auto schedule = scheduleRecord(mission, window, 1.0, 1450.0,
                                         1570.0);
    const auto observer = observerRecord("along-track", mission, window, 1.0,
                                         {"ssh"});
    const auto destination = memoryDestination("preflight");
    require(!std::filesystem::exists(destination),
            "preflight memory destination unexpectedly exists");
    const auto canonical = portableRecord(
        shape, {observer},
        {outputFile("primary", destination,
                    outputGroup("along-track", schedule,
                                {"along-track"}))});

    atwv::resetCounters();
    WVModelOutputConfiguration rejected;
    const auto rejection = WVModelOutputConfiguration::compile(
        canonical, {}, {}, WVModelOutputPolicy::create, catalog(false),
        1450.0, 1570.0, rejected, &configuration, false);
    require(!rejection && noExtensionConstruction(atwv::counters()) &&
                !std::filesystem::exists(destination),
            "built-in-only model-output preflight reached an AlongTrack "
            "provider or created its destination");

    WVModelOutputConfiguration accepted;
    const auto extended = catalog(true);
    const auto status = WVModelOutputConfiguration::compile(
        canonical, {}, {}, WVModelOutputPolicy::create, extended, 1450.0,
        1570.0, accepted, &configuration, false);
    requireStatus(status, "explicitly extended model-output compilation");
    const auto constructed = atwv::counters();
    require(accepted.plan().groupCount() == 2 &&
                accepted.plan().eventCount() == 1 &&
                constructed.scheduleConstructionCount > 0 &&
                constructed.observerConfigurationResolutionCount > 0 &&
                constructed.observerConstructionCount > 0 &&
                !std::filesystem::exists(destination),
            "extended catalog did not resolve the same canonical AlongTrack "
            "record without creating its destination");
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
        ++rightHandSideEvaluationCount_;
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

    std::size_t rightHandSideEvaluationCount() const noexcept {
        return rightHandSideEvaluationCount_;
    }

private:
    WVIntegrationStateLayout layout_;
    std::size_t rightHandSideEvaluationCount_ = 0;
};

struct MutableState final {
    WVShape2D shape;
    std::vector<WVComplex64> Ap;
    std::vector<WVComplex64> Am;
    std::vector<WVComplex64> A0;
    WVAdditionalStateStorage additional;
    WVMutableIntegrationState view;

    MutableState(const WVIntegrationStateLayout& layout,
                 double initialTime,
                 PipelineProbe* probe = nullptr)
        : shape(layout.coefficientShape()),
          Ap(shape.elementCount()),
          Am(shape.elementCount()),
          A0(shape.elementCount()) {
        if (probe != nullptr) {
            ++probe->stateSizedAllocationCount;
        }
        requireStatus(additional.initialize(layout),
                      "routing additional-state allocation");
        for (std::size_t index = 0; index < shape.elementCount(); ++index) {
            const auto scalar = static_cast<double>(index + 1);
            Ap[index] = {1.0e-3 * std::sin(0.13 * scalar),
                         1.0e-3 * std::cos(0.17 * scalar)};
            Am[index] = {-8.0e-4 * std::cos(0.11 * scalar),
                         7.0e-4 * std::sin(0.19 * scalar)};
            A0[index] = {6.0e-4 * std::sin(0.07 * scalar),
                         5.0e-4 * std::cos(0.23 * scalar)};
        }
        view = {{initialTime,
                 initialTime,
                 {{Ap.data(), shape}, {Am.data(), shape},
                  {A0.data(), shape}}},
                additional.mutableBlocks(), additional.blockCount()};
    }
};

const WVOutputObserverView& alongTrackObserver(
    const WVOutputRouteView& route,
    const char* identifier = "along-track") {
    for (std::size_t index = 0; index < route.observerCount; ++index) {
        const auto& observer = route.observers[index];
        if (observer.record != nullptr &&
            observer.record->identifier == identifier) {
            return observer;
        }
    }
    throw std::runtime_error("output route has no requested AlongTrack observer");
}

class MemoryRetrySink final : public WVOutputSink {
public:
    MemoryRetrySink(WVObserverOutputEvaluationService& source,
                    PipelineProbe& probe)
        : source_(source) {
        ++probe.destinationConstructionCount;
    }

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
        const auto& observer = alongTrackObserver(route);
        WVObservationOccurrenceIdentity identity;
        status = source_.preparedOccurrenceIdentity(route, observer, identity);
        if (!status) {
            return status;
        }
        ++identityCount_;
        if (!hasIdentity_) {
            firstIdentity_ = identity;
            hasIdentity_ = true;
        } else {
            semanticIdentityPreserved_ =
                semanticIdentityPreserved_ &&
                sameObservationOccurrenceIdentity(firstIdentity_, identity);
            preparedIdentityPreserved_ =
                preparedIdentityPreserved_ &&
                samePreparedObservationOccurrenceIdentity(firstIdentity_,
                                                          identity);
        }

        if (!hasBatch_) {
            status = source_.observationBatch(identity, *observer.record,
                                              batch_);
            if (status) {
                WVObservationSchema schema;
                status = source_.observationSchema(*observer.record, schema);
                if (status) {
                    status = validateObservationBatch(schema, batch_);
                }
            }
            if (!status) {
                return status;
            }
            hasBatch_ = true;
            ++batchBuildCount_;
        }

        if (route.fileOrdinal >= attempts_.size()) {
            return {WVKernelStatusCode::invalidConfiguration,
                    "unexpected memory-route ordinal"};
        }
        ++attempts_[route.fileOrdinal];
        if (failureEnabled_ && route.fileOrdinal == 1 &&
            attempts_[route.fileOrdinal] == 1) {
            return {WVKernelStatusCode::numericalFailure,
                    "injected later AlongTrack memory-route failure"};
        }
        ++successfulRouteCount_;
        result.writeCount = 1;
        result.writtenBytes = batch_.metrics().liveBytes;
        if (&route == event.routes + event.routeCount - 1) {
            source_.complete(event);
        }
        return WVKernelStatus::ok();
    }

    void disableFailure() noexcept { failureEnabled_ = false; }

    const std::array<std::size_t, 2>& attempts() const noexcept {
        return attempts_;
    }

    std::size_t identityCount() const noexcept { return identityCount_; }
    std::size_t batchBuildCount() const noexcept { return batchBuildCount_; }
    std::size_t successfulRouteCount() const noexcept {
        return successfulRouteCount_;
    }
    bool semanticIdentityPreserved() const noexcept {
        return semanticIdentityPreserved_;
    }
    bool preparedIdentityPreserved() const noexcept {
        return preparedIdentityPreserved_;
    }

private:
    WVObserverOutputEvaluationService& source_;
    WVObservationOccurrenceIdentity firstIdentity_;
    WVObservationBatch batch_;
    std::array<std::size_t, 2> attempts_{};
    std::size_t identityCount_ = 0;
    std::size_t batchBuildCount_ = 0;
    std::size_t successfulRouteCount_ = 0;
    bool hasIdentity_ = false;
    bool hasBatch_ = false;
    bool semanticIdentityPreserved_ = true;
    bool preparedIdentityPreserved_ = true;
    bool failureEnabled_ = true;
};

struct PipelineAttempt final {
    bool compilationSucceeded = false;
    bool runtimeSucceeded = false;
    PipelineProbe probe;
    atwv::ExtensionCounters counters;
    bool destinationExists = false;
    std::string runtimeMessage;
};

PipelineAttempt attemptGuardedPipeline(bool useExtendedCatalog) {
    const auto configuration = modelConfiguration();
    const auto shape = coefficientShape(configuration);
    const auto mission = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow window(
        configuration.Lx, configuration.Ly, configuration.latitude, 3.3);
    const auto schedule = scheduleRecord(mission, window, 1.0, 1450.0,
                                         1570.0);
    const auto observer = observerRecord("along-track", mission, window, 1.0,
                                         {"ssh"});
    const auto destination = memoryDestination(
        useExtendedCatalog ? "guard-extended" : "guard-built-in");
    const auto canonical = portableRecord(
        shape, {observer},
        {outputFile("primary", destination,
                    outputGroup("along-track", schedule,
                                {"along-track"}))});

    atwv::resetCounters();
    PipelineAttempt result;
    WVModelOutputConfiguration output;
    auto status = WVModelOutputConfiguration::compile(
        canonical, {}, {}, WVModelOutputPolicy::create,
        catalog(useExtendedCatalog), 1450.0, 1570.0, output,
        &configuration, false);
    result.compilationSucceeded = static_cast<bool>(status);
    if (status) {
        std::unique_ptr<WVObserverOutputEvaluationService> source;
        status = WVObserverOutputEvaluationService::create(
            configuration, false, output.descriptor(),
            std::make_unique<WVReferenceFFTEngine>(), source);
        if (status) {
            ZeroIntegrationSystem system(output.plan().stateLayout());
            MutableState state(system.stateLayout(), 1450.0, &result.probe);
            WVFixedStepRK4 integrator(system, WVFixedStepRK4Options{true});
            status = integrator.prepareStateAfterRestart(state.view);
            if (status) {
                MemoryRetrySink sink(*source, result.probe);
                WVOutputDriver driver(integrator, output.plan());
                ++result.probe.integrationAdvanceCount;
                status = driver.advanceToTime(state.view, 1570.0, 60.0,
                                              sink);
            }
        }
        result.runtimeSucceeded = static_cast<bool>(status);
    }
    result.counters = atwv::counters();
    result.destinationExists = std::filesystem::exists(destination);
    result.runtimeMessage = status.message;
    return result;
}

void testCatalogPreflightGuardsRuntimeAssembly() {
    const auto rejected = attemptGuardedPipeline(false);
    require(!rejected.compilationSucceeded &&
                !rejected.runtimeSucceeded &&
                noExtensionConstruction(rejected.counters) &&
                rejected.probe.destinationConstructionCount == 0 &&
                rejected.probe.stateSizedAllocationCount == 0 &&
                rejected.probe.integrationAdvanceCount == 0 &&
                !rejected.destinationExists,
            "built-in catalog rejection did not precede provider "
            "construction, state allocation, integration, and output");

    const auto accepted = attemptGuardedPipeline(true);
    require(accepted.compilationSucceeded && accepted.runtimeSucceeded &&
                accepted.counters.scheduleConstructionCount > 0 &&
                accepted.counters.observerConstructionCount > 0 &&
                accepted.probe.destinationConstructionCount == 1 &&
                accepted.probe.stateSizedAllocationCount == 1 &&
                accepted.probe.integrationAdvanceCount == 1 &&
                !accepted.destinationExists,
            "explicitly extended catalog did not unlock the same guarded "
            "runtime pipeline (compile=" +
                std::to_string(accepted.compilationSucceeded) +
                ", runtime=" + std::to_string(accepted.runtimeSucceeded) +
                ", schedules=" +
                std::to_string(
                    accepted.counters.scheduleConstructionCount) +
                ", observers=" +
                std::to_string(accepted.counters.observerConstructionCount) +
                ", destinations=" +
                std::to_string(
                    accepted.probe.destinationConstructionCount) +
                ", states=" +
                std::to_string(accepted.probe.stateSizedAllocationCount) +
                ", advances=" +
                std::to_string(accepted.probe.integrationAdvanceCount) +
                ", file=" + std::to_string(accepted.destinationExists) +
                ", status=" + accepted.runtimeMessage + ")");
}

WVModelOutputConfiguration coincidentConfiguration(
    const WVTransformConstantStratificationConfiguration& configuration,
    const std::shared_ptr<const WVExtensionCatalog>& extended) {
    const auto shape = coefficientShape(configuration);
    const auto mission = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow window(
        configuration.Lx, configuration.Ly, configuration.latitude, 3.3);
    const auto schedule = scheduleRecord(mission, window, 1.0, 1450.0,
                                         1570.0);
    const auto observer = observerRecord("along-track", mission, window, 1.0,
                                         {"ssh"});
    const std::vector<std::string> observers{"along-track"};
    const auto record = portableRecord(
        shape, {observer},
        {outputFile("primary", memoryDestination("retry-primary"),
                    outputGroup("along-track", schedule, observers)),
         outputFile("secondary", memoryDestination("retry-secondary"),
                    outputGroup("along-track", schedule, observers))});
    WVModelOutputConfiguration output;
    requireStatus(WVModelOutputConfiguration::compile(
                      record, {}, {}, WVModelOutputPolicy::create, extended,
                      1450.0, 1570.0, output, &configuration, false),
                  "coincident AlongTrack model-output compilation");
    return output;
}

void testOutputDriverFailureRetry() {
    atwv::resetCounters();
    const auto configuration = modelConfiguration();
    const auto extended = catalog(true);
    auto output = coincidentConfiguration(configuration, extended);
    require(output.plan().eventCount() == 1 &&
                output.plan().event(0).routeCount == 2,
            "coincident configuration did not resolve one two-route event");

    std::unique_ptr<WVObserverOutputEvaluationService> source;
    requireStatus(WVObserverOutputEvaluationService::create(
                      configuration, false, output.descriptor(),
                      std::make_unique<WVReferenceFFTEngine>(), source),
                  "routing observation source construction");
    ZeroIntegrationSystem system(output.plan().stateLayout());
    PipelineProbe probe;
    MutableState state(system.stateLayout(), 1450.0, &probe);
    WVFixedStepRK4 integrator(system, WVFixedStepRK4Options{true});
    requireStatus(integrator.prepareStateAfterRestart(state.view),
                  "routing integrator restart preparation");
    MemoryRetrySink sink(*source, probe);
    WVOutputDriver driver(integrator, output.plan());

    ++probe.integrationAdvanceCount;
    auto status = driver.advanceToTime(state.view, 1570.0, 60.0, sink);
    require(!status, "injected later memory-route failure unexpectedly succeeded");
    require(driver.hasPendingDelivery(),
            "later memory-route failure did not retain a pending event: " +
                status.message);
    require(sink.attempts() == std::array<std::size_t, 2>{1, 1},
            "later memory-route failure attempted the wrong destinations");
    const auto rightHandSidesAtFailure =
        system.rightHandSideEvaluationCount();
    const auto acceptedStepsAtFailure =
        integrator.metrics().acceptedStepCount;
    const auto countersAtFailure = atwv::counters();
    const auto metricsAtFailure = source->metrics();

    sink.disableFailure();
    ++probe.integrationAdvanceCount;
    status = driver.advanceToTime(state.view, 1570.0, 60.0, sink);
    requireStatus(status, "AlongTrack memory-route retry");
    require(!driver.hasPendingDelivery(),
            "successful memory-route retry remained pending");
    require(state.view.waveVortex.t == 1570.0,
            "successful memory-route retry did not commit the accepted state");
    require(sink.attempts() == std::array<std::size_t, 2>{1, 2} &&
                sink.successfulRouteCount() == 2,
            "retry repeated a committed route or skipped the failed route");
    require(sink.identityCount() == 3 &&
                sink.semanticIdentityPreserved() &&
                sink.preparedIdentityPreserved(),
            "retry changed the retained semantic occurrence identity");
    require(sink.batchBuildCount() == 1,
            "coincident routes or retry rebuilt the memory batch");
    require(system.rightHandSideEvaluationCount() >
                rightHandSidesAtFailure &&
                integrator.metrics().acceptedStepCount ==
                    acceptedStepsAtFailure + 1,
            "retry repeated the retained step instead of advancing only the "
            "remaining interval");
    const auto countersAfterRetry = atwv::counters();
    const auto metricsAfterRetry = source->metrics();
    require(countersAtFailure.occurrencePreparationCount == 1 &&
                countersAtFailure.batchBuildCount == 1 &&
                countersAfterRetry.occurrencePreparationCount ==
                    countersAtFailure.occurrencePreparationCount &&
                countersAfterRetry.batchBuildCount ==
                    countersAtFailure.batchBuildCount &&
                metricsAfterRetry.occurrencePreparationCount ==
                    metricsAtFailure.occurrencePreparationCount &&
                metricsAfterRetry.occurrenceBatchBuildCount ==
                    metricsAtFailure.occurrenceBatchBuildCount,
            "retry regenerated AlongTrack geometry, fields, or batch");
    require(driver.metrics().generatedSemanticOccurrenceCount == 1 &&
                driver.metrics().deliveryAttemptCount == 3 &&
                driver.metrics().committedDeliveryCount == 2 &&
                driver.metrics().failureCount == 1 &&
                probe.destinationConstructionCount == 1 &&
                probe.stateSizedAllocationCount == 1 &&
                probe.integrationAdvanceCount == 2,
            "driver commit or pipeline lifecycle metrics are incorrect");
}

WVOutputScheduleOccurrence firstOccurrence(
    const std::shared_ptr<const WVExtensionCatalog>& extended,
    const WVOutputScheduleRecord& record,
    std::shared_ptr<const WVOutputSchedule>& schedule) {
    auto status = extended->outputSchedules().resolve(record, schedule);
    requireStatus(status, "routing schedule resolution");
    WVOutputScheduleOccurrence occurrence;
    bool available = false;
    status = schedule->peek({}, record.initialTime, record.finalTime,
                            occurrence, available);
    requireStatus(status, "routing occurrence discovery");
    require(available, "routing schedule emitted no occurrence");
    return occurrence;
}

WVOutputRouteView occurrenceRoute(
    std::size_t fileOrdinal,
    std::size_t semanticScheduleOrdinal,
    const WVOutputObserverView* observers,
    std::size_t observerCount,
    const WVOutputGroupRecord& group,
    const WVOutputSchedulePayloadSchema& schema,
    const WVOutputScheduleOccurrence& occurrence) {
    WVOutputRouteView route;
    route.fileOrdinal = fileOrdinal;
    route.groupOrdinal = 0;
    route.scheduleOrdinal = occurrence.ordinal;
    route.observers = observers;
    route.observerCount = observerCount;
    route.proposedScheduleCursor = &occurrence.proposedCursor.values;
    route.semanticScheduleOrdinal = semanticScheduleOrdinal;
    route.schedulePayloadSchema = &schema;
    route.schedulePayload = &occurrence.payload;
    route.scheduleCursorIdentity = occurrence.cursorIdentity;
    route.semanticScheduleRecord = &group;
    return route;
}

void testCompleteOccurrenceReuseKeys() {
    atwv::resetCounters();
    const auto configuration = modelConfiguration();
    const auto shape = coefficientShape(configuration);
    const auto mission = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow window(
        configuration.Lx, configuration.Ly, configuration.latitude, 3.3);
    const auto scheduleRecordValue = scheduleRecord(
        mission, window, 1.0, 1450.0, 1570.0);
    const auto sshObserver = observerRecord("along-ssh", mission, window, 1.0,
                                            {"ssh"});
    const auto ssuObserver = observerRecord("along-ssu", mission, window, 1.0,
                                            {"ssu"});
    const std::vector<std::string> memberships{"along-ssh", "along-ssu"};
    const auto canonical = portableRecord(
        shape, {sshObserver, ssuObserver},
        {outputFile("identity", memoryDestination("identity"),
                    outputGroup("same-logical-pass", scheduleRecordValue,
                                memberships))});
    const auto extended = catalog(true);
    WVModelOutputConfiguration output;
    requireStatus(WVModelOutputConfiguration::compile(
                      canonical, {}, {}, WVModelOutputPolicy::create, extended,
                      1450.0, 1570.0, output, &configuration, false),
                  "reuse-key model-output compilation");

    std::unique_ptr<WVObserverOutputEvaluationService> source;
    requireStatus(WVObserverOutputEvaluationService::create(
                      configuration, false, output.descriptor(),
                      std::make_unique<WVReferenceFFTEngine>(), source),
                  "reuse-key observation source construction");
    requireStatus(source->preflight(output.plan()),
                  "reuse-key observation source preflight");

    std::shared_ptr<const WVOutputSchedule> schedule;
    const auto occurrence = firstOccurrence(
        extended, scheduleRecordValue, schedule);
    const auto& records = output.descriptor().observers();
    require(records.size() == 3,
            "reuse-key descriptor has the wrong observer count");
    const auto* resolvedSsh = output.descriptor().resolvedObserver(records[1]);
    const auto* resolvedSsu = output.descriptor().resolvedObserver(records[2]);
    require(resolvedSsh != nullptr && resolvedSsu != nullptr,
            "reuse-key AlongTrack observers are unresolved");
    const std::array<WVOutputObserverView, 2> observers{{
        {1, &records[1], resolvedSsh},
        {2, &records[2], resolvedSsu},
    }};

    const auto sameFirst = outputGroup(
        "same-logical-pass", scheduleRecordValue, memberships);
    const auto sameSecond = outputGroup(
        "same-logical-pass", scheduleRecordValue, memberships);
    const auto otherLogical = outputGroup(
        "other-logical-pass", scheduleRecordValue, memberships);
    auto changedSchedule = scheduleRecordValue;
    const auto centerLongitude = std::find_if(
        changedSchedule.configuration.values.begin(),
        changedSchedule.configuration.values.end(), [](const auto& value) {
            return value.name == atwv::centerLongitudeDegreesField;
        });
    require(centerLongitude != changedSchedule.configuration.values.end(),
            "reuse-key schedule has no center longitude");
    std::get<std::vector<double>>(centerLongitude->storage)[0] =
        std::nextafter(
            std::get<std::vector<double>>(centerLongitude->storage)[0], 4.0);
    const auto otherConfiguration = outputGroup(
        "same-logical-pass", changedSchedule, memberships);
    require(!sameLogicalOutputScheduleIdentity(sameFirst, otherLogical) &&
                !sameLogicalOutputScheduleIdentity(sameFirst,
                                                   otherConfiguration),
            "reuse-key fixtures did not encode distinct logical schedules");

    std::array<WVOutputRouteView, 4> routes{{
        occurrenceRoute(0, 10, observers.data(), observers.size(), sameFirst,
                        schedule->payloadSchema(), occurrence),
        occurrenceRoute(1, 10, observers.data(), observers.size(), sameSecond,
                        schedule->payloadSchema(), occurrence),
        occurrenceRoute(2, 11, observers.data(), observers.size(), otherLogical,
                        schedule->payloadSchema(), occurrence),
        occurrenceRoute(3, 12, observers.data(), observers.size(),
                        otherConfiguration, schedule->payloadSchema(),
                        occurrence),
    }};
    ZeroIntegrationSystem system(output.plan().stateLayout());
    MutableState state(system.stateLayout(), occurrence.scheduledTime);
    std::vector<WVAdditionalStateBlockConstView> blockViews;
    WVOutputEvent event;
    event.eventOrdinal = 30;
    event.scheduledTime = occurrence.scheduledTime;
    event.kind = WVOutputEventKind::acceptedEndpoint;
    event.state = integrationConstView(state.view, blockViews);
    event.routes = routes.data();
    event.routeCount = routes.size();

    const auto metricsBefore = source->metrics();
    const auto countersBefore = atwv::counters();
    requireStatus(source->prepare(event),
                  "complete reuse-key occurrence preparation");
    std::array<WVObservationOccurrenceIdentity, 4> sshIdentities;
    std::array<WVObservationOccurrenceIdentity, 4> ssuIdentities;
    for (std::size_t route = 0; route < routes.size(); ++route) {
        requireStatus(source->preparedOccurrenceIdentity(
                          routes[route], observers[0], sshIdentities[route]),
                      "ssh reuse-key identity");
        requireStatus(source->preparedOccurrenceIdentity(
                          routes[route], observers[1], ssuIdentities[route]),
                      "ssu reuse-key identity");
    }
    require(sameObservationOccurrenceIdentity(sshIdentities[0],
                                              sshIdentities[1]) &&
                samePreparedObservationOccurrenceIdentity(
                    sshIdentities[0], sshIdentities[1]) &&
                sameObservationOccurrenceIdentity(ssuIdentities[0],
                                                  ssuIdentities[1]) &&
                samePreparedObservationOccurrenceIdentity(
                    ssuIdentities[0], ssuIdentities[1]),
            "compatible logical schedule routes did not reuse occurrences");
    require(!sameObservationOccurrenceIdentity(sshIdentities[0],
                                               sshIdentities[2]) &&
                !samePreparedObservationOccurrenceIdentity(
                    sshIdentities[0], sshIdentities[2]) &&
                !sameObservationOccurrenceIdentity(sshIdentities[0],
                                                   sshIdentities[3]) &&
                !samePreparedObservationOccurrenceIdentity(
                    sshIdentities[0], sshIdentities[3]),
            "distinct logical schedule instance or configuration reused an "
            "occurrence");
    require(!sameObservationOccurrenceIdentity(sshIdentities[0],
                                               ssuIdentities[0]) &&
                !samePreparedObservationOccurrenceIdentity(
                    sshIdentities[0], ssuIdentities[0]) &&
                sshIdentities[0].fieldPlanFingerprint !=
                    ssuIdentities[0].fieldPlanFingerprint,
            "different resolved observer field plans reused an occurrence");
    const auto metricsAfterPrepare = source->metrics();
    const auto countersAfterPrepare = atwv::counters();
    require(metricsAfterPrepare.occurrencePreparationCount -
                    metricsBefore.occurrencePreparationCount ==
                6 &&
                metricsAfterPrepare.occurrenceReuseCount -
                        metricsBefore.occurrenceReuseCount ==
                    2 &&
                countersAfterPrepare.occurrencePreparationCount -
                        countersBefore.occurrencePreparationCount ==
                    6,
            "reuse-key preparation did not use the complete "
            "occurrence/geometry/field-plan key");

    struct CachedBatch final {
        WVObservationOccurrenceIdentity identity;
        WVObservationBatch batch;
    };
    std::vector<CachedBatch> cached;
    std::size_t cacheReuseCount = 0;
    const auto cacheBatch = [&](const WVObservationOccurrenceIdentity& identity,
                                const WVObserverRecord& record,
                                const char* context) {
        const auto found = std::find_if(
            cached.begin(), cached.end(), [&](const auto& candidate) {
                return samePreparedObservationOccurrenceIdentity(
                    candidate.identity, identity);
            });
        if (found != cached.end()) {
            ++cacheReuseCount;
            return;
        }
        CachedBatch candidate;
        candidate.identity = identity;
        requireStatus(source->observationBatch(identity, record,
                                               candidate.batch),
                      context);
        cached.push_back(std::move(candidate));
    };
    for (std::size_t route = 0; route < routes.size(); ++route) {
        cacheBatch(sshIdentities[route], records[1], "ssh reuse-key batch");
        cacheBatch(ssuIdentities[route], records[2], "ssu reuse-key batch");
    }
    const auto metricsAfterBatches = source->metrics();
    const auto countersAfterBatches = atwv::counters();
    const auto serviceBatchBuilds =
        metricsAfterBatches.occurrenceBatchBuildCount -
        metricsBefore.occurrenceBatchBuildCount;
    const auto providerBatchBuilds =
        countersAfterBatches.batchBuildCount - countersBefore.batchBuildCount;
    require(cached.size() == 6 && cacheReuseCount == 2 &&
                serviceBatchBuilds == 6 && providerBatchBuilds == 6,
            "compatible routes rebuilt batches or incompatible keys shared "
            "them (service " + std::to_string(serviceBatchBuilds) +
                ", provider " + std::to_string(providerBatchBuilds) + ")");
    source->complete(event);
}

} // namespace

int main() {
    try {
        testModelOutputCatalogPreflight();
        testCatalogPreflightGuardsRuntimeAssembly();
        testOutputDriverFailureRetry();
        testCompleteOccurrenceReuseKeys();
        std::cout << "PASS: AlongTrack WaveVortex routing and preflight\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
