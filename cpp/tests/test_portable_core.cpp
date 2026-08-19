#include "alongtrack/portable_core.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <sstream>
#include <type_traits>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void checkClose(double actual,
                double expected,
                double absoluteTolerance,
                double relativeTolerance,
                const std::string& message) {
    const double tolerance = absoluteTolerance + relativeTolerance * std::abs(expected);
    if (!(std::abs(actual - expected) <= tolerance)) {
        ++failures;
        std::cerr.precision(17);
        std::cerr << "FAIL: " << message << " expected " << expected
                  << ", got " << actual << ", tolerance " << tolerance << '\n';
    }
}

void checkParity(double actual,
                 double expected,
                 double absoluteCeiling,
                 const std::string& message) {
    const double difference = std::abs(actual - expected);
    const double normalizedDifference = difference / std::max(1.0, std::abs(expected));
    if (!(normalizedDifference <= 1.0e-12 && difference <= absoluteCeiling)) {
        ++failures;
        std::cerr.precision(17);
        std::cerr << "FAIL: " << message << " expected " << expected
                  << ", got " << actual << ", normalized difference "
                  << normalizedDifference << ", absolute difference " << difference
                  << '\n';
    }
}

struct ReferenceRow final {
    std::string kind;
    std::string name;
    double input0;
    double input1;
    double input2;
    double expected0;
    double expected1;
    double expected2;
    double expected3;
};

std::vector<ReferenceRow> matlabReferenceRows() {
    std::ifstream input(ATS_MATLAB_REFERENCE_PATH);
    if (!input) {
        throw std::runtime_error("unable to open MATLAB parity fixture");
    }
    std::string line;
    std::getline(input, line);
    std::vector<ReferenceRow> rows;
    while (std::getline(input, line)) {
        std::stringstream stream(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(stream, field, ',')) {
            fields.push_back(field);
        }
        if (fields.size() != 9) {
            throw std::runtime_error("invalid MATLAB parity fixture row");
        }
        rows.push_back({fields[0], fields[1], std::stod(fields[2]), std::stod(fields[3]),
                        std::stod(fields[4]), std::stod(fields[5]), std::stod(fields[6]),
                        std::stod(fields[7]), std::stod(fields[8])});
    }
    return rows;
}

void testMatlabParityFixture() {
    const auto rows = matlabReferenceRows();
    check(rows.size() == 80, "all MATLAB reference cases were loaded");
    for (const auto& row : rows) {
        if (row.kind == "mission") {
            const auto mission = alongtrack::ResolvedMission::fromCatalog(row.name);
            checkClose(mission.orbit().semiMajorAxisKm(), row.expected0, 1.0e-12, 0.0,
                       row.name + " MATLAB resolved semi-major axis");
            checkClose(mission.orbitalPeriodSeconds(), row.expected1, 1.0e-12, 1.0e-15,
                       row.name + " MATLAB orbital period");
            checkClose(mission.nodalPeriodSeconds(), row.expected2, 1.0e-12, 1.0e-15,
                       row.name + " MATLAB nodal period");
            if (std::isinf(row.expected3)) {
                check(std::isinf(mission.repeatCycleSeconds()),
                      row.name + " MATLAB non-repeating configuration");
            } else {
                checkParity(mission.repeatCycleSeconds(), row.expected3, 1.0e-9,
                            row.name + " MATLAB repeat period");
            }
        } else if (row.kind == "orbit") {
            const auto mission = alongtrack::ResolvedMission::fromCatalog(row.name);
            checkClose(mission.orbit().eccentricity(), row.expected0, 0.0, 0.0,
                       row.name + " MATLAB eccentricity");
            checkClose(mission.orbit().inclinationDegrees(), row.expected1, 0.0, 0.0,
                       row.name + " MATLAB inclination");
            checkClose(mission.orbit().ascendingNodeLongitudeDegrees(), row.expected2, 0.0, 0.0,
                       row.name + " MATLAB ascending-node longitude");
            if (std::isinf(row.expected3)) {
                check(std::isinf(mission.passesPerCycle()),
                      row.name + " MATLAB non-repeating pass count");
            } else {
                checkClose(mission.passesPerCycle(), row.expected3, 0.0, 0.0,
                           row.name + " MATLAB passes per cycle");
            }
        } else if (row.kind == "ground") {
            const auto mission = alongtrack::ResolvedMission::fromCatalog(row.name);
            const auto point = alongtrack::missionGroundTrackPoint(mission, row.input0);
            checkClose(point.latitudeDegrees, row.expected0, 1.0e-12, 1.0e-14,
                       row.name + " MATLAB ground-track latitude");
            checkClose(point.longitudeDegrees, row.expected1, 1.0e-12, 1.0e-14,
                       row.name + " MATLAB ground-track longitude");
        } else if (row.kind == "circular") {
            const auto point = alongtrack::circularOrbitGroundTrack(
                row.input1, row.input2, 0.2, row.input0);
            checkClose(point.latitudeDegrees, row.expected0, 1.0e-12, 1.0e-14,
                       "MATLAB circular-orbit latitude");
            checkClose(point.longitudeDegrees, row.expected1, 1.0e-12, 1.0e-14,
                       "MATLAB circular-orbit longitude");
        } else if (row.kind == "projection") {
            const alongtrack::TransverseMercator projection(row.input2);
            const auto point = projection.forward({row.input0, row.input1});
            const auto inverse = projection.inverse(point);
            checkParity(point.xMeters, row.expected0, 1.0e-9,
                        "MATLAB projected x");
            checkParity(point.yMeters, row.expected1, 1.0e-9,
                        "MATLAB projected y");
            checkParity(inverse.latitudeDegrees, row.expected2, 1.0e-12,
                        "MATLAB inverse latitude");
            checkParity(inverse.longitudeDegrees, row.expected3, 1.0e-12,
                        "MATLAB inverse longitude");
        } else if (row.kind == "window") {
            const auto mission = alongtrack::ResolvedMission::fromCatalog(row.name);
            const alongtrack::ProjectionWindow window(3000000.0, 3000000.0, 0.0, 0.0);
            const auto track = alongtrack::projectedMissionWindow(
                mission, window, row.input0, row.input1);
            const auto passes = alongtrack::segmentPasses(track);
            const auto triggers = alongtrack::passTriggerTimes(passes);
            check(track.size() == static_cast<std::size_t>(row.expected0),
                  row.name + " MATLAB projected-window sample count");
            check(passes.size() == static_cast<std::size_t>(row.expected1),
                  row.name + " MATLAB pass count");
            checkClose(triggers.front(), row.expected2, 0.0, 0.0,
                       row.name + " MATLAB first trigger");
            if (std::isfinite(row.expected3)) {
                checkClose(triggers.back(), row.expected3, 0.0, 0.0,
                           row.name + " MATLAB last trigger");
            }
        } else if (row.kind == "window_first" || row.kind == "window_last") {
            const auto mission = alongtrack::ResolvedMission::fromCatalog(row.name);
            const alongtrack::ProjectionWindow window(3000000.0, 3000000.0, 0.0, 0.0);
            const auto track = alongtrack::projectedMissionWindow(mission, window, 0.0, 30000.0);
            const auto& point = row.kind == "window_first" ? track.front() : track.back();
            checkClose(point.timeSeconds, row.input0, 0.0, 0.0,
                       row.name + " MATLAB projected endpoint time");
            checkParity(point.xMeters, row.expected0, 1.0e-9,
                        row.name + " MATLAB projected endpoint x");
            checkParity(point.yMeters, row.expected1, 1.0e-9,
                        row.name + " MATLAB projected endpoint y");
        } else if (row.kind == "pass") {
            const auto mission = alongtrack::ResolvedMission::fromCatalog(row.name);
            const alongtrack::ProjectionWindow window(3000000.0, 3000000.0, 0.0, 0.0);
            const auto track = alongtrack::projectedMissionWindow(mission, window, 0.0, 30000.0);
            const auto passes = alongtrack::segmentPasses(track);
            const auto passIndex = static_cast<std::size_t>(row.input0) - 1;
            check(passIndex < passes.size(), row.name + " MATLAB pass index");
            if (passIndex < passes.size()) {
                const auto& pass = passes[passIndex];
                check(pass.size() == static_cast<std::size_t>(row.input1),
                      row.name + " MATLAB pass sample count");
                checkClose(pass.front().timeSeconds, row.expected0, 0.0, 0.0,
                           row.name + " MATLAB pass start");
                checkClose(pass.back().timeSeconds, row.expected1, 0.0, 0.0,
                           row.name + " MATLAB pass end");
                checkParity(pass.back().xMeters, row.expected2, 1.0e-9,
                            row.name + " MATLAB pass-end x");
                checkParity(pass.back().yMeters, row.expected3, 1.0e-9,
                            row.name + " MATLAB pass-end y");
            }
        } else if (row.kind == "dateline") {
            const auto mission = alongtrack::ResolvedMission::fromCatalog(row.name);
            const alongtrack::ProjectionWindow window(100000.0, 100000.0, row.input2, 179.9);
            const auto track = alongtrack::projectedMissionWindow(
                mission, window, row.input0, row.input1);
            check(track.size() == static_cast<std::size_t>(row.expected0),
                  "MATLAB antimeridian window sample count");
            checkClose(track.front().timeSeconds, row.expected1, 0.0, 0.0,
                       "MATLAB antimeridian window start");
            checkClose(track.back().timeSeconds, row.expected2, 0.0, 0.0,
                       "MATLAB antimeridian window end");
            std::size_t negativeLongitudeCount = 0;
            for (const auto& point : track) {
                if (alongtrack::missionGroundTrackPoint(mission, point.timeSeconds).longitudeDegrees < 0.0) {
                    ++negativeLongitudeCount;
                }
            }
            check(negativeLongitudeCount == static_cast<std::size_t>(row.expected3),
                  "MATLAB antimeridian window negative-longitude count");
            check(negativeLongitudeCount > 0 && negativeLongitudeCount < track.size(),
                  "antimeridian window retains samples on both longitude signs");
        } else {
            check(false, "unknown MATLAB parity fixture kind: " + row.kind);
        }
    }
}

void testResolvedMissionConfiguration() {
    const auto j3 = alongtrack::ResolvedMission::fromCatalog("j3");
    check(j3.isRepeating(), "Jason-3 is repeating");
    checkClose(j3.orbit().semiMajorAxisKm(), 7716.3604974707214, 1.0e-12, 0.0,
               "Jason-3 resolved semi-major axis matches MATLAB");
    checkClose(j3.orbitalPeriodSeconds(), 6745.7422657375528, 1.0e-12, 1.0e-15,
               "Jason-3 orbital period matches MATLAB");
    checkClose(j3.nodalPeriodSeconds(), 6738.9117344768665, 1.0e-12, 1.0e-15,
               "Jason-3 nodal period matches MATLAB");
    checkParity(j3.repeatCycleSeconds(), 9.9156165248688577 * 86400.0, 1.0e-9,
                "Jason-3 repeat cycle matches MATLAB");

    const auto geodetic = alongtrack::ResolvedMission::fromCatalog("alg");
    check(!geodetic.isRepeating(), "AltiKa geodetic mission is non-repeating");
    check(std::isinf(geodetic.repeatCycleSeconds()), "geodetic repeat cycle is infinity");
    checkClose(geodetic.nodalPeriodSeconds(), 7117.6624828536442, 1.0e-12, 1.0e-15,
               "geodetic nodal period matches MATLAB");

    check(alongtrack::ResolvedMission::catalogKeys().size() == 26,
          "all MATLAB numerical mission entries are available");
    for (const std::string_view key : alongtrack::ResolvedMission::catalogKeys()) {
        const auto mission = alongtrack::ResolvedMission::fromCatalog(key);
        check(mission.key() == key, "catalog key resolves to an owned immutable mission");
        check(mission.orbit().semiMajorAxisKm() > 0.0,
              "every MATLAB catalog entry has a resolved semi-major axis");
        check(mission.nodalPeriodSeconds() > 0.0,
              "every MATLAB catalog entry has a resolved nodal period");
        check(mission.isRepeating() == std::isfinite(mission.passesPerCycle()),
              "every catalog repeat flag is resolved from numeric configuration");
    }
}

void testResolvedMissionIsIndependentOfSourceDefinition() {
    alongtrack::MissionDefinition source{"custom", 7000.0, 7001.0, 0.001, 98.0, 12.0, 200.0};
    const alongtrack::ResolvedMission resolved(source);
    source.resolvedSemiMajorAxisKm = 9000.0;
    source.eccentricity = 0.5;
    source.passesPerCycle = std::numeric_limits<double>::infinity();
    checkClose(resolved.orbit().semiMajorAxisKm(), 7001.0, 0.0, 0.0,
               "resolved configuration owns its semi-major axis");
    checkClose(resolved.orbit().eccentricity(), 0.001, 0.0, 0.0,
               "resolved configuration owns its eccentricity");
    check(resolved.isRepeating(), "resolved repeat behavior cannot be changed through catalog source");
    check(!std::is_copy_assignable_v<alongtrack::ResolvedMission>,
          "resolved mission is not mutable by assignment");
}

void testPassSegmentationAndEmptyWindows() {
    const std::vector<alongtrack::TimedProjectedPoint> flat{
        {1.0, 2.0, 10.0},
        {2.0, 3.0, 11.0},
        {4.0, 5.0, 14.0},
        {5.0, 6.0, 15.0},
    };
    const auto passes = alongtrack::segmentPasses(flat);
    check(passes.size() == 2, "time gaps split passes");
    check(passes[0].size() == 2 && passes[1].size() == 2, "pass samples are preserved");
    const auto triggers = alongtrack::passTriggerTimes(passes);
    check(triggers == std::vector<double>({10.0, 14.0}), "trigger times are pass starts");
    check(alongtrack::segmentPasses({}).empty(), "empty track produces no passes");

    const auto mission = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow emptyWindow(1000.0, 1000.0, 0.0, 0.0);
    const auto emptyTrack = alongtrack::projectedMissionWindow(mission, emptyWindow, 0.0, 600.0);
    check(emptyTrack.empty(), "empty geographic window produces no retained samples");
}

void testProjectionRoundTripAndLongitudeWrapping() {
    const alongtrack::TransverseMercator projection(-123.0);
    const alongtrack::GeographicPoint source{24.0, -122.75};
    const auto projected = projection.forward(source);
    const auto inverse = projection.inverse(projected);
    checkClose(inverse.latitudeDegrees, source.latitudeDegrees, 1.0e-10, 1.0e-12,
               "projection latitude round trip");
    checkClose(inverse.longitudeDegrees, source.longitudeDegrees, 1.0e-10, 1.0e-12,
               "projection longitude round trip");

    const alongtrack::TransverseMercator datelineProjection(179.9);
    const auto east = datelineProjection.forward({0.0, -179.9});
    check(std::abs(east.xMeters) < 30000.0,
          "longitude is wrapped relative to central meridian at the dateline");
    const auto eastInverse = datelineProjection.inverse(east);
    checkClose(eastInverse.longitudeDegrees, -179.9, 1.0e-10, 1.0e-12,
               "dateline projection inverse preserves wrapped longitude");

    const alongtrack::ProjectionWindow boundaryWindow(
        2000000.0, 1000000.0, 24.0, -123.0, alongtrack::WindowOrigin::centered);
    const auto center = projection.forward({24.0, -123.0});
    const auto boundaryGeographic = projection.inverse(
        {center.xMeters + boundaryWindow.widthMeters() / 2.0, center.yMeters});
    const auto boundary = alongtrack::projectIntoWindow(boundaryWindow, boundaryGeographic);
    check(boundary.has_value(), "a projection-boundary point is included");
    const auto outsideGeographic = projection.inverse(
        {center.xMeters + boundaryWindow.widthMeters() / 2.0 + 1.0, center.yMeters});
    check(!alongtrack::projectIntoWindow(boundaryWindow, outsideGeographic).has_value(),
          "a point beyond the projection boundary is excluded");
}

void testWindowSamplingIsBoundedAndCoversMissionKinds() {
    const auto repeating = alongtrack::ResolvedMission::fromCatalog("j3");
    const auto geodetic = alongtrack::ResolvedMission::fromCatalog("alg");
    const alongtrack::ProjectionWindow globalWindow(30000000.0, 18000000.0, 0.0, 0.0,
                                                    alongtrack::WindowOrigin::centered);
    const auto repeatTrack = alongtrack::projectedMissionWindow(repeating, globalWindow, 0.0, 12000.0, 1.0);
    const auto geodeticTrack = alongtrack::projectedMissionWindow(geodetic, globalWindow, 0.0, 12000.0, 1.0);
    check(!repeatTrack.empty(), "repeating mission window is supported");
    check(!geodeticTrack.empty(), "non-repeating geodetic mission window is supported");
    check(repeatTrack.capacity() <= 12001, "retained storage is no larger than requested working window");
    check(geodeticTrack.capacity() <= 12001, "geodetic retained storage is bounded by working window");
    check(sizeof(alongtrack::ResolvedMission) < 256,
          "mission configuration does not contain a future trajectory");

    const alongtrack::ProjectionWindow smallWindow(1000.0, 1000.0, 0.0, 0.0);
    constexpr double longWindowSeconds = 180.0 * 86400.0;
    constexpr double longWindowIntervalSeconds = 60.0;
    const auto longTrack = alongtrack::projectedMissionWindow(
        geodetic, smallWindow, 0.0, longWindowSeconds, longWindowIntervalSeconds);
    const auto requestedSampleCount = static_cast<std::size_t>(
        longWindowSeconds / longWindowIntervalSeconds) + 1;
    check(requestedSampleCount == 259201,
          "long-window probe evaluates the complete requested 180-day window");
    check(longTrack.empty() && longTrack.capacity() == 0,
          "an empty long window retains no future trajectory samples");
    check(longTrack.capacity() <= requestedSampleCount,
          "long geodetic requests retain only points in the requested working window");
}

void testValidation() {
    bool unknownRejected = false;
    try {
        (void)alongtrack::ResolvedMission::fromCatalog("missing");
    } catch (const std::invalid_argument&) {
        unknownRejected = true;
    }
    check(unknownRejected, "unknown mission is rejected during resolution");

    bool invalidWindowRejected = false;
    try {
        (void)alongtrack::ProjectionWindow(-1.0, 1.0, 0.0, 0.0);
    } catch (const std::invalid_argument&) {
        invalidWindowRejected = true;
    }
    check(invalidWindowRejected, "invalid projection window is rejected");
}

} // namespace

int main() {
    testMatlabParityFixture();
    testResolvedMissionConfiguration();
    testResolvedMissionIsIndependentOfSourceDefinition();
    testPassSegmentationAndEmptyWindows();
    testProjectionRoundTripAndLongitudeWrapping();
    testWindowSamplingIsBoundedAndCoversMissionKinds();
    testValidation();
    if (failures != 0) {
        std::cerr << failures << " portable-core checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All portable-core checks passed\n";
    return EXIT_SUCCESS;
}
