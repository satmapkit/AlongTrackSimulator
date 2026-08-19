#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace alongtrack {

inline constexpr double earthGravitationalParameterKm3PerS2 = 398600.4418;
inline constexpr double earthJ2 = 1.08262668e-3;
inline constexpr double earthEquatorialRadiusKm = 6378.1363;
inline constexpr double siderealDaySeconds = 86164.0;

struct MissionDefinition final {
    std::string key;
    double semiMajorAxisKm;
    double resolvedSemiMajorAxisKm;
    double eccentricity;
    double inclinationDegrees;
    double ascendingNodeLongitudeDegrees;
    double passesPerCycle;
};

class OrbitConfiguration final {
public:
    OrbitConfiguration(double semiMajorAxisKm,
                       double eccentricity,
                       double inclinationDegrees,
                       double ascendingNodeLongitudeDegrees);

    OrbitConfiguration(const OrbitConfiguration&) = default;
    OrbitConfiguration& operator=(const OrbitConfiguration&) = delete;

    [[nodiscard]] double semiMajorAxisKm() const noexcept;
    [[nodiscard]] double eccentricity() const noexcept;
    [[nodiscard]] double inclinationDegrees() const noexcept;
    [[nodiscard]] double ascendingNodeLongitudeDegrees() const noexcept;

private:
    const double semiMajorAxisKm_;
    const double eccentricity_;
    const double inclinationDegrees_;
    const double ascendingNodeLongitudeDegrees_;
};

class ResolvedMission final {
public:
    explicit ResolvedMission(const MissionDefinition& definition);

    ResolvedMission(const ResolvedMission&) = default;
    ResolvedMission& operator=(const ResolvedMission&) = delete;

    [[nodiscard]] static ResolvedMission fromCatalog(std::string_view key);
    [[nodiscard]] static std::vector<std::string_view> catalogKeys();

    [[nodiscard]] const std::string& key() const noexcept;
    [[nodiscard]] const OrbitConfiguration& orbit() const noexcept;
    [[nodiscard]] bool isRepeating() const noexcept;
    [[nodiscard]] double passesPerCycle() const noexcept;
    [[nodiscard]] double orbitalPeriodSeconds() const noexcept;
    [[nodiscard]] double nodalPeriodSeconds() const noexcept;
    [[nodiscard]] double repeatCycleSeconds() const noexcept;

private:
    const std::string key_;
    const OrbitConfiguration orbit_;
    const double passesPerCycle_;
    const double orbitalPeriodSeconds_;
    const double nodalPeriodSeconds_;
    const double repeatCycleSeconds_;
};

struct GeographicPoint final {
    double latitudeDegrees;
    double longitudeDegrees;
};

struct ProjectedPoint final {
    double xMeters;
    double yMeters;
};

struct TimedProjectedPoint final {
    double xMeters;
    double yMeters;
    double timeSeconds;
};

using Pass = std::vector<TimedProjectedPoint>;

[[nodiscard]] double wrapLongitudeDegrees(double longitudeDegrees) noexcept;
[[nodiscard]] double orbitalPeriodSeconds(double semiMajorAxisKm) noexcept;
[[nodiscard]] double nodalPeriodSeconds(const OrbitConfiguration& orbit) noexcept;

// The portable implementation of the MATLAB nodal-precession algorithm. All angular
// configuration inputs are degrees and time is seconds.
[[nodiscard]] GeographicPoint groundTrackWithNodalPrecession(
    const OrbitConfiguration& orbit,
    double argumentOfPeriapsisDegrees,
    double meanAnomalyAtEpochDegrees,
    double timeSeconds) noexcept;

// Matches computeGroundTrackCircularOrbit. The MATLAB routine uses inclination as a
// radian angle despite its legacy documentation, so that unit is explicit here.
[[nodiscard]] GeographicPoint circularOrbitGroundTrack(
    double semiMajorAxisKm,
    double inclinationRadians,
    double meanAnomalyAtEpochRadians,
    double timeSeconds) noexcept;

// Mission time zero follows the MATLAB public mission API and is shifted by one quarter
// orbital period relative to the low-level orbital epoch.
[[nodiscard]] GeographicPoint missionGroundTrackPoint(
    const ResolvedMission& mission,
    double missionTimeSeconds) noexcept;

class TransverseMercator final {
public:
    explicit TransverseMercator(double centralMeridianDegrees, double scale = 0.9996);

    TransverseMercator(const TransverseMercator&) = default;
    TransverseMercator& operator=(const TransverseMercator&) = delete;

    [[nodiscard]] double centralMeridianDegrees() const noexcept;
    [[nodiscard]] double scale() const noexcept;
    [[nodiscard]] ProjectedPoint forward(GeographicPoint point) const noexcept;
    [[nodiscard]] GeographicPoint inverse(ProjectedPoint point) const noexcept;

private:
    const double centralMeridianDegrees_;
    const double scale_;
};

enum class WindowOrigin {
    centered,
    lowerLeft,
};

class ProjectionWindow final {
public:
    ProjectionWindow(double widthMeters,
                     double heightMeters,
                     double centerLatitudeDegrees,
                     double centerLongitudeDegrees,
                     WindowOrigin origin = WindowOrigin::lowerLeft);

    ProjectionWindow(const ProjectionWindow&) = default;
    ProjectionWindow& operator=(const ProjectionWindow&) = delete;

    [[nodiscard]] double widthMeters() const noexcept;
    [[nodiscard]] double heightMeters() const noexcept;
    [[nodiscard]] double centerLatitudeDegrees() const noexcept;
    [[nodiscard]] double centerLongitudeDegrees() const noexcept;
    [[nodiscard]] WindowOrigin origin() const noexcept;

private:
    const double widthMeters_;
    const double heightMeters_;
    const double centerLatitudeDegrees_;
    const double centerLongitudeDegrees_;
    const WindowOrigin origin_;
};

// Returns local window coordinates when a geographic point is on or inside the
// projection boundary, including either coordinate-origin convention.
[[nodiscard]] std::optional<ProjectedPoint> projectIntoWindow(
    const ProjectionWindow& window,
    GeographicPoint point);

// Samples and immediately filters one requested working window. The generator owns no
// trajectory storage; retained storage is only the returned points that intersect the box.
[[nodiscard]] std::vector<TimedProjectedPoint> projectedMissionWindow(
    const ResolvedMission& mission,
    const ProjectionWindow& window,
    double initialTimeSeconds,
    double finalTimeSeconds,
    double sampleIntervalSeconds = 1.0);

[[nodiscard]] std::vector<Pass> segmentPasses(
    const std::vector<TimedProjectedPoint>& track,
    double maximumContinuousGapSeconds = 1.0);

[[nodiscard]] std::vector<double> passTriggerTimes(const std::vector<Pass>& passes);

} // namespace alongtrack
