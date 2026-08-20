#pragma once

#include "WaveVortexRuntime/WVExtensionCatalog.hpp"

#include <cstddef>
#include <cstdint>

namespace alongtrack::wavevortex_extension {

inline constexpr char scheduleTypeIdentifier[] = "WVAlongTrackSchedule";
inline constexpr char observingSystemTypeIdentifier[] =
    "WVAlongTrackObservingSystem";
inline constexpr std::uint32_t contractVersion = 1;

inline constexpr char scheduleConfigurationSchemaIdentifier[] =
    "wv-along-track-schedule-configuration-v1";
inline constexpr char observerConfigurationSchemaIdentifier[] =
    "wv-along-track-observer-configuration-v1";
inline constexpr char scheduleCursorSchemaIdentifier[] =
    "wv-along-track-pass-cursor-v1";
inline constexpr char schedulePayloadSchemaIdentifier[] =
    "wv-along-track-pass-payload-v1";
inline constexpr char cursorDescriptorField[] = "descriptor";
inline constexpr char repeatCycleIndexPayloadField[] = "repeatCycleIndex";
inline constexpr char firstSampleIndexPayloadField[] = "firstSampleIndex";
inline constexpr char sampleCountPayloadField[] = "sampleCount";
inline constexpr char constructionConfigurationIdentityPayloadField[] =
    "constructionConfigurationIdentity";

// Construction-record names are public serialization metadata. Production
// event paths use only the numeric payload slots below.
inline constexpr char missionKeyField[] = "missionKey";
inline constexpr char semiMajorAxisKmField[] = "semiMajorAxisKm";
inline constexpr char resolvedSemiMajorAxisKmField[] =
    "resolvedSemiMajorAxisKm";
inline constexpr char eccentricityField[] = "eccentricity";
inline constexpr char inclinationDegreesField[] = "inclinationDegrees";
inline constexpr char ascendingNodeLongitudeDegreesField[] =
    "ascendingNodeLongitudeDegrees";
// Zero denotes a non-repeating/geodetic mission. Positive values are the
// exact number of passes per repeat cycle.
inline constexpr char passesPerCycleField[] = "passesPerCycle";
inline constexpr char missionEpochSecondsField[] = "missionEpochSeconds";
inline constexpr char windowWidthMetersField[] = "windowWidthMeters";
inline constexpr char windowHeightMetersField[] = "windowHeightMeters";
inline constexpr char centerLatitudeDegreesField[] = "centerLatitudeDegrees";
inline constexpr char centerLongitudeDegreesField[] =
    "centerLongitudeDegrees";
inline constexpr char lowerLeftOriginField[] = "lowerLeftOrigin";
inline constexpr char sampleIntervalSecondsField[] = "sampleIntervalSeconds";
inline constexpr char fieldNamesField[] = "fieldNames";
// WVPositionInterpolation encoded as zero (linear) or one (spline).
inline constexpr char interpolationField[] = "interpolation";

inline constexpr std::size_t repeatCycleIndexSlot = 0;
inline constexpr std::size_t firstSampleIndexSlot = 1;
inline constexpr std::size_t sampleCountSlot = 2;
inline constexpr std::size_t constructionConfigurationIdentitySlot = 3;

struct ExtensionCounters final {
    std::size_t scheduleConstructionCount = 0;
    std::size_t schedulePeekCount = 0;
    std::size_t observerConfigurationResolutionCount = 0;
    std::size_t observerConstructionCount = 0;
    std::size_t outputPlanResolutionCount = 0;
    std::size_t occurrencePreparationCount = 0;
    std::size_t batchBuildCount = 0;
};

void resetCounters() noexcept;
[[nodiscard]] ExtensionCounters counters() noexcept;

[[nodiscard]] ::wavevortex::WVKernelStatus
createSchedulePayloadSchema(
    ::wavevortex::runtime::WVOutputSchedulePayloadSchema& schema);

// Produces the signed, bit-exact identity carried by payloads and cursors.
// Both supported construction-record schemas resolve to the same identity
// when their mission, epoch, projection, and cadence fields match exactly.
[[nodiscard]] ::wavevortex::WVKernelStatus
constructionConfigurationIdentity(
    const ::wavevortex::runtime::WVPortableTypedRecord& configuration,
    std::int64_t& identity);

// Registration is explicit and must be called on the application-owned
// builder before freeze(). No static or process-global registry is used.
[[nodiscard]] ::wavevortex::WVKernelStatus
registerAlongTrackExtensions(
    ::wavevortex::runtime::WVExtensionCatalogBuilder& builder);

} // namespace alongtrack::wavevortex_extension
