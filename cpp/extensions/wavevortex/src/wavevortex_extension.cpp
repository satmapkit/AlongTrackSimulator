#include "alongtrack/wavevortex_extension.hpp"

#include "alongtrack/portable_core.hpp"
#include "WaveVortexRuntime/WVObserverOutputProvider.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace alongtrack::wavevortex_extension {
namespace {

namespace runtime = ::wavevortex::runtime;
using ::wavevortex::WVKernelStatus;
using ::wavevortex::WVKernelStatusCode;

constexpr std::size_t maximumConstructionRecordBytes = 64U * 1024U;
constexpr std::uint64_t fnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;

std::atomic<std::size_t> scheduleConstructionCount{0};
std::atomic<std::size_t> schedulePeekCount{0};
std::atomic<std::size_t> observerConfigurationResolutionCount{0};
std::atomic<std::size_t> observerConstructionCount{0};
std::atomic<std::size_t> outputPlanResolutionCount{0};
std::atomic<std::size_t> occurrencePreparationCount{0};
std::atomic<std::size_t> batchBuildCount{0};

[[nodiscard]] WVKernelStatus invalid(std::string message) {
    return {WVKernelStatusCode::invalidConfiguration, std::move(message)};
}

[[nodiscard]] WVKernelStatus allocationFailure(std::string message) {
    return {WVKernelStatusCode::allocationFailure, std::move(message)};
}

[[nodiscard]] WVKernelStatus sizeOverflow(std::string message) {
    return {WVKernelStatusCode::sizeOverflow, std::move(message)};
}

struct SourceConfiguration final {
    std::string missionKey;
    double semiMajorAxisKm = 0.0;
    double resolvedSemiMajorAxisKm = 0.0;
    double eccentricity = 0.0;
    double inclinationDegrees = 0.0;
    double ascendingNodeLongitudeDegrees = 0.0;
    double passesPerCycle = 0.0;
    double missionEpochSeconds = 0.0;
    double windowWidthMeters = 0.0;
    double windowHeightMeters = 0.0;
    double centerLatitudeDegrees = 0.0;
    double centerLongitudeDegrees = 0.0;
    bool lowerLeftOrigin = true;
    double sampleIntervalSeconds = 0.0;
    std::int64_t constructionIdentity = 0;
};

struct ObserverConfiguration final {
    SourceConfiguration source;
    std::vector<std::string> fieldNames;
    runtime::WVPositionInterpolation interpolation =
        runtime::WVPositionInterpolation::linear;
};

struct ResolvedField final {
    runtime::WVPortableVariable identifier = runtime::WVPortableVariable::invalid;
    std::string name;
    std::string units;
    std::string description;
    std::uint64_t dependencyMask = 0;
};

[[nodiscard]] const runtime::WVPortableNamedValue* scalar(
    const runtime::WVPortableTypedRecord& record,
    const char* name,
    runtime::WVPortableValueType type) noexcept {
    const auto* value = record.value(name);
    return value != nullptr && value->valueType() == type &&
                   value->dimensions.empty() && value->valueCount() == 1
               ? value
               : nullptr;
}

[[nodiscard]] bool readRealScalar(const runtime::WVPortableTypedRecord& record,
                                  const char* name,
                                  double& output) noexcept {
    const auto* value = scalar(record, name, runtime::WVPortableValueType::real);
    if (value == nullptr) {
        return false;
    }
    output = std::get<std::vector<double>>(value->storage).front();
    return true;
}

[[nodiscard]] bool readIntegerScalar(
    const runtime::WVPortableTypedRecord& record,
    const char* name,
    std::int64_t& output) noexcept {
    const auto* value =
        scalar(record, name, runtime::WVPortableValueType::integer);
    if (value == nullptr) {
        return false;
    }
    output = std::get<std::vector<std::int64_t>>(value->storage).front();
    return true;
}

[[nodiscard]] bool readBooleanScalar(
    const runtime::WVPortableTypedRecord& record,
    const char* name,
    bool& output) noexcept {
    const auto* value =
        scalar(record, name, runtime::WVPortableValueType::boolean);
    if (value == nullptr) {
        return false;
    }
    const auto stored =
        std::get<std::vector<std::uint8_t>>(value->storage).front();
    if (stored > 1U) {
        return false;
    }
    output = stored != 0U;
    return true;
}

[[nodiscard]] bool readTextScalar(const runtime::WVPortableTypedRecord& record,
                                  const char* name,
                                  std::string& output) {
    const auto* value = scalar(record, name, runtime::WVPortableValueType::text);
    if (value == nullptr) {
        return false;
    }
    output = std::get<std::vector<std::string>>(value->storage).front();
    return true;
}

[[nodiscard]] bool readTextVector(const runtime::WVPortableTypedRecord& record,
                                  const char* name,
                                  std::vector<std::string>& output) {
    const auto* value = record.value(name);
    if (value == nullptr || value->valueType() != runtime::WVPortableValueType::text ||
        value->dimensions.size() != 1U ||
        value->dimensions.front() != value->valueCount()) {
        return false;
    }
    output = std::get<std::vector<std::string>>(value->storage);
    return true;
}

void hashByte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= fnvPrime;
}

void hashUnsigned(std::uint64_t& hash, std::uint64_t value) noexcept {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        hashByte(hash, static_cast<std::uint8_t>(value & 0xffU));
        value >>= 8U;
    }
}

void hashDouble(std::uint64_t& hash, double value) noexcept {
    static_assert(sizeof(double) == sizeof(std::uint64_t),
                  "AlongTrack configuration identities require binary64 doubles");
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    hashUnsigned(hash, bits);
}

void hashString(std::uint64_t& hash, const std::string& value) noexcept {
    hashUnsigned(hash, static_cast<std::uint64_t>(value.size()));
    for (const char character : value) {
        hashByte(hash, static_cast<std::uint8_t>(character));
    }
}

[[nodiscard]] std::int64_t signedIdentity(std::uint64_t identity) noexcept {
    std::int64_t result = 0;
    std::memcpy(&result, &identity, sizeof(result));
    return result;
}

[[nodiscard]] std::int64_t sourceConfigurationIdentity(
    const SourceConfiguration& configuration) noexcept {
    std::uint64_t hash = fnvOffset;
    hashString(hash, configuration.missionKey);
    hashDouble(hash, configuration.semiMajorAxisKm);
    hashDouble(hash, configuration.resolvedSemiMajorAxisKm);
    hashDouble(hash, configuration.eccentricity);
    hashDouble(hash, configuration.inclinationDegrees);
    hashDouble(hash, configuration.ascendingNodeLongitudeDegrees);
    hashDouble(hash, configuration.passesPerCycle);
    hashDouble(hash, configuration.missionEpochSeconds);
    hashDouble(hash, configuration.windowWidthMeters);
    hashDouble(hash, configuration.windowHeightMeters);
    hashDouble(hash, configuration.centerLatitudeDegrees);
    hashDouble(hash, configuration.centerLongitudeDegrees);
    hashByte(hash, static_cast<std::uint8_t>(configuration.lowerLeftOrigin));
    hashDouble(hash, configuration.sampleIntervalSeconds);
    return signedIdentity(hash);
}

[[nodiscard]] WVKernelStatus parseSourceConfiguration(
    const runtime::WVPortableTypedRecord& record,
    const char* expectedSchema,
    std::size_t expectedValueCount,
    SourceConfiguration& output) {
    const auto validation = runtime::validatePortableTypedRecord(
        record, {maximumConstructionRecordBytes, true, true});
    if (!validation || record.schemaIdentifier != expectedSchema ||
        record.schemaVersion != contractVersion ||
        record.values.size() != expectedValueCount) {
        return invalid("Invalid AlongTrack construction record.");
    }

    SourceConfiguration candidate;
    if (!readTextScalar(record, missionKeyField, candidate.missionKey) ||
        !readRealScalar(record, semiMajorAxisKmField,
                        candidate.semiMajorAxisKm) ||
        !readRealScalar(record, resolvedSemiMajorAxisKmField,
                        candidate.resolvedSemiMajorAxisKm) ||
        !readRealScalar(record, eccentricityField, candidate.eccentricity) ||
        !readRealScalar(record, inclinationDegreesField,
                        candidate.inclinationDegrees) ||
        !readRealScalar(record, ascendingNodeLongitudeDegreesField,
                        candidate.ascendingNodeLongitudeDegrees) ||
        !readRealScalar(record, passesPerCycleField,
                        candidate.passesPerCycle) ||
        !readRealScalar(record, missionEpochSecondsField,
                        candidate.missionEpochSeconds) ||
        !readRealScalar(record, windowWidthMetersField,
                        candidate.windowWidthMeters) ||
        !readRealScalar(record, windowHeightMetersField,
                        candidate.windowHeightMeters) ||
        !readRealScalar(record, centerLatitudeDegreesField,
                        candidate.centerLatitudeDegrees) ||
        !readRealScalar(record, centerLongitudeDegreesField,
                        candidate.centerLongitudeDegrees) ||
        !readBooleanScalar(record, lowerLeftOriginField,
                           candidate.lowerLeftOrigin) ||
        !readRealScalar(record, sampleIntervalSecondsField,
                        candidate.sampleIntervalSeconds)) {
        return invalid("AlongTrack construction fields have the wrong type or shape.");
    }

    const bool repeatCountIsValid =
        candidate.passesPerCycle == 0.0 ||
        (candidate.passesPerCycle > 0.0 &&
         std::trunc(candidate.passesPerCycle) == candidate.passesPerCycle);
    if (candidate.missionKey.empty() || candidate.semiMajorAxisKm <= 0.0 ||
        candidate.resolvedSemiMajorAxisKm <= 0.0 ||
        candidate.eccentricity < 0.0 || candidate.eccentricity >= 1.0 ||
        !repeatCountIsValid || candidate.windowWidthMeters <= 0.0 ||
        candidate.windowHeightMeters <= 0.0 ||
        candidate.centerLatitudeDegrees < -90.0 ||
        candidate.centerLatitudeDegrees > 90.0 ||
        candidate.sampleIntervalSeconds <= 0.0) {
        return invalid("AlongTrack construction values are outside their valid ranges.");
    }
    candidate.constructionIdentity = sourceConfigurationIdentity(candidate);
    output = std::move(candidate);
    return WVKernelStatus::ok();
}

[[nodiscard]] WVKernelStatus parseObserverConfiguration(
    const runtime::WVPortableTypedRecord& record,
    ObserverConfiguration& output) {
    ObserverConfiguration candidate;
    auto status = parseSourceConfiguration(
        record, observerConfigurationSchemaIdentifier, 16U, candidate.source);
    if (!status) {
        return status;
    }
    std::int64_t interpolation = 0;
    if (!readTextVector(record, fieldNamesField, candidate.fieldNames) ||
        !readIntegerScalar(record, interpolationField, interpolation) ||
        candidate.fieldNames.empty() || interpolation < 0 || interpolation > 1) {
        return invalid("AlongTrack observer fields or interpolation are invalid.");
    }
    std::set<std::string> uniqueFields;
    for (const auto& field : candidate.fieldNames) {
        if (field.empty() || !uniqueFields.insert(field).second) {
            return invalid("AlongTrack observer field names must be nonempty and unique.");
        }
    }
    candidate.interpolation =
        interpolation == 0 ? runtime::WVPositionInterpolation::linear
                           : runtime::WVPositionInterpolation::spline;
    output = std::move(candidate);
    return WVKernelStatus::ok();
}

[[nodiscard]] MissionDefinition missionDefinition(
    const SourceConfiguration& configuration) {
    const double passes = configuration.passesPerCycle == 0.0
                              ? std::numeric_limits<double>::infinity()
                              : configuration.passesPerCycle;
    return {configuration.missionKey,
            configuration.semiMajorAxisKm,
            configuration.resolvedSemiMajorAxisKm,
            configuration.eccentricity,
            configuration.inclinationDegrees,
            configuration.ascendingNodeLongitudeDegrees,
            passes};
}

[[nodiscard]] ProjectionWindow projectionWindow(
    const SourceConfiguration& configuration) {
    return {configuration.windowWidthMeters,
            configuration.windowHeightMeters,
            configuration.centerLatitudeDegrees,
            configuration.centerLongitudeDegrees,
            configuration.lowerLeftOrigin ? WindowOrigin::lowerLeft
                                          : WindowOrigin::centered};
}

[[nodiscard]] MissionPassSource passSource(
    const SourceConfiguration& configuration) {
    return {ResolvedMission(missionDefinition(configuration)),
            projectionWindow(configuration),
            configuration.sampleIntervalSeconds};
}

// These three helpers are the complete extension-to-scientific-core boundary.
// They translate model time to immutable mission time without duplicating pass
// discovery, geometry, projection, or orbital algorithms in the adapter.
[[nodiscard]] std::optional<MissionPassDescriptor> nextMissionPass(
    const MissionPassSource& source,
    const std::optional<MissionPassDescriptor>& committed,
    double lowerBound,
    double upperBound,
    double missionEpochSeconds) {
    const double missionLower = lowerBound - missionEpochSeconds;
    const double missionUpper = upperBound - missionEpochSeconds;
    if (!std::isfinite(missionLower) || !std::isfinite(missionUpper)) {
        throw std::overflow_error("AlongTrack mission-time bounds are not finite");
    }
    return source.nextPass(committed, missionLower, missionUpper);
}

[[nodiscard]] double missionPassTriggerTime(
    const MissionPassSource& source,
    const MissionPassDescriptor& descriptor,
    double missionEpochSeconds) {
    const double time = source.passTriggerTime(descriptor) + missionEpochSeconds;
    if (!std::isfinite(time)) {
        throw std::overflow_error("AlongTrack trigger time is not finite");
    }
    return time;
}

[[nodiscard]] Pass reconstructMissionPass(
    const MissionPassSource& source,
    const MissionPassDescriptor& descriptor,
    double missionEpochSeconds) {
    auto pass = source.reconstructPass(descriptor);
    for (auto& point : pass) {
        point.timeSeconds += missionEpochSeconds;
        if (!std::isfinite(point.timeSeconds)) {
            throw std::overflow_error("AlongTrack sample time is not finite");
        }
    }
    return pass;
}

[[nodiscard]] WVKernelStatus resolveFields(
    const ObserverConfiguration& configuration,
    std::vector<ResolvedField>& output,
    std::uint64_t& requestedFieldMask,
    std::uint64_t& dependencyMask) {
    std::vector<ResolvedField> fields;
    fields.reserve(configuration.fieldNames.size());
    requestedFieldMask = 0;
    dependencyMask = 0;
    for (const auto& name : configuration.fieldNames) {
        const auto* metadata = runtime::findPortableVariable(name);
        if (metadata == nullptr || metadata->isComplex ||
            metadata->naturalRank != runtime::WVPortableNaturalRank::horizontal ||
            (metadata->samplingMask & runtime::portablePositionSampling) == 0U) {
            return invalid("AlongTrack fields must be real, horizontal, and position-sampleable.");
        }
        const auto ordinal = static_cast<std::size_t>(metadata->identifier);
        if (ordinal >= 64U) {
            return sizeOverflow("AlongTrack requested-field identity exceeds its resolved mask.");
        }
        requestedFieldMask |= std::uint64_t{1} << ordinal;
        dependencyMask |= metadata->primitiveDependencyMask;
        fields.push_back({metadata->identifier, metadata->name, metadata->units,
                          metadata->description,
                          metadata->primitiveDependencyMask});
    }
    output = std::move(fields);
    return WVKernelStatus::ok();
}

[[nodiscard]] WVKernelStatus validateObserverRecord(
    const runtime::WVObserverRecord& record,
    const runtime::WVPortableTypedRecord& configuration,
    const ObserverConfiguration& resolved) {
    if (record.typeIdentifier != observingSystemTypeIdentifier ||
        record.contractVersion != contractVersion || record.identifier.empty() ||
        record.name.empty() ||
        !runtime::samePortableTypedRecordValue(record.configuration,
                                               configuration) ||
        record.fieldNames != resolved.fieldNames ||
        record.trackedFieldInterpolation != resolved.interpolation ||
        !record.stateBlockIdentifiers.empty() || !record.x.empty() ||
        !record.y.empty() || !record.z.empty() || record.outputScale != 1.0 ||
        record.outputOffset != 0.0) {
        return invalid("AlongTrack observer record conflicts with its resolved configuration.");
    }
    return WVKernelStatus::ok();
}

[[nodiscard]] runtime::WVObservationVariable observationVariable(
    std::string identifier,
    runtime::WVObservationScalarType scalarType,
    std::vector<std::string> dimensions,
    runtime::WVObservationValueLayout layout,
    runtime::WVObservationCoordinateRole coordinateRole,
    std::string units,
    std::string description) {
    runtime::WVObservationVariable variable;
    variable.identifier = identifier;
    variable.name = std::move(identifier);
    variable.scalarType = scalarType;
    variable.dimensionIdentifiers = std::move(dimensions);
    variable.layout = layout;
    variable.units = std::move(units);
    variable.description = std::move(description);
    variable.coordinateRole = coordinateRole;
    return variable;
}

void addOccurrenceValue(runtime::WVObserverOutputPlan& plan,
                        runtime::WVObservationVariable variable) {
    const auto identifier = variable.identifier;
    plan.schema.variables.push_back(std::move(variable));
    plan.occurrenceValues.push_back({identifier, 0U});
    runtime::WVObserverOutputChannel channel;
    channel.variableIdentifier = identifier;
    channel.source = runtime::WVObserverOutputChannelSource::occurrenceValue;
    channel.sourceIdentifier = identifier;
    plan.channels.push_back(std::move(channel));
}

void addOccurrenceField(runtime::WVObserverOutputPlan& plan,
                        const ResolvedField& field,
                        runtime::WVPositionInterpolation interpolation) {
    plan.schema.variables.push_back(observationVariable(
        field.name, runtime::WVObservationScalarType::real64, {"sample"},
        runtime::WVObservationValueLayout::flat,
        runtime::WVObservationCoordinateRole::none, field.units,
        field.description));
    runtime::WVObserverOutputChannel channel;
    channel.variableIdentifier = field.name;
    channel.source = runtime::WVObserverOutputChannelSource::occurrenceField;
    channel.sourceIdentifier = field.name;
    channel.positionSetSlot = 0U;
    channel.sampling.kind = runtime::WVFieldSamplingKind::positions;
    channel.sampling.interpolation = interpolation;
    plan.channels.push_back(std::move(channel));
}

[[nodiscard]] WVKernelStatus validatePlanningDomain(
    const SourceConfiguration& source,
    const runtime::WVObserverOutputPlanningContext& context) {
    if (context.configuration == nullptr) {
        return invalid("AlongTrack output planning requires a model configuration.");
    }
    if (context.configuration->Lx != source.windowWidthMeters ||
        context.configuration->Ly != source.windowHeightMeters) {
        return invalid("AlongTrack projection dimensions must exactly match model Lx and Ly.");
    }
    return WVKernelStatus::ok();
}

[[nodiscard]] WVKernelStatus buildOutputPlan(
    const runtime::WVObserverRecord& record,
    const ObserverConfiguration& configuration,
    const std::vector<ResolvedField>& fields,
    runtime::WVObserverOutputPlan& output) {
    runtime::WVObserverOutputPlan plan;
    plan.schema.identifier = record.identifier + "-along-track-observation-v1";
    plan.schema.version = 1;
    plan.schema.metadata.attributes = {
        {"provider", observingSystemTypeIdentifier},
        {"mission", configuration.source.missionKey},
        {"contract", "generic-along-track-occurrence-v1"}};

    runtime::WVObservationAxis sampleAxis;
    sampleAxis.identifier = "sample";
    sampleAxis.name = "sample";
    sampleAxis.kind = runtime::WVObservationAxisKind::unlimited;
    sampleAxis.extent = 0U;
    sampleAxis.coordinateRole = runtime::WVObservationCoordinateRole::identifier;
    plan.schema.axes.push_back(std::move(sampleAxis));

    addOccurrenceValue(
        plan, observationVariable(
                  "sample-time", runtime::WVObservationScalarType::real64,
                  {"sample"}, runtime::WVObservationValueLayout::flat,
                  runtime::WVObservationCoordinateRole::sampleTime, "s",
                  "mission sample time; the model state is evaluated at the pass trigger"));
    addOccurrenceValue(
        plan, observationVariable(
                  "x", runtime::WVObservationScalarType::real64, {"sample"},
                  runtime::WVObservationValueLayout::flat,
                  runtime::WVObservationCoordinateRole::x, "m",
                  "projected along-track x coordinate"));
    addOccurrenceValue(
        plan, observationVariable(
                  "y", runtime::WVObservationScalarType::real64, {"sample"},
                  runtime::WVObservationValueLayout::flat,
                  runtime::WVObservationCoordinateRole::y, "m",
                  "projected along-track y coordinate"));
    addOccurrenceValue(
        plan, observationVariable(
                  "repeat-cycle-index",
                  runtime::WVObservationScalarType::integer64, {},
                  runtime::WVObservationValueLayout::record,
                  runtime::WVObservationCoordinateRole::pass, {},
                  "repeat-cycle component of the exact pass identity"));
    addOccurrenceValue(
        plan, observationVariable(
                  "first-sample-index",
                  runtime::WVObservationScalarType::integer64, {},
                  runtime::WVObservationValueLayout::record,
                  runtime::WVObservationCoordinateRole::none, {},
                  "cadence-lattice start of the exact pass identity"));
    addOccurrenceValue(
        plan, observationVariable(
                  "sample-count", runtime::WVObservationScalarType::integer64,
                  {}, runtime::WVObservationValueLayout::record,
                  runtime::WVObservationCoordinateRole::none, {},
                  "sample extent of the exact pass identity"));

    plan.occurrencePositionSets.push_back(
        {"along-track-samples", "sample-time", "x", "y", ""});
    auto status = createSchedulePayloadSchema(plan.occurrencePayloadSchema);
    if (!status) {
        return status;
    }
    for (const auto& field : fields) {
        addOccurrenceField(plan, field, configuration.interpolation);
    }
    status = runtime::validateObservationSchema(plan.schema);
    if (!status) {
        return status;
    }
    output = std::move(plan);
    return WVKernelStatus::ok();
}

[[nodiscard]] WVKernelStatus decodeCursorDescriptor(
    const runtime::WVOutputScheduleCursor& cursor,
    const MissionPassSource& source,
    std::int64_t expectedConstructionIdentity,
    std::optional<MissionPassDescriptor>& descriptor) {
    descriptor.reset();
    if (cursor.committedOrdinal < runtime::WVNoCommittedOutputOrdinal) {
        return invalid("AlongTrack cursor ordinal is less than -1.");
    }
    if (cursor.committedOrdinal == runtime::WVNoCommittedOutputOrdinal) {
        if (!runtime::isCanonicalEmptyPortableTypedRecord(cursor.values)) {
            return invalid("A fresh AlongTrack cursor must not contain provider state.");
        }
        return WVKernelStatus::ok();
    }
    if (cursor.values.schemaIdentifier != scheduleCursorSchemaIdentifier ||
        cursor.values.schemaVersion != contractVersion ||
        cursor.values.values.size() != 1U ||
        cursor.values.encodedBytes() > runtime::WVMaximumOutputScheduleCursorBytes) {
        return invalid("AlongTrack cursor has the wrong typed schema.");
    }
    const auto& value = cursor.values.values.front();
    if (value.name != cursorDescriptorField ||
        value.valueType() != runtime::WVPortableValueType::integer ||
        value.dimensions.size() != 1U || value.dimensions.front() != 4U ||
        value.valueCount() != 4U) {
        return invalid("AlongTrack cursor descriptor has the wrong type or shape.");
    }
    const auto& values = std::get<std::vector<std::int64_t>>(value.storage);
    if (values[constructionConfigurationIdentitySlot] !=
        expectedConstructionIdentity) {
        return invalid("AlongTrack cursor belongs to a different resolved construction.");
    }
    MissionPassDescriptor candidate{values[repeatCycleIndexSlot],
                                    values[firstSampleIndexSlot],
                                    values[sampleCountSlot]};
    if (!source.isStructurallyValid(candidate)) {
        return invalid("AlongTrack cursor contains an invalid pass descriptor.");
    }
    descriptor = candidate;
    return WVKernelStatus::ok();
}

[[nodiscard]] runtime::WVPortableTypedRecord cursorRecord(
    const MissionPassDescriptor& descriptor,
    std::int64_t constructionIdentity) {
    runtime::WVPortableTypedRecord record;
    record.schemaIdentifier = scheduleCursorSchemaIdentifier;
    record.schemaVersion = contractVersion;
    record.values.push_back(
        {cursorDescriptorField,
         {4U},
         std::vector<std::int64_t>{descriptor.repeatCycleIndex,
                                   descriptor.firstSampleIndex,
                                   descriptor.sampleCount,
                                   constructionIdentity}});
    return record;
}

[[nodiscard]] std::uint64_t occurrenceIdentity(
    runtime::WVOutputScheduleOrdinal ordinal,
    const MissionPassDescriptor& descriptor,
    std::int64_t constructionIdentity) noexcept {
    std::uint64_t hash = fnvOffset;
    hashUnsigned(hash, static_cast<std::uint64_t>(ordinal));
    hashUnsigned(hash, static_cast<std::uint64_t>(descriptor.repeatCycleIndex));
    hashUnsigned(hash, static_cast<std::uint64_t>(descriptor.firstSampleIndex));
    hashUnsigned(hash, static_cast<std::uint64_t>(descriptor.sampleCount));
    std::uint64_t constructionBits = 0;
    std::memcpy(&constructionBits, &constructionIdentity,
                sizeof(constructionBits));
    hashUnsigned(hash, constructionBits);
    return hash;
}

class WVAlongTrackSchedule final : public runtime::WVOutputSchedule {
public:
    WVAlongTrackSchedule(runtime::WVPortableTypedRecord configuration,
                         SourceConfiguration resolved,
                         double initialTime,
                         double finalTime,
                         runtime::WVOutputSchedulePayloadSchema payloadSchema)
        : configuration_(std::move(configuration)), source_(passSource(resolved)),
          missionEpochSeconds_(resolved.missionEpochSeconds),
          initialTime_(initialTime), finalTime_(finalTime),
          constructionIdentity_(resolved.constructionIdentity),
          payloadSchema_(std::move(payloadSchema)) {}

    const char* typeIdentifier() const noexcept final {
        return scheduleTypeIdentifier;
    }

    std::uint32_t contractVersion() const noexcept final {
        return wavevortex_extension::contractVersion;
    }

    const runtime::WVOutputSchedulePayloadSchema&
    payloadSchema() const noexcept final {
        return payloadSchema_;
    }

    WVKernelStatus validateCursor(
        const runtime::WVOutputScheduleCursor& cursor) const final {
        try {
            std::optional<MissionPassDescriptor> descriptor;
            auto status = decodeCursorDescriptor(cursor, source_,
                                                 constructionIdentity_, descriptor);
            if (!status || !descriptor.has_value()) {
                return status;
            }
            const double time = missionPassTriggerTime(
                source_, *descriptor, missionEpochSeconds_);
            if (time < initialTime_ || time > finalTime_) {
                return invalid("AlongTrack cursor lies outside its schedule bounds.");
            }
            return WVKernelStatus::ok();
        } catch (const std::overflow_error& error) {
            return sizeOverflow(error.what());
        } catch (const std::exception& error) {
            return invalid(error.what());
        }
    }

    WVKernelStatus committedTime(const runtime::WVOutputScheduleCursor& cursor,
                                 double& time,
                                 bool& available) const final {
        available = false;
        auto status = validateCursor(cursor);
        if (!status) {
            return status;
        }
        if (cursor.committedOrdinal == runtime::WVNoCommittedOutputOrdinal) {
            return WVKernelStatus::ok();
        }
        std::optional<MissionPassDescriptor> descriptor;
        status = decodeCursorDescriptor(cursor, source_, constructionIdentity_,
                                        descriptor);
        if (!status || !descriptor.has_value()) {
            return status;
        }
        try {
            time = missionPassTriggerTime(source_, *descriptor,
                                          missionEpochSeconds_);
            available = true;
            return WVKernelStatus::ok();
        } catch (const std::overflow_error& error) {
            return sizeOverflow(error.what());
        } catch (const std::exception& error) {
            return invalid(error.what());
        }
    }

    WVKernelStatus peek(const runtime::WVOutputScheduleCursor& cursor,
                        double lowerBound,
                        double upperBound,
                        runtime::WVOutputScheduleOccurrence& occurrence,
                        bool& available) const final {
        schedulePeekCount.fetch_add(1U, std::memory_order_relaxed);
        available = false;
        if (!std::isfinite(lowerBound) || !std::isfinite(upperBound) ||
            upperBound < lowerBound) {
            return invalid("AlongTrack schedule bounds must be finite and ordered.");
        }
        std::optional<MissionPassDescriptor> committed;
        auto status = decodeCursorDescriptor(cursor, source_,
                                             constructionIdentity_, committed);
        if (!status) {
            return status;
        }
        if (cursor.committedOrdinal ==
            std::numeric_limits<runtime::WVOutputScheduleOrdinal>::max()) {
            return sizeOverflow("AlongTrack schedule ordinal overflowed int64.");
        }
        const double lower = std::max(lowerBound, initialTime_);
        const double upper = std::min(upperBound, finalTime_);
        if (upper < lower) {
            return WVKernelStatus::ok();
        }
        try {
            const auto next = nextMissionPass(source_, committed, lower, upper,
                                              missionEpochSeconds_);
            if (!next.has_value()) {
                return WVKernelStatus::ok();
            }
            const auto ordinal = cursor.committedOrdinal + 1;
            runtime::WVOutputScheduleOccurrence candidate;
            candidate.scheduledTime = missionPassTriggerTime(
                source_, *next, missionEpochSeconds_);
            candidate.ordinal = ordinal;
            candidate.proposedCursor.committedOrdinal = ordinal;
            candidate.proposedCursor.values =
                cursorRecord(*next, constructionIdentity_);
            status = candidate.payload.reset(payloadSchema_);
            if (!status) {
                return status;
            }
            const std::int64_t repeatCycleIndex = next->repeatCycleIndex;
            const std::int64_t firstSampleIndex = next->firstSampleIndex;
            const std::int64_t sampleCount = next->sampleCount;
            status = candidate.payload.setInteger(
                payloadSchema_, repeatCycleIndexSlot, &repeatCycleIndex, 1U);
            if (status) {
                status = candidate.payload.setInteger(
                    payloadSchema_, firstSampleIndexSlot, &firstSampleIndex, 1U);
            }
            if (status) {
                status = candidate.payload.setInteger(
                    payloadSchema_, sampleCountSlot, &sampleCount, 1U);
            }
            if (status) {
                status = candidate.payload.setInteger(
                    payloadSchema_, constructionConfigurationIdentitySlot,
                    &constructionIdentity_, 1U);
            }
            if (!status) {
                return status;
            }
            candidate.cursorIdentity = occurrenceIdentity(
                ordinal, *next, constructionIdentity_);
            occurrence = std::move(candidate);
            available = true;
            return WVKernelStatus::ok();
        } catch (const std::bad_alloc&) {
            return allocationFailure("Unable to allocate an AlongTrack occurrence.");
        } catch (const std::overflow_error& error) {
            return sizeOverflow(error.what());
        } catch (const std::exception& error) {
            return invalid(error.what());
        }
    }

    std::size_t persistentBytes() const noexcept final {
        return sizeof(*this) +
               configuration_.persistentBytes() - sizeof(configuration_) +
               source_.persistentBytes() - sizeof(source_) +
               payloadSchema_.persistentBytes() - sizeof(payloadSchema_);
    }

private:
    const runtime::WVPortableTypedRecord configuration_;
    const MissionPassSource source_;
    const double missionEpochSeconds_ = 0.0;
    const double initialTime_ = 0.0;
    const double finalTime_ = 0.0;
    const std::int64_t constructionIdentity_ = 0;
    const runtime::WVOutputSchedulePayloadSchema payloadSchema_;
};

[[nodiscard]] std::shared_ptr<const runtime::WVOutputSchedule>
makeAlongTrackSchedule(const runtime::WVOutputScheduleRecord& record,
                       WVKernelStatus& status) {
    SourceConfiguration configuration;
    status = parseSourceConfiguration(record.configuration,
                                      scheduleConfigurationSchemaIdentifier,
                                      14U, configuration);
    if (!status || !std::isfinite(record.initialTime) ||
        !std::isfinite(record.finalTime) ||
        record.finalTime < record.initialTime) {
        if (status) {
            status = invalid("AlongTrack schedule bounds are invalid.");
        }
        return {};
    }
    runtime::WVOutputSchedulePayloadSchema payloadSchema;
    status = createSchedulePayloadSchema(payloadSchema);
    if (!status) {
        return {};
    }
    try {
        auto schedule = std::make_shared<WVAlongTrackSchedule>(
            record.configuration, std::move(configuration), record.initialTime,
            record.finalTime, std::move(payloadSchema));
        scheduleConstructionCount.fetch_add(1U, std::memory_order_relaxed);
        status = WVKernelStatus::ok();
        return schedule;
    } catch (const std::bad_alloc&) {
        status = allocationFailure("Unable to allocate an AlongTrack schedule.");
    } catch (const std::overflow_error& error) {
        status = sizeOverflow(error.what());
    } catch (const std::exception& error) {
        status = invalid(error.what());
    }
    return {};
}

[[nodiscard]] WVKernelStatus decodeOccurrenceDescriptor(
    const runtime::WVObserverOccurrencePreparationContext& context,
    const MissionPassSource& source,
    std::uint64_t expectedPayloadSchemaFingerprint,
    std::int64_t expectedConstructionIdentity,
    MissionPassDescriptor& descriptor) {
    if (context.payloadSchema == nullptr || context.payload == nullptr ||
        context.payloadSchema->fingerprint() != expectedPayloadSchemaFingerprint) {
        return invalid("AlongTrack occurrence payload schema is missing or incompatible.");
    }
    runtime::WVOutputSchedulePayloadIntegerView repeat;
    runtime::WVOutputSchedulePayloadIntegerView first;
    runtime::WVOutputSchedulePayloadIntegerView count;
    runtime::WVOutputSchedulePayloadIntegerView construction;
    auto status = context.payload->integer(
        *context.payloadSchema, repeatCycleIndexSlot, repeat);
    if (status) {
        status = context.payload->integer(
            *context.payloadSchema, firstSampleIndexSlot, first);
    }
    if (status) {
        status = context.payload->integer(
            *context.payloadSchema, sampleCountSlot, count);
    }
    if (status) {
        status = context.payload->integer(
            *context.payloadSchema, constructionConfigurationIdentitySlot,
            construction);
    }
    if (!status) {
        return status;
    }
    if (repeat.count != 1U || first.count != 1U || count.count != 1U ||
        construction.count != 1U || repeat.data == nullptr ||
        first.data == nullptr || count.data == nullptr ||
        construction.data == nullptr ||
        construction.data[0] != expectedConstructionIdentity ||
        count.data[0] < 0) {
        return invalid("AlongTrack occurrence payload is malformed or belongs to another construction.");
    }
    MissionPassDescriptor candidate{repeat.data[0], first.data[0], count.data[0]};
    if (candidate.sampleCount == 0) {
        const MissionPassDescriptor structuralProbe{
            candidate.repeatCycleIndex, candidate.firstSampleIndex, 1};
        if (!source.isStructurallyValid(structuralProbe)) {
            return invalid("Empty AlongTrack occurrence has an invalid pass identity.");
        }
    } else if (!source.isStructurallyValid(candidate)) {
        return invalid("AlongTrack occurrence contains an invalid pass descriptor.");
    }
    descriptor = candidate;
    return WVKernelStatus::ok();
}

[[nodiscard]] WVKernelStatus resizeCoordinateValues(
    runtime::WVObserverOccurrenceWorkspace& workspace,
    std::size_t count,
    double*& sampleTimes,
    double*& x,
    double*& y) {
    const std::vector<std::size_t> extents{count};
    auto status = workspace.resizeReal(0U, extents, sampleTimes);
    if (status) {
        status = workspace.resizeReal(1U, extents, x);
    }
    if (status) {
        status = workspace.resizeReal(2U, extents, y);
    }
    return status;
}

[[nodiscard]] WVKernelStatus storePassIdentity(
    runtime::WVObserverOccurrenceWorkspace& workspace,
    const MissionPassDescriptor& descriptor) {
    std::int64_t* repeat = nullptr;
    std::int64_t* first = nullptr;
    std::int64_t* count = nullptr;
    auto status = workspace.resizeInteger(3U, {}, repeat);
    if (status) {
        status = workspace.resizeInteger(4U, {}, first);
    }
    if (status) {
        status = workspace.resizeInteger(5U, {}, count);
    }
    if (!status) {
        return status;
    }
    *repeat = descriptor.repeatCycleIndex;
    *first = descriptor.firstSampleIndex;
    *count = descriptor.sampleCount;
    return WVKernelStatus::ok();
}

class WVAlongTrackObservingSystem final : public runtime::WVObservingSystem {
public:
    WVAlongTrackObservingSystem(
        std::string observerIdentifier,
        std::string observerName,
        runtime::WVPortableTypedRecord configuration,
        ObserverConfiguration resolved,
        std::vector<ResolvedField> fields,
        runtime::WVObserverOutputPlan outputPlan,
        std::uint64_t requestedFieldMask,
        std::uint64_t dependencyMask)
        : typeIdentifier_(observingSystemTypeIdentifier),
          observerIdentifier_(std::move(observerIdentifier)),
          observerName_(std::move(observerName)),
          configuration_(std::move(configuration)),
          source_(passSource(resolved.source)),
          missionEpochSeconds_(resolved.source.missionEpochSeconds),
          constructionIdentity_(resolved.source.constructionIdentity),
          requestedFieldNames_(std::move(resolved.fieldNames)),
          interpolation_(resolved.interpolation), fields_(std::move(fields)),
          outputPlan_(std::move(outputPlan)),
          payloadSchemaFingerprint_(outputPlan_.occurrencePayloadSchema.fingerprint()),
          requestedFieldMask_(requestedFieldMask),
          dependencyMask_(dependencyMask) {}

    const std::string& typeIdentifier() const noexcept final {
        return typeIdentifier_;
    }

    std::uint32_t contractVersion() const noexcept final {
        return wavevortex_extension::contractVersion;
    }

    WVKernelStatus validate(
        const runtime::WVObserverRecord& observer,
        const std::map<std::string, const runtime::WVStateBlockRecord*>&,
        std::map<std::string, std::size_t>&) const final {
        std::uint64_t resolvedDependencyMask = 0;
        for (const auto& field : fields_) {
            resolvedDependencyMask |= field.dependencyMask;
        }
        if (observer.identifier != observerIdentifier_ ||
            observer.name != observerName_ ||
            observer.typeIdentifier != typeIdentifier_ ||
            observer.contractVersion != contractVersion() ||
            !runtime::samePortableTypedRecordValue(observer.configuration,
                                                   configuration_) ||
            observer.fieldNames != requestedFieldNames_ ||
            observer.trackedFieldInterpolation != interpolation_ ||
            !observer.stateBlockIdentifiers.empty() ||
            requestedFieldMask_ == 0U ||
            resolvedDependencyMask != dependencyMask_) {
            return invalid("Resolved AlongTrack observer record has drifted.");
        }
        return WVKernelStatus::ok();
    }

    WVKernelStatus executionPlan(
        const runtime::WVObserverRecord& observer,
        runtime::WVObserverExecutionPlan& plan) const final {
        if (observer.identifier != observerIdentifier_) {
            return invalid("AlongTrack execution plan requested for another observer.");
        }
        runtime::WVObserverExecutionPlan candidate;
        candidate.fieldListAttribute = "fieldNames";
        candidate.persistedName = observerName_;
        candidate.outputFields = requestedFieldNames_;
        plan = std::move(candidate);
        return WVKernelStatus::ok();
    }

    WVKernelStatus outputPlan(
        const runtime::WVObserverRecord& observer,
        const runtime::WVObserverOutputPlanningContext& context,
        runtime::WVObserverOutputPlan& plan) const final {
        outputPlanResolutionCount.fetch_add(1U, std::memory_order_relaxed);
        if (observer.identifier != observerIdentifier_ ||
            !runtime::samePortableTypedRecordValue(observer.configuration,
                                                   configuration_)) {
            return invalid("AlongTrack output plan requested for another resolved observer.");
        }
        const auto status = validatePlanningDomain(
            sourceConfigurationForPlanning(), context);
        if (!status) {
            return status;
        }
        plan = outputPlan_;
        return WVKernelStatus::ok();
    }

    WVKernelStatus prepareOccurrence(
        const runtime::WVObserverRecord&,
        const runtime::WVObserverOutputPlan& plan,
        const runtime::WVObserverOccurrencePreparationContext& context,
        runtime::WVObserverOccurrenceWorkspace& workspace) const final {
        occurrencePreparationCount.fetch_add(1U, std::memory_order_relaxed);
        MissionPassDescriptor descriptor;
        auto status = decodeOccurrenceDescriptor(
            context, source_, payloadSchemaFingerprint_, constructionIdentity_,
            descriptor);
        if (!status) {
            return status;
        }

        try {
            Pass pass;
            const MissionPassDescriptor triggerDescriptor =
                descriptor.sampleCount == 0
                    ? MissionPassDescriptor{descriptor.repeatCycleIndex,
                                            descriptor.firstSampleIndex, 1}
                    : descriptor;
            const double trigger = missionPassTriggerTime(
                source_, triggerDescriptor, missionEpochSeconds_);
            if (trigger != context.scheduledTime) {
                return invalid("AlongTrack occurrence time conflicts with its pass identity.");
            }
            if (descriptor.sampleCount > 0) {
                pass = reconstructMissionPass(source_, descriptor,
                                              missionEpochSeconds_);
            }
            workspace.prepareFor(plan);
            const auto count = pass.size();
            double* sampleTimes = nullptr;
            double* x = nullptr;
            double* y = nullptr;
            status = resizeCoordinateValues(workspace, count, sampleTimes, x, y);
            if (!status) {
                return status;
            }
            status = storePassIdentity(workspace, descriptor);
            if (!status) {
                return status;
            }

            auto& positions = workspace.positionSets.at(0U);
            positions.extents = {count};
            positions.sampleTimes.resize(count);
            positions.x.resize(count);
            positions.y.resize(count);
            positions.z.clear();
            for (std::size_t index = 0; index < count; ++index) {
                const auto& point = pass[index];
                sampleTimes[index] = point.timeSeconds;
                x[index] = point.xMeters;
                y[index] = point.yMeters;
                positions.sampleTimes[index] = point.timeSeconds;
                positions.x[index] = point.xMeters;
                positions.y[index] = point.yMeters;
            }
            return WVKernelStatus::ok();
        } catch (const std::bad_alloc&) {
            return allocationFailure("Unable to allocate AlongTrack event geometry.");
        } catch (const std::overflow_error& error) {
            return sizeOverflow(error.what());
        } catch (const std::exception& error) {
            return invalid(error.what());
        }
    }

    WVKernelStatus observationBatch(
        const runtime::WVObserverRecord& observer,
        const runtime::WVObserverOutputPlan& plan,
        const runtime::WVObserverOutputEvaluationContext& context,
        runtime::WVObservationBatchKind kind,
        runtime::WVObservationBatch& batch) const final {
        batchBuildCount.fetch_add(1U, std::memory_order_relaxed);
        return runtime::WVObservingSystem::observationBatch(
            observer, plan, context, kind, batch);
    }

    std::size_t persistentBytes() const noexcept final {
        std::size_t bytes = sizeof(*this) + typeIdentifier_.capacity() +
                            observerIdentifier_.capacity() +
                            observerName_.capacity() +
                            configuration_.persistentBytes() -
                            sizeof(configuration_) +
                            source_.persistentBytes() - sizeof(source_) +
                            runtime::observerOutputPlanRetainedBytes(outputPlan_) +
                            requestedFieldNames_.capacity() * sizeof(std::string) +
                            fields_.capacity() * sizeof(ResolvedField);
        for (const auto& name : requestedFieldNames_) {
            bytes += name.capacity();
        }
        for (const auto& field : fields_) {
            bytes += field.name.capacity() + field.units.capacity() +
                     field.description.capacity();
        }
        return bytes;
    }

private:
    [[nodiscard]] SourceConfiguration sourceConfigurationForPlanning() const {
        SourceConfiguration result;
        result.windowWidthMeters = source_.window().widthMeters();
        result.windowHeightMeters = source_.window().heightMeters();
        return result;
    }

    const std::string typeIdentifier_;
    const std::string observerIdentifier_;
    const std::string observerName_;
    const runtime::WVPortableTypedRecord configuration_;
    const MissionPassSource source_;
    const double missionEpochSeconds_ = 0.0;
    const std::int64_t constructionIdentity_ = 0;
    const std::vector<std::string> requestedFieldNames_;
    const runtime::WVPositionInterpolation interpolation_;
    const std::vector<ResolvedField> fields_;
    const runtime::WVObserverOutputPlan outputPlan_;
    const std::uint64_t payloadSchemaFingerprint_ = 0;
    const std::uint64_t requestedFieldMask_ = 0;
    const std::uint64_t dependencyMask_ = 0;
};

[[nodiscard]] WVKernelStatus resolveObserverConfiguration(
    const runtime::WVObserverRecord& record,
    runtime::WVPortableTypedRecord& configuration) {
    observerConfigurationResolutionCount.fetch_add(1U,
                                                   std::memory_order_relaxed);
    ObserverConfiguration resolved;
    auto status = parseObserverConfiguration(record.configuration, resolved);
    if (!status) {
        return status;
    }
    status = validateObserverRecord(record, record.configuration, resolved);
    if (!status) {
        return status;
    }
    // This callback canonicalizes construction data only. Field identities,
    // dependencies, schema, and the coarse evaluation plan are resolved once
    // by the per-record factory below. The independent output-plan resolver is
    // used only for WVM's provider-free persisted-schema preflight.
    configuration = record.configuration;
    return WVKernelStatus::ok();
}

[[nodiscard]] WVKernelStatus makeAlongTrackObserver(
    const runtime::WVObserverRecord& record,
    const runtime::WVPortableTypedRecord& configuration,
    std::shared_ptr<const runtime::WVObservingSystem>& output) {
    ObserverConfiguration resolved;
    auto status = parseObserverConfiguration(configuration, resolved);
    if (!status) {
        return status;
    }
    status = validateObserverRecord(record, configuration, resolved);
    if (!status) {
        return status;
    }
    std::vector<ResolvedField> fields;
    std::uint64_t requestedMask = 0;
    std::uint64_t dependencyMask = 0;
    status = resolveFields(resolved, fields, requestedMask, dependencyMask);
    if (!status) {
        return status;
    }
    runtime::WVObserverOutputPlan plan;
    status = buildOutputPlan(record, resolved, fields, plan);
    if (!status) {
        return status;
    }
    try {
        output = std::make_shared<WVAlongTrackObservingSystem>(
            record.identifier, record.name, configuration, std::move(resolved),
            std::move(fields), std::move(plan), requestedMask, dependencyMask);
        observerConstructionCount.fetch_add(1U, std::memory_order_relaxed);
        return WVKernelStatus::ok();
    } catch (const std::bad_alloc&) {
        return allocationFailure("Unable to allocate an AlongTrack observer.");
    } catch (const std::overflow_error& error) {
        return sizeOverflow(error.what());
    } catch (const std::exception& error) {
        return invalid(error.what());
    }
}

[[nodiscard]] WVKernelStatus resolveAlongTrackOutputPlan(
    const runtime::WVObserverRecord& record,
    const runtime::WVObserverOutputPlanningContext& context,
    runtime::WVObserverOutputPlan& output) {
    outputPlanResolutionCount.fetch_add(1U, std::memory_order_relaxed);
    ObserverConfiguration resolved;
    auto status = parseObserverConfiguration(record.configuration, resolved);
    if (!status) {
        return status;
    }
    status = validateObserverRecord(record, record.configuration, resolved);
    if (!status) {
        return status;
    }
    status = validatePlanningDomain(resolved.source, context);
    if (!status) {
        return status;
    }
    std::vector<ResolvedField> fields;
    std::uint64_t requestedMask = 0;
    std::uint64_t dependencyMask = 0;
    status = resolveFields(resolved, fields, requestedMask, dependencyMask);
    if (!status) {
        return status;
    }
    return buildOutputPlan(record, resolved, fields, output);
}

} // namespace

void resetCounters() noexcept {
    scheduleConstructionCount.store(0U, std::memory_order_relaxed);
    schedulePeekCount.store(0U, std::memory_order_relaxed);
    observerConfigurationResolutionCount.store(0U, std::memory_order_relaxed);
    observerConstructionCount.store(0U, std::memory_order_relaxed);
    outputPlanResolutionCount.store(0U, std::memory_order_relaxed);
    occurrencePreparationCount.store(0U, std::memory_order_relaxed);
    batchBuildCount.store(0U, std::memory_order_relaxed);
}

ExtensionCounters counters() noexcept {
    return {
        scheduleConstructionCount.load(std::memory_order_relaxed),
        schedulePeekCount.load(std::memory_order_relaxed),
        observerConfigurationResolutionCount.load(std::memory_order_relaxed),
        observerConstructionCount.load(std::memory_order_relaxed),
        outputPlanResolutionCount.load(std::memory_order_relaxed),
        occurrencePreparationCount.load(std::memory_order_relaxed),
        batchBuildCount.load(std::memory_order_relaxed)};
}

WVKernelStatus createSchedulePayloadSchema(
    runtime::WVOutputSchedulePayloadSchema& schema) {
    return runtime::WVOutputSchedulePayloadSchema::create(
        schedulePayloadSchemaIdentifier, contractVersion,
        {{repeatCycleIndexPayloadField,
          runtime::WVOutputSchedulePayloadType::integer64, {}},
         {firstSampleIndexPayloadField,
          runtime::WVOutputSchedulePayloadType::integer64, {}},
         {sampleCountPayloadField,
          runtime::WVOutputSchedulePayloadType::integer64, {}},
         {constructionConfigurationIdentityPayloadField,
          runtime::WVOutputSchedulePayloadType::integer64, {}}},
        schema);
}

WVKernelStatus constructionConfigurationIdentity(
    const runtime::WVPortableTypedRecord& configuration,
    std::int64_t& identity) {
    try {
        SourceConfiguration resolved;
        WVKernelStatus status;
        if (configuration.schemaIdentifier ==
            scheduleConfigurationSchemaIdentifier) {
            status = parseSourceConfiguration(
                configuration, scheduleConfigurationSchemaIdentifier, 14U,
                resolved);
        } else if (configuration.schemaIdentifier ==
                   observerConfigurationSchemaIdentifier) {
            ObserverConfiguration observer;
            status = parseObserverConfiguration(configuration, observer);
            if (status) {
                resolved = std::move(observer.source);
            }
        } else {
            return invalid("Unsupported AlongTrack construction schema identity.");
        }
        if (!status) {
            return status;
        }
        identity = resolved.constructionIdentity;
        return WVKernelStatus::ok();
    } catch (const std::bad_alloc&) {
        return allocationFailure(
            "Unable to resolve an AlongTrack construction identity.");
    } catch (const std::exception& error) {
        return invalid(error.what());
    }
}

WVKernelStatus registerAlongTrackExtensions(
    runtime::WVExtensionCatalogBuilder& builder) {
    auto status = builder.addOutputScheduleFactory(
        {scheduleTypeIdentifier, contractVersion, &makeAlongTrackSchedule});
    if (!status) {
        return status;
    }
    return builder.addObserverFactory(runtime::WVObserverFactoryRegistration(
        observingSystemTypeIdentifier, contractVersion, &makeAlongTrackObserver,
        &resolveObserverConfiguration, {}, {}, &resolveAlongTrackOutputPlan));
}

} // namespace alongtrack::wavevortex_extension
