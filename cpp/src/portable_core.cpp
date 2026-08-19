#include "alongtrack/portable_core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace alongtrack {
namespace {

constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double degreesToRadians = pi / 180.0;
constexpr double radiansToDegrees = 180.0 / pi;
constexpr double groundTrackJ2 = 1.08263e-3;
constexpr double groundTrackEarthRadiusKm = 6378.137;
constexpr double earthRotationRadiansPerSecond = 7.2921159e-5;
constexpr double wgs84SemiMajorAxisMeters = 6378137.0;
constexpr double wgs84InverseFlattening = 298.257223563;

// resolvedSemiMajorAxisKm is the immutable post-construction value produced by
// AlongTrackSimulator.m for each catalog record. The MATLAB fixture generator verifies
// all 26 values and their derived periods before the kernel is used in an inner loop.
const std::array<MissionDefinition, 26> missionCatalog{{
    {"al", 7159.4960000000001, 7165.0944771205905, 0.000165, 98.549999999999997, 0.125, 1002.0},
    {"alg", 8000.4960000000001, 8000.4960000000001, 0.0, 98.549999999999997, 0.125, std::numeric_limits<double>::infinity()},
    {"c2", 7095.0, 7095.0, 0.0, 92.030000000000001, 22.699999999999999, std::numeric_limits<double>::infinity()},
    {"c2n", 7095.0, 7095.0, 0.0, 92.030000000000001, 0.0, std::numeric_limits<double>::infinity()},
    {"e1", 7159.4960000000001, 7165.0838939880377, 0.0001042, 98.543000000000006, 0.20000000000000001, 1002.0},
    {"e1g", 7147.1909999999998, 7152.7944244358187, 0.0001042, 98.491, 25.879999999999999, 4822.0},
    {"e2", 7159.5, 7165.0838857591152, 0.0001042, 98.543000000000006, 0.050000000000000003, 1002.0},
    {"en", 7159.5, 7165.094554245471, 0.001165, 98.549999999999997, -25.0, 1002.0},
    {"enn", 7142.0, 7147.778913402557, 0.001158, 98.549999999999997, 0.67000000000000004, 862.0},
    {"g2", 7146.1999999999998, 7166.4120234012616, 0.00398, 108.0596, 1.0800000000000001, 488.0},
    {"h2a", 7349.0, 7347.0344514369963, 0.0, 99.299999999999997, -122.932, 386.0},
    {"h2b", 7351.0, 7347.034445726872, 0.0, 99.299999999999997, -37.150399999999998, 386.0},
    {"j1", 7714.4278000000004, 7716.3604974707214, 0.000095, 66.039000000000001, 99.924199999999999, 254.0},
    {"j1g", 7702.4369999999999, 7702.4369999999999, 0.0002, 66.042000000000002, -51.219999999999999, std::numeric_limits<double>::infinity()},
    {"j1n", 7714.4279999999999, 7716.3605135945327, 0.000095, 66.039000000000001, 98.510000000000005, 254.0},
    {"j2", 7714.4278000000004, 7716.3604974707214, 0.000095, 66.039000000000001, 99.924199999999999, 254.0},
    {"j2g", 7702.0, 7702.0, 0.00001, 66.039000000000001, 86.049999999999997, std::numeric_limits<double>::infinity()},
    {"j2n", 7714.4278000000004, 7716.3604974707214, 0.000095, 66.039000000000001, 98.510000000000005, 254.0},
    {"j3", 7714.4278000000004, 7716.3604974707214, 0.000095, 66.039000000000001, 99.924199999999999, 254.0},
    {"j3n", 7714.4278000000004, 7716.3604974707214, 0.000095, 66.039000000000001, 98.510000000000005, 254.0},
    {"s3a", 7177.9265400000004, 7183.542766297056, 0.001148, 98.645589000000001, 0.55000000000000004, 770.0},
    {"s3b", 7177.9265400000004, 7183.542766297056, 0.001148, 98.645589000000001, 0.17000000000000001, 770.0},
    {"s6a", 7714.4278000000004, 7716.3604974707214, 0.000095, 66.039000000000001, 99.924199999999999, 254.0},
    {"swon", 7268.7188999999998, 7273.6553437548282, 0.00105, 77.599999999999994, 0.0, 584.0},
    {"tp", 7714.4278000000004, 7716.3604974707214, 0.000095, 66.039000000000001, 98.510000000000005, 254.0},
    {"tpn", 7714.4278000000004, 7716.3604974707214, 0.000095, 66.039000000000001, 99.924199999999999, 254.0},
}};

[[nodiscard]] double eccentricAnomaly(double meanAnomaly, double eccentricity) noexcept {
    double eccentricAnomalyValue = meanAnomaly;
    for (int iteration = 0; iteration < 100; ++iteration) {
        const double residual = eccentricAnomalyValue - eccentricity * std::sin(eccentricAnomalyValue) - meanAnomaly;
        const double derivative = 1.0 - eccentricity * std::cos(eccentricAnomalyValue);
        const double update = -residual / derivative;
        eccentricAnomalyValue += update;
        if (std::abs(update) < 1.0e-8) {
            break;
        }
    }
    return eccentricAnomalyValue;
}

[[nodiscard]] double meridionalArc(double latitudeRadians) noexcept {
    constexpr double c00 = 1.0;
    constexpr double c02 = 0.25;
    constexpr double c04 = 0.046875;
    constexpr double c06 = 0.01953125;
    constexpr double c08 = 0.01068115234375;
    constexpr double c22 = 0.75;
    constexpr double c44 = 0.46875;
    constexpr double c46 = 0.01302083333333333333;
    constexpr double c48 = 0.00712076822916666666;
    constexpr double c66 = 0.36458333333333333333;
    constexpr double c68 = 0.00569661458333333333;
    constexpr double c88 = 0.3076171875;

    const double flattening = 1.0 / wgs84InverseFlattening;
    const double eccentricitySquared = flattening * (2.0 - flattening);
    double power = eccentricitySquared * eccentricitySquared;
    const double aPrime = c00 - eccentricitySquared * (c02 + eccentricitySquared * (c04 + eccentricitySquared * (c06 + eccentricitySquared * c08)));
    const double bPrime = eccentricitySquared * (c22 - eccentricitySquared * (c04 + eccentricitySquared * (c06 + eccentricitySquared * c08)));
    const double cPrime = power * (c44 - eccentricitySquared * (c46 + eccentricitySquared * c48));
    power *= eccentricitySquared;
    const double dPrime = power * (c66 - eccentricitySquared * c68);
    const double ePrime = power * eccentricitySquared * c88;
    const double sine = std::sin(latitudeRadians);
    const double sineSquared = sine * sine;
    const double sineCosine = std::cos(latitudeRadians) * sine;
    return wgs84SemiMajorAxisMeters *
           (aPrime * latitudeRadians - sineCosine *
            (bPrime + sineSquared * (cPrime + sineSquared * (dPrime + sineSquared * ePrime))));
}

[[nodiscard]] double inverseMeridionalArc(double yMeters) noexcept {
    const double flattening = 1.0 / wgs84InverseFlattening;
    const double eccentricitySquared = flattening * (2.0 - flattening);
    const double inverseOneMinusEccentricitySquared = 1.0 / (1.0 - eccentricitySquared);
    double latitude = yMeters / wgs84SemiMajorAxisMeters;
    for (int iteration = 0; iteration < 10; ++iteration) {
        const double sine = std::sin(latitude);
        const double factor = 1.0 - eccentricitySquared * sine * sine;
        const double update = (meridionalArc(latitude) - yMeters) *
                              (factor * std::sqrt(factor)) *
                              inverseOneMinusEccentricitySquared /
                              wgs84SemiMajorAxisMeters;
        latitude -= update;
        if (std::abs(update) < 1.0e-11) {
            break;
        }
    }
    return latitude;
}

[[nodiscard]] bool isFinitePositive(double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

struct GeographicBounds final {
    double minimumLatitude;
    double maximumLatitude;
    double minimumLongitude;
    double maximumLongitude;
    bool crossesAntimeridian;

    [[nodiscard]] bool contains(GeographicPoint point) const noexcept {
        const bool withinLatitude = point.latitudeDegrees >= minimumLatitude &&
                                    point.latitudeDegrees <= maximumLatitude;
        const bool withinLongitude = crossesAntimeridian
                                         ? true
                                         : point.longitudeDegrees >= minimumLongitude &&
                                               point.longitudeDegrees <= maximumLongitude;
        return withinLatitude && withinLongitude;
    }
};

[[nodiscard]] GeographicBounds geographicBoundsForWindow(const ProjectionWindow& window,
                                                          const TransverseMercator& projection,
                                                          ProjectedPoint projectedCenter) noexcept {
    const double halfWidth = window.widthMeters() / 2.0;
    const double halfHeight = window.heightMeters() / 2.0;
    const std::array<ProjectedPoint, 6> boundaryPoints{{
        {projectedCenter.xMeters - halfWidth, projectedCenter.yMeters - halfHeight},
        {projectedCenter.xMeters - halfWidth, projectedCenter.yMeters + halfHeight},
        {projectedCenter.xMeters, projectedCenter.yMeters + halfHeight},
        {projectedCenter.xMeters, projectedCenter.yMeters - halfHeight},
        {projectedCenter.xMeters + halfWidth, projectedCenter.yMeters - halfHeight},
        {projectedCenter.xMeters + halfWidth, projectedCenter.yMeters + halfHeight},
    }};
    GeographicBounds bounds{90.0, -90.0, 180.0, -180.0, false};
    for (const ProjectedPoint point : boundaryPoints) {
        const GeographicPoint geographic = projection.inverse(point);
        bounds.minimumLatitude = std::min(bounds.minimumLatitude, geographic.latitudeDegrees);
        bounds.maximumLatitude = std::max(bounds.maximumLatitude, geographic.latitudeDegrees);
        bounds.minimumLongitude = std::min(bounds.minimumLongitude, geographic.longitudeDegrees);
        bounds.maximumLongitude = std::max(bounds.maximumLongitude, geographic.longitudeDegrees);
    }
    bounds.crossesAntimeridian = bounds.maximumLongitude - bounds.minimumLongitude > 180.0;
    return bounds;
}

} // namespace

OrbitConfiguration::OrbitConfiguration(double semiMajorAxisKm,
                                       double eccentricity,
                                       double inclinationDegrees,
                                       double ascendingNodeLongitudeDegrees)
    : semiMajorAxisKm_(semiMajorAxisKm),
      eccentricity_(eccentricity),
      inclinationDegrees_(inclinationDegrees),
      ascendingNodeLongitudeDegrees_(ascendingNodeLongitudeDegrees) {
    if (!isFinitePositive(semiMajorAxisKm_)) {
        throw std::invalid_argument("semi-major axis must be finite and positive");
    }
    if (!std::isfinite(eccentricity_) || eccentricity_ < 0.0 || eccentricity_ >= 1.0) {
        throw std::invalid_argument("eccentricity must be finite and in [0, 1)");
    }
    if (!std::isfinite(inclinationDegrees_)) {
        throw std::invalid_argument("inclination must be finite");
    }
    if (!std::isfinite(ascendingNodeLongitudeDegrees_)) {
        throw std::invalid_argument("ascending-node longitude must be finite");
    }
}

double OrbitConfiguration::semiMajorAxisKm() const noexcept { return semiMajorAxisKm_; }
double OrbitConfiguration::eccentricity() const noexcept { return eccentricity_; }
double OrbitConfiguration::inclinationDegrees() const noexcept { return inclinationDegrees_; }
double OrbitConfiguration::ascendingNodeLongitudeDegrees() const noexcept { return ascendingNodeLongitudeDegrees_; }

ResolvedMission::ResolvedMission(const MissionDefinition& definition)
    : key_(definition.key),
      orbit_(definition.resolvedSemiMajorAxisKm,
             definition.eccentricity,
             definition.inclinationDegrees,
             definition.ascendingNodeLongitudeDegrees),
      passesPerCycle_(definition.passesPerCycle),
      orbitalPeriodSeconds_(alongtrack::orbitalPeriodSeconds(orbit_.semiMajorAxisKm())),
      nodalPeriodSeconds_(alongtrack::nodalPeriodSeconds(orbit_)),
      repeatCycleSeconds_(std::isfinite(passesPerCycle_)
                              ? orbitalPeriodSeconds_ * passesPerCycle_ / 2.0
                              : std::numeric_limits<double>::infinity()) {
    if (key_.empty()) {
        throw std::invalid_argument("mission key must not be empty");
    }
    if (!(std::isinf(passesPerCycle_) || isFinitePositive(passesPerCycle_))) {
        throw std::invalid_argument("passes per cycle must be positive or infinity");
    }
}

ResolvedMission ResolvedMission::fromCatalog(std::string_view key) {
    const auto found = std::find_if(missionCatalog.begin(), missionCatalog.end(),
                                    [key](const MissionDefinition& definition) {
                                        return definition.key == key;
                                    });
    if (found == missionCatalog.end()) {
        throw std::invalid_argument("unknown AlongTrack mission: " + std::string(key));
    }
    return ResolvedMission(*found);
}

std::vector<std::string_view> ResolvedMission::catalogKeys() {
    std::vector<std::string_view> keys;
    keys.reserve(missionCatalog.size());
    for (const auto& mission : missionCatalog) {
        keys.emplace_back(mission.key);
    }
    return keys;
}

const std::string& ResolvedMission::key() const noexcept { return key_; }
const OrbitConfiguration& ResolvedMission::orbit() const noexcept { return orbit_; }
bool ResolvedMission::isRepeating() const noexcept { return std::isfinite(repeatCycleSeconds_); }
double ResolvedMission::passesPerCycle() const noexcept { return passesPerCycle_; }
double ResolvedMission::orbitalPeriodSeconds() const noexcept { return orbitalPeriodSeconds_; }
double ResolvedMission::nodalPeriodSeconds() const noexcept { return nodalPeriodSeconds_; }
double ResolvedMission::repeatCycleSeconds() const noexcept { return repeatCycleSeconds_; }

double wrapLongitudeDegrees(double longitudeDegrees) noexcept {
    double wrapped = std::fmod(longitudeDegrees + 180.0, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return wrapped - 180.0;
}

double orbitalPeriodSeconds(double semiMajorAxisKm) noexcept {
    return 2.0 * pi * std::sqrt(semiMajorAxisKm * semiMajorAxisKm * semiMajorAxisKm /
                                earthGravitationalParameterKm3PerS2);
}

double nodalPeriodSeconds(const OrbitConfiguration& orbit) noexcept {
    const double inclination = orbit.inclinationDegrees() * degreesToRadians;
    const double meanMotion = std::sqrt(earthGravitationalParameterKm3PerS2 /
                                        std::pow(orbit.semiMajorAxisKm(), 3.0));
    const double a = 3.0 * earthJ2 * (4.0 - 5.0 * std::sin(inclination) * std::sin(inclination)) /
                     (4.0 * std::pow(orbit.semiMajorAxisKm() / earthEquatorialRadiusKm, 2.0) *
                      std::sqrt(1.0 - orbit.eccentricity() * orbit.eccentricity()) *
                      std::pow(1.0 + orbit.eccentricity(), 2.0));
    const double b = 3.0 * earthJ2 * std::pow(1.0 + orbit.eccentricity(), 3.0) /
                     (2.0 * std::pow(orbit.semiMajorAxisKm() / earthEquatorialRadiusKm, 2.0) *
                      std::pow(1.0 - orbit.eccentricity() * orbit.eccentricity(), 3.0));
    const double factor = 1.0 - a - b;
    return 2.0 * pi * factor / meanMotion;
}

GeographicPoint groundTrackWithNodalPrecession(const OrbitConfiguration& orbit,
                                                double argumentOfPeriapsisDegrees,
                                                double meanAnomalyAtEpochDegrees,
                                                double timeSeconds) noexcept {
    const double inclination = orbit.inclinationDegrees() * degreesToRadians;
    const double ascendingNodeAtEpoch = orbit.ascendingNodeLongitudeDegrees() * degreesToRadians;
    const double argumentOfPeriapsis = argumentOfPeriapsisDegrees * degreesToRadians;
    const double meanAnomalyAtEpoch = meanAnomalyAtEpochDegrees * degreesToRadians;
    const double meanMotion = std::sqrt(earthGravitationalParameterKm3PerS2 /
                                        std::pow(orbit.semiMajorAxisKm(), 3.0));
    const double semiLatusRectum = orbit.semiMajorAxisKm() *
                                   (1.0 - orbit.eccentricity() * orbit.eccentricity());
    const double ascendingNodeRate = -1.5 * groundTrackJ2 *
                                     std::pow(groundTrackEarthRadiusKm / semiLatusRectum, 2.0) *
                                     meanMotion * std::cos(inclination);
    const double meanAnomaly = meanAnomalyAtEpoch + meanMotion * timeSeconds;
    const double eccentricAnomalyValue = eccentricAnomaly(meanAnomaly, orbit.eccentricity());
    const double trueAnomaly = 2.0 * std::atan2(
        std::sqrt(1.0 + orbit.eccentricity()) * std::sin(eccentricAnomalyValue / 2.0),
        std::sqrt(1.0 - orbit.eccentricity()) * std::cos(eccentricAnomalyValue / 2.0));
    const double cosineTheta = std::cos(trueAnomaly + argumentOfPeriapsis);
    const double sineTheta = std::sin(trueAnomaly + argumentOfPeriapsis);
    const double ascendingNode = ascendingNodeAtEpoch +
                                 (-earthRotationRadiansPerSecond + ascendingNodeRate) * timeSeconds;
    const double cosineNode = std::cos(ascendingNode);
    const double sineNode = std::sin(ascendingNode);
    const double x = cosineTheta * cosineNode - std::cos(inclination) * sineTheta * sineNode;
    const double y = cosineTheta * sineNode + std::cos(inclination) * sineTheta * cosineNode;
    const double z = sineTheta * std::sin(inclination);
    const double longitude = std::atan2(y, x) * radiansToDegrees;
    const double latitude = std::atan2(z, std::sqrt(x * x + y * y)) * radiansToDegrees;
    return {latitude, wrapLongitudeDegrees(longitude)};
}

GeographicPoint circularOrbitGroundTrack(double semiMajorAxisKm,
                                          double inclinationRadians,
                                          double meanAnomalyAtEpochRadians,
                                          double timeSeconds) noexcept {
    const double period = orbitalPeriodSeconds(semiMajorAxisKm);
    const double trueAnomaly = meanAnomalyAtEpochRadians + 2.0 * pi * timeSeconds / period;
    const double xEci = semiMajorAxisKm * std::cos(trueAnomaly);
    const double yEci = semiMajorAxisKm * std::cos(inclinationRadians) * std::sin(trueAnomaly);
    const double zEci = semiMajorAxisKm * std::sin(inclinationRadians) * std::sin(trueAnomaly);
    const double earthAngle = earthRotationRadiansPerSecond * timeSeconds;
    const double xEcef = std::cos(earthAngle) * xEci + std::sin(earthAngle) * yEci;
    const double yEcef = -std::sin(earthAngle) * xEci + std::cos(earthAngle) * yEci;
    const double norm = std::sqrt(xEcef * xEcef + yEcef * yEcef + zEci * zEci);
    return {std::asin(zEci / norm) * radiansToDegrees,
            std::atan2(yEcef, xEcef) * radiansToDegrees};
}

GeographicPoint missionGroundTrackPoint(const ResolvedMission& mission,
                                         double missionTimeSeconds) noexcept {
    const double shiftedTime = missionTimeSeconds - mission.orbitalPeriodSeconds() / 4.0;
    return groundTrackWithNodalPrecession(mission.orbit(), 0.0, 0.0, shiftedTime);
}

TransverseMercator::TransverseMercator(double centralMeridianDegrees, double scale)
    : centralMeridianDegrees_(centralMeridianDegrees), scale_(scale) {
    if (!std::isfinite(centralMeridianDegrees_)) {
        throw std::invalid_argument("central meridian must be finite");
    }
    if (!isFinitePositive(scale_)) {
        throw std::invalid_argument("projection scale must be finite and positive");
    }
}

double TransverseMercator::centralMeridianDegrees() const noexcept { return centralMeridianDegrees_; }
double TransverseMercator::scale() const noexcept { return scale_; }

ProjectedPoint TransverseMercator::forward(GeographicPoint point) const noexcept {
    const double latitude = point.latitudeDegrees * degreesToRadians;
    const double sine = std::sin(latitude);
    const double cosine = std::cos(latitude);
    const double tangent = std::tan(latitude);
    const double sineSquared = sine * sine;
    const double cosineSquared = cosine * cosine;
    const double tangentSquared = tangent * tangent;
    const double flattening = 1.0 / wgs84InverseFlattening;
    double eccentricitySquared = flattening * (2.0 - flattening);
    const double radius = wgs84SemiMajorAxisMeters /
                          std::sqrt(1.0 - eccentricitySquared * sineSquared);
    eccentricitySquared /= 1.0 - eccentricitySquared;
    const double ePrimeCosineSquared = eccentricitySquared * cosineSquared;
    const double deltaLongitude = wrapLongitudeDegrees(point.longitudeDegrees - centralMeridianDegrees_) * degreesToRadians;
    const double deltaSquaredCosineSquared = deltaLongitude * deltaLongitude * cosineSquared;

    const double t7 = 1.0 - tangentSquared + ePrimeCosineSquared;
    const double t8 = 5.0 + tangentSquared * (tangentSquared - 18.0) +
                      ePrimeCosineSquared * (14.0 - 58.0 * tangentSquared);
    const double t9 = 61.0 - tangentSquared * (479.0 - tangentSquared * (179.0 - tangentSquared));
    const double x = scale_ * radius * cosine * deltaLongitude *
                     (1.0 + (deltaSquaredCosineSquared / 6.0) *
                      (t7 + (deltaSquaredCosineSquared / 20.0) *
                       (t8 + (deltaSquaredCosineSquared / 42.0) * t9)));

    const double t3 = 5.0 - tangentSquared + ePrimeCosineSquared * (9.0 + 4.0 * ePrimeCosineSquared);
    const double t4 = 61.0 - tangentSquared * (58.0 - tangentSquared) +
                      270.0 * ePrimeCosineSquared - 330.0 * tangentSquared * ePrimeCosineSquared;
    const double t5 = 1385.0 - tangentSquared * (3111.0 - tangentSquared * (543.0 - tangentSquared));
    const double y = scale_ * meridionalArc(latitude) +
                     (scale_ * radius * sine * cosine / 2.0) * deltaLongitude * deltaLongitude *
                     (1.0 + (deltaSquaredCosineSquared / 12.0) *
                      (t3 + (deltaSquaredCosineSquared / 30.0) *
                       (t4 + (deltaSquaredCosineSquared / 56.0) * t5)));
    return {x, y};
}

GeographicPoint TransverseMercator::inverse(ProjectedPoint point) const noexcept {
    double latitude = inverseMeridionalArc(point.yMeters / scale_);
    if (std::abs(latitude) >= 2.0 * pi) {
        return {point.yMeters < 0.0 ? -90.0 : 90.0, 0.0};
    }

    const double sine = std::sin(latitude);
    const double cosine = std::cos(latitude);
    const double tangent = std::tan(latitude);
    const double sineSquared = sine * sine;
    const double cosineSquared = cosine * cosine;
    const double tangentSquared = tangent * tangent;
    const double flattening = 1.0 / wgs84InverseFlattening;
    double eccentricitySquared = flattening * (2.0 - flattening);
    const double radius = wgs84SemiMajorAxisMeters /
                          std::sqrt(1.0 - eccentricitySquared * sineSquared);
    eccentricitySquared /= 1.0 - eccentricitySquared;
    const double ePrimeCosineSquared = eccentricitySquared * cosineSquared;
    const double t11 = 5.0 + 3.0 * tangentSquared +
                       ePrimeCosineSquared * (1.0 - 4.0 * ePrimeCosineSquared - 9.0 * tangentSquared);
    const double t12 = 61.0 + tangentSquared *
                       (90.0 - 25.0 * ePrimeCosineSquared + 45.0 * tangentSquared) +
                       46.0 * ePrimeCosineSquared;
    const double t13 = 1385.0 + tangentSquared *
                       (3633.0 + tangentSquared * (4095.0 + tangentSquared * 1575.0));
    const double d = point.xMeters / (radius * scale_);
    const double dSquared = d * d;
    latitude -= 0.5 * (1.0 + ePrimeCosineSquared) * tangent * dSquared *
                (1.0 - (dSquared / 12.0) *
                 (t11 - (dSquared / 30.0) *
                  (t12 - (dSquared / 56.0) * t13)));

    const double t15 = 1.0 + 2.0 * tangentSquared + ePrimeCosineSquared;
    const double t16 = 5.0 + tangentSquared *
                       (28.0 + 24.0 * tangentSquared + 8.0 * ePrimeCosineSquared) +
                       6.0 * ePrimeCosineSquared;
    const double t17 = 61.0 + tangentSquared *
                       (662.0 + tangentSquared * (1320.0 + 720.0 * tangentSquared));
    const double longitude = centralMeridianDegrees_ + radiansToDegrees * (d / cosine) *
                             (1.0 - (dSquared / 6.0) *
                              (t15 - (dSquared / 20.0) *
                               (t16 - (dSquared / 42.0) * t17)));
    return {latitude * radiansToDegrees, wrapLongitudeDegrees(longitude)};
}

ProjectionWindow::ProjectionWindow(double widthMeters,
                                   double heightMeters,
                                   double centerLatitudeDegrees,
                                   double centerLongitudeDegrees,
                                   WindowOrigin origin)
    : widthMeters_(widthMeters),
      heightMeters_(heightMeters),
      centerLatitudeDegrees_(centerLatitudeDegrees),
      centerLongitudeDegrees_(centerLongitudeDegrees),
      origin_(origin) {
    if (!isFinitePositive(widthMeters_) || !isFinitePositive(heightMeters_)) {
        throw std::invalid_argument("projection-window dimensions must be finite and positive");
    }
    if (!std::isfinite(centerLatitudeDegrees_) || !std::isfinite(centerLongitudeDegrees_)) {
        throw std::invalid_argument("projection-window center must be finite");
    }
}

double ProjectionWindow::widthMeters() const noexcept { return widthMeters_; }
double ProjectionWindow::heightMeters() const noexcept { return heightMeters_; }
double ProjectionWindow::centerLatitudeDegrees() const noexcept { return centerLatitudeDegrees_; }
double ProjectionWindow::centerLongitudeDegrees() const noexcept { return centerLongitudeDegrees_; }
WindowOrigin ProjectionWindow::origin() const noexcept { return origin_; }

std::optional<ProjectedPoint> projectIntoWindow(const ProjectionWindow& window,
                                                GeographicPoint point) {
    const TransverseMercator projection(window.centerLongitudeDegrees());
    const ProjectedPoint projectedCenter = projection.forward(
        {window.centerLatitudeDegrees(), window.centerLongitudeDegrees()});
    const GeographicBounds geographicBounds =
        geographicBoundsForWindow(window, projection, projectedCenter);
    if (!geographicBounds.contains(point)) {
        return std::nullopt;
    }
    const ProjectedPoint projected = projection.forward(point);
    const double halfWidth = window.widthMeters() / 2.0;
    const double halfHeight = window.heightMeters() / 2.0;
    double x = projected.xMeters - projectedCenter.xMeters;
    double y = projected.yMeters - projectedCenter.yMeters;
    if (x < -halfWidth || x > halfWidth || y < -halfHeight || y > halfHeight) {
        return std::nullopt;
    }
    if (window.origin() == WindowOrigin::lowerLeft) {
        x += halfWidth;
        y += halfHeight;
    }
    return ProjectedPoint{x, y};
}

std::vector<TimedProjectedPoint> projectedMissionWindow(const ResolvedMission& mission,
                                                        const ProjectionWindow& window,
                                                        double initialTimeSeconds,
                                                        double finalTimeSeconds,
                                                        double sampleIntervalSeconds) {
    if (!std::isfinite(initialTimeSeconds) || !std::isfinite(finalTimeSeconds) ||
        finalTimeSeconds < initialTimeSeconds) {
        throw std::invalid_argument("requested time window must be finite and ordered");
    }
    if (!isFinitePositive(sampleIntervalSeconds)) {
        throw std::invalid_argument("sample interval must be finite and positive");
    }

    const TransverseMercator projection(window.centerLongitudeDegrees());
    const ProjectedPoint projectedCenter = projection.forward(
        {window.centerLatitudeDegrees(), window.centerLongitudeDegrees()});
    const GeographicBounds geographicBounds =
        geographicBoundsForWindow(window, projection, projectedCenter);
    const double halfWidth = window.widthMeters() / 2.0;
    const double halfHeight = window.heightMeters() / 2.0;
    const double span = finalTimeSeconds - initialTimeSeconds;
    const auto intervalCount = static_cast<std::size_t>(std::floor(span / sampleIntervalSeconds));
    std::vector<TimedProjectedPoint> points;

    for (std::size_t index = 0; index <= intervalCount; ++index) {
        const double time = initialTimeSeconds + static_cast<double>(index) * sampleIntervalSeconds;
        const GeographicPoint geographic = missionGroundTrackPoint(mission, time);
        if (!geographicBounds.contains(geographic)) {
            continue;
        }
        const ProjectedPoint projected = projection.forward(geographic);
        double x = projected.xMeters - projectedCenter.xMeters;
        double y = projected.yMeters - projectedCenter.yMeters;
        if (x < -halfWidth || x > halfWidth || y < -halfHeight || y > halfHeight) {
            continue;
        }
        if (window.origin() == WindowOrigin::lowerLeft) {
            x += halfWidth;
            y += halfHeight;
        }
        points.push_back({x, y, time});
    }
    return points;
}

std::vector<Pass> segmentPasses(const std::vector<TimedProjectedPoint>& track,
                                double maximumContinuousGapSeconds) {
    if (!isFinitePositive(maximumContinuousGapSeconds)) {
        throw std::invalid_argument("maximum continuous gap must be finite and positive");
    }
    if (track.empty()) {
        return {};
    }
    std::vector<Pass> passes;
    passes.emplace_back();
    passes.back().push_back(track.front());
    for (std::size_t index = 1; index < track.size(); ++index) {
        if (track[index].timeSeconds - track[index - 1].timeSeconds > maximumContinuousGapSeconds) {
            passes.emplace_back();
        }
        passes.back().push_back(track[index]);
    }
    return passes;
}

std::vector<double> passTriggerTimes(const std::vector<Pass>& passes) {
    std::vector<double> times;
    times.reserve(passes.size());
    for (const Pass& pass : passes) {
        if (pass.empty()) {
            throw std::invalid_argument("pass list contains an empty pass");
        }
        times.push_back(pass.front().timeSeconds);
    }
    return times;
}

} // namespace alongtrack
