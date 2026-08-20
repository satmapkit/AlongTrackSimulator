#include "alongtrack/wavevortex_extension.hpp"

#include "WaveVortexRuntime/WVExtensionCatalog.hpp"
#include "WaveVortexRuntime/WVIntegrationState.hpp"
#include "WaveVortexRuntime/WVModelOutputNetCDF.hpp"
#include "WaveVortexRuntime/WVOutputOrchestration.hpp"
#include "WaveVortexRuntime/WVOutputSchedule.hpp"

#include <nlohmann/json.hpp>
#include <netcdf.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

#ifndef ATS4_BASE_RUNNER
#error "ATS4_BASE_RUNNER must identify wave-vortex-run"
#endif

#ifndef ATS4_EXTENDED_RUNNER
#error "ATS4_EXTENDED_RUNNER must identify alongtrack-wave-vortex-run"
#endif

#ifndef ATS4_REPEATING_REQUEST
#error "ATS4_REPEATING_REQUEST must identify the repeating request fixture"
#endif

#ifndef ATS4_GEODETIC_REQUEST
#error "ATS4_GEODETIC_REQUEST must identify the geodetic request fixture"
#endif

namespace {

namespace atwv = alongtrack::wavevortex_extension;
namespace runtime = wavevortex::runtime;
using json = nlohmann::json;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireStatus(const wavevortex::WVKernelStatus& status,
                   const std::string& context) {
    require(static_cast<bool>(status), context + ": " + status.message);
}

std::vector<char> bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "unable to read " + path.string());
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

std::string text(const std::filesystem::path& path) {
    const auto data = bytes(path);
    return {data.begin(), data.end()};
}

json readJSON(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input),
            "unable to read JSON fixture " + path.string());
    return json::parse(input);
}

void writeJSON(const std::filesystem::path& path, const json& value) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output),
            "unable to write request " + path.string());
    output << value.dump(2) << '\n';
    require(static_cast<bool>(output),
            "unable to finish request " + path.string());
}

std::string shellQuote(const std::filesystem::path& path) {
#if defined(_WIN32)
    std::string result = "\"";
    for (const char character : path.string()) {
        if (character == '\"') {
            result += "\\\"";
        } else {
            result += character;
        }
    }
    result += '\"';
#else
    std::string result = "'";
    for (const char character : path.string()) {
        if (character == '\'') {
            result += "'\"'\"'";
        } else {
            result += character;
        }
    }
    result += '\'';
#endif
    return result;
}

int run(const std::filesystem::path& executable,
        const std::filesystem::path& request,
        const std::filesystem::path& standardOutput,
        const std::filesystem::path& standardError) {
    const auto command = shellQuote(executable) + " --request " +
                         shellQuote(request) + " >" +
                         shellQuote(standardOutput) + " 2>" +
                         shellQuote(standardError);
    const int status = std::system(command.c_str());
#if defined(_WIN32)
    return status;
#else
    if (status == -1) {
        return status;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : status;
#endif
}

std::shared_ptr<const runtime::WVExtensionCatalog> extendedCatalog() {
    runtime::WVExtensionCatalogBuilder builder;
    auto status = runtime::addBuiltInExtensions(builder);
    if (status) {
        status = atwv::registerAlongTrackExtensions(builder);
    }
    std::shared_ptr<const runtime::WVExtensionCatalog> catalog;
    if (status) {
        status = builder.freeze(catalog);
    }
    requireStatus(status, "source-linked runner test catalog");
    require(catalog != nullptr, "source-linked runner test catalog is null");
    return catalog;
}

struct PreparedRequest final {
    json document;
    std::filesystem::path path;
    std::filesystem::path report;
    std::vector<std::filesystem::path> originalSources;
    std::vector<std::filesystem::path> copiedSources;
    std::vector<std::vector<char>> originalSourceBytes;
    std::vector<std::vector<char>> copiedSourceBytes;
    std::vector<std::filesystem::path> destinations;
};

std::filesystem::path resolveFixturePath(
    const std::filesystem::path& requestPath,
    const std::string& value) {
    const std::filesystem::path raw(value);
    return raw.is_absolute() ? raw : requestPath.parent_path() / raw;
}

PreparedRequest prepareRequest(const std::filesystem::path& templatePath,
                               const std::filesystem::path& directory,
                               const std::string& name) {
    PreparedRequest result;
    result.document = readJSON(templatePath);
    require(result.document.value("schemaIdentifier", std::string{}) ==
                    "wave-vortex-run-request-v1" &&
                result.document.value("schemaVersion", 0) == 1,
            name + " request has the wrong portable schema");
    require(result.document.at("output").at("policy") == "create",
            name + " request must be an authoritative create request");
    require(result.document.at("execution").at("fftProvider") ==
                    "reference" &&
                result.document.at("execution").at("threads") == 1,
            name + " request must use the serialized reference provider");

    auto& modelFiles = result.document.at("modelFiles");
    require(modelFiles.is_array() && !modelFiles.empty(),
            name + " request has no model files");
    for (std::size_t index = 0; index < modelFiles.size(); ++index) {
        const auto source = resolveFixturePath(
            templatePath, modelFiles.at(index).get<std::string>());
        require(std::filesystem::is_regular_file(source),
                name + " source fixture is missing: " + source.string());
        auto copy = directory /
                    (name + "-source-" + std::to_string(index + 1) +
                     source.extension().string());
        std::filesystem::copy_file(
            source, copy, std::filesystem::copy_options::overwrite_existing);
        result.originalSources.push_back(source);
        result.copiedSources.push_back(copy);
        result.originalSourceBytes.push_back(bytes(source));
        result.copiedSourceBytes.push_back(bytes(copy));
        modelFiles.at(index) = copy.string();
    }

    auto& destinations = result.document.at("output").at("destinations");
    require(destinations.is_object() && !destinations.empty(),
            name + " request has no complete destination map");
    for (auto& [identifier, value] : destinations.items()) {
        (void)value;
        const auto destination = directory / (name + "-" + identifier + ".nc");
        result.destinations.push_back(destination);
        destinations[identifier] = destination.string();
    }
    result.report = directory / (name + "-report.json");
    result.document["report"] = result.report.string();
    result.path = directory / (name + "-request.json");
    writeJSON(result.path, result.document);
    return result;
}

void requireSourcesUnchanged(const PreparedRequest& request,
                             const std::string& context) {
    for (std::size_t index = 0; index < request.originalSources.size();
         ++index) {
        require(bytes(request.originalSources[index]) ==
                    request.originalSourceBytes[index],
                context + " changed committed MATLAB fixture " +
                    request.originalSources[index].string());
        require(bytes(request.copiedSources[index]) ==
                    request.copiedSourceBytes[index],
                context + " changed source model " +
                    request.copiedSources[index].string());
    }
}

bool hasVariable(const runtime::WVObservationSchema& schema,
                 const std::string& identifier) {
    return std::any_of(
        schema.variables.begin(), schema.variables.end(),
        [&](const auto& variable) {
            return variable.identifier == identifier;
        });
}

struct AlongTrackProgress final {
    std::int64_t continuationOrdinal = runtime::WVNoCommittedOutputOrdinal;
    std::int64_t destinationOrdinal = runtime::WVNoCommittedOutputOrdinal;
    std::size_t recordCount = 0;
};

using ProgressMap = std::map<std::string, AlongTrackProgress>;

std::string progressKey(const std::string& file,
                        const std::string& group) {
    return file + "\n" + group;
}

ProgressMap requireAlongTrackPersistence(
    const runtime::WVModelOutputNetCDFInspection& inspection,
    const std::string& context) {
    std::vector<std::string> alongTrackObservers;
    for (const auto& observer : inspection.observerRecord.observers) {
        if (observer.typeIdentifier == atwv::observingSystemTypeIdentifier) {
            alongTrackObservers.push_back(observer.identifier);
            require(observer.contractVersion == atwv::contractVersion &&
                        !observer.configuration.values.empty(),
                    context + " lost typed AlongTrack observer configuration");
        }
    }
    require(!alongTrackObservers.empty(),
            context + " contains no AlongTrack observer");

    bool hasSsh = false;
    bool hasSsu = false;
    bool hasSsv = false;
    for (const auto& identifier : alongTrackObservers) {
        const auto schema = std::find_if(
            inspection.observationSchemas.begin(),
            inspection.observationSchemas.end(),
            [&](const auto& candidate) {
                return candidate.observerIdentifier == identifier;
            });
        require(schema != inspection.observationSchemas.end(),
                context + " lost the AlongTrack observation schema");
        for (const char* required :
             {"sample-time", "x", "y", "repeat-cycle-index",
              "first-sample-index", "sample-count"}) {
            require(hasVariable(schema->schema, required),
                    context + " schema is missing " + required);
        }
        hasSsh = hasSsh || hasVariable(schema->schema, "ssh");
        hasSsu = hasSsu || hasVariable(schema->schema, "ssu");
        hasSsv = hasSsv || hasVariable(schema->schema, "ssv");
    }
    require(hasSsh && hasSsu && hasSsv,
            context + " schemas do not cover ssh, ssu, and ssv");

    ProgressMap result;
    bool hasCommittedOccurrence = false;
    for (const auto& file : inspection.observerRecord.outputFiles) {
        for (const auto& group : file.groups) {
            if (group.schedule.typeIdentifier != atwv::scheduleTypeIdentifier) {
                continue;
            }
            require(group.schedule.contractVersion == atwv::contractVersion &&
                        !group.schedule.configuration.values.empty(),
                    context + " lost typed AlongTrack schedule configuration");
            const auto continuation = std::find_if(
                inspection.scheduleContinuations.begin(),
                inspection.scheduleContinuations.end(),
                [&](const auto& candidate) {
                    return candidate.fileIdentifier == file.identifier &&
                           candidate.groupIdentifier == group.identifier;
                });
            const auto destination = std::find_if(
                inspection.destinationProgress.begin(),
                inspection.destinationProgress.end(),
                [&](const auto& candidate) {
                    return candidate.fileIdentifier == file.identifier &&
                           candidate.groupIdentifier == group.identifier;
                });
            require(continuation != inspection.scheduleContinuations.end() &&
                        destination != inspection.destinationProgress.end(),
                    context +
                        " conflated schedule continuation and destination progress");
            const auto continuationOrdinal =
                continuation->cursor.committedOrdinal;
            const auto destinationOrdinal =
                destination->committedScheduleCursor.committedOrdinal;
            if (continuationOrdinal >= 0) {
                hasCommittedOccurrence = true;
                require(continuation->cursor.values.schemaIdentifier ==
                            atwv::scheduleCursorSchemaIdentifier &&
                            continuation->cursor.values.schemaVersion ==
                                atwv::contractVersion,
                        context +
                            " persisted an incompatible continuation cursor");
            }
            if (destinationOrdinal >= 0) {
                hasCommittedOccurrence = true;
                require(destination->committedScheduleCursor.values
                                    .schemaIdentifier ==
                                atwv::scheduleCursorSchemaIdentifier &&
                            destination->committedScheduleCursor.values
                                    .schemaVersion == atwv::contractVersion,
                        context +
                            " persisted an incompatible destination cursor");
            }
            result.emplace(
                progressKey(file.identifier, group.identifier),
                AlongTrackProgress{continuationOrdinal, destinationOrdinal,
                                   destination->recordCount});
        }
    }
    require(!result.empty() && hasCommittedOccurrence,
            context + " committed no AlongTrack occurrence");
    return result;
}

runtime::WVModelOutputNetCDFInspection inspect(
    const std::vector<std::filesystem::path>& paths,
    const std::shared_ptr<const runtime::WVExtensionCatalog>& catalog,
    const std::string& context) {
    std::vector<std::string> names;
    names.reserve(paths.size());
    for (const auto& path : paths) {
        names.push_back(path.string());
    }
    runtime::WVModelOutputNetCDFInspection result;
    const auto status = runtime::WVModelOutputNetCDFSink::inspect(
        names, *catalog, result);
    require(static_cast<bool>(status), context + ": " + status.message);
    return result;
}

double nextAlongTrackTime(
    const runtime::WVModelOutputNetCDFInspection& inspection,
    const std::shared_ptr<const runtime::WVExtensionCatalog>& catalog,
    const std::string& context) {
    for (const auto& file : inspection.observerRecord.outputFiles) {
        for (const auto& group : file.groups) {
            if (group.schedule.typeIdentifier != atwv::scheduleTypeIdentifier) {
                continue;
            }
            const auto continuation = std::find_if(
                inspection.scheduleContinuations.begin(),
                inspection.scheduleContinuations.end(),
                [&](const auto& candidate) {
                    return candidate.fileIdentifier == file.identifier &&
                           candidate.groupIdentifier == group.identifier;
                });
            require(continuation != inspection.scheduleContinuations.end(),
                    context + " has no persisted schedule continuation");
            std::shared_ptr<const runtime::WVOutputSchedule> schedule;
            auto status = catalog->outputSchedules().resolve(group.schedule,
                                                              schedule);
            requireStatus(status, context + " schedule reconstruction");
            runtime::WVOutputScheduleOccurrence occurrence;
            bool available = false;
            status = schedule->peek(
                continuation->cursor, inspection.latestRestart.t,
                group.schedule.finalTime, occurrence, available);
            requireStatus(status, context + " next occurrence discovery");
            if (available) {
                return occurrence.scheduledTime;
            }
        }
    }
    throw std::runtime_error(
        context + " fixture has no future AlongTrack occurrence for append");
}

void requireProgressAdvanced(const ProgressMap& before,
                             const ProgressMap& after,
                             const std::string& context) {
    require(before.size() == after.size(),
            context + " changed the logical AlongTrack schedule set");
    bool advanced = false;
    for (const auto& [key, initial] : before) {
        const auto found = after.find(key);
        require(found != after.end() &&
                    found->second.continuationOrdinal >=
                        initial.continuationOrdinal &&
                    found->second.destinationOrdinal >=
                        initial.destinationOrdinal &&
                    found->second.recordCount >= initial.recordCount,
                context + " regressed persisted commit progress");
        const auto ordinalDelta = found->second.destinationOrdinal -
                                  initial.destinationOrdinal;
        const auto recordDelta = found->second.recordCount - initial.recordCount;
        require(ordinalDelta == static_cast<std::int64_t>(recordDelta),
                context + " duplicated or omitted an AlongTrack pass");
        advanced = advanced || ordinalDelta > 0;
    }
    require(advanced, context + " did not append a future AlongTrack pass");
}

void requireSameProgress(const ProgressMap& segmented,
                         const ProgressMap& continuous,
                         const std::string& context) {
    require(segmented.size() == continuous.size(),
            context + " changed the continuous schedule set");
    for (const auto& [key, value] : segmented) {
        const auto found = continuous.find(key);
        require(found != continuous.end() &&
                    found->second.continuationOrdinal ==
                        value.continuationOrdinal &&
                    found->second.destinationOrdinal ==
                        value.destinationOrdinal &&
                    found->second.recordCount == value.recordCount,
                context +
                    " segmented continuation differs from continuous execution");
    }
}

void requireNetCDF(int status, const std::string& context) {
    require(status == NC_NOERR,
            context + ": " + (status == NC_NOERR ? std::string{} :
                                             std::string(nc_strerror(status))));
}

class NetCDFReadOnly final {
public:
    explicit NetCDFReadOnly(const std::filesystem::path& path)
        : path_(path.string()) {
        requireNetCDF(nc_open(path_.c_str(), NC_NOWRITE, &file_),
                      "open persisted output " + path_);
    }

    ~NetCDFReadOnly() {
        if (file_ >= 0) {
            (void)nc_close(file_);
        }
    }

    NetCDFReadOnly(const NetCDFReadOnly&) = delete;
    NetCDFReadOnly& operator=(const NetCDFReadOnly&) = delete;

    int group(const std::string& identifier) const {
        int result = -1;
        requireNetCDF(nc_inq_ncid(file_, identifier.c_str(), &result),
                      "find persisted group " + path_ + "/" + identifier);
        return result;
    }

    std::vector<double> realValues(int group,
                                   const std::string& variable) const {
        const auto descriptor = variableDescriptor(group, variable, NC_DOUBLE);
        std::vector<double> result(descriptor.second);
        if (!result.empty()) {
            requireNetCDF(nc_get_var_double(group, descriptor.first,
                                            result.data()),
                          "read " + path_ + "/" + variable);
        }
        return result;
    }

    std::vector<long long> integerValues(
        int group, const std::string& variable) const {
        const auto descriptor = variableDescriptor(group, variable, NC_INT64);
        std::vector<long long> result(descriptor.second);
        if (!result.empty()) {
            requireNetCDF(nc_get_var_longlong(group, descriptor.first,
                                              result.data()),
                          "read " + path_ + "/" + variable);
        }
        return result;
    }

private:
    std::pair<int, std::size_t> variableDescriptor(
        int group, const std::string& variable, nc_type expectedType) const {
        int identifier = -1;
        requireNetCDF(nc_inq_varid(group, variable.c_str(), &identifier),
                      "find " + path_ + "/" + variable);
        nc_type type = NC_NAT;
        requireNetCDF(nc_inq_vartype(group, identifier, &type),
                      "inspect type for " + path_ + "/" + variable);
        require(type == expectedType,
                path_ + "/" + variable + " has the wrong NetCDF type");
        int rank = 0;
        requireNetCDF(nc_inq_varndims(group, identifier, &rank),
                      "inspect rank for " + path_ + "/" + variable);
        require(rank >= 0, path_ + "/" + variable + " has a negative rank");
        std::vector<int> dimensions(static_cast<std::size_t>(rank));
        if (rank > 0) {
            requireNetCDF(nc_inq_vardimid(group, identifier,
                                          dimensions.data()),
                          "inspect dimensions for " + path_ + "/" + variable);
        }
        std::size_t count = 1;
        for (const int dimension : dimensions) {
            std::size_t length = 0;
            requireNetCDF(nc_inq_dimlen(group, dimension, &length),
                          "inspect extent for " + path_ + "/" + variable);
            require(length == 0 ||
                        count <= std::numeric_limits<std::size_t>::max() /
                                     length,
                    path_ + "/" + variable + " extent overflowed size_t");
            count *= length;
        }
        return {identifier, count};
    }

    std::string path_;
    int file_ = -1;
};

struct AlongTrackValues final {
    std::vector<double> triggerTimes;
    std::vector<double> sampleTimes;
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> ssh;
    std::vector<double> ssu;
    std::vector<double> ssv;
    std::vector<long long> scheduleOrdinals;
    std::vector<long long> repeatCycleIndices;
    std::vector<long long> firstSampleIndices;
    std::vector<long long> sampleCounts;
    std::vector<long long> committedSampleCounts;
};

AlongTrackValues readAlongTrackValues(const std::filesystem::path& path,
                                      const std::string& groupIdentifier) {
    NetCDFReadOnly file(path);
    const int group = file.group(groupIdentifier);
    AlongTrackValues result;
    result.triggerTimes = file.realValues(group, "t");
    result.sampleTimes = file.realValues(group, "sample-time");
    result.x = file.realValues(group, "x");
    result.y = file.realValues(group, "y");
    result.ssh = file.realValues(group, "ssh");
    result.ssu = file.realValues(group, "ssu");
    result.ssv = file.realValues(group, "ssv");
    result.scheduleOrdinals =
        file.integerValues(group, "portableScheduleOrdinal");
    result.repeatCycleIndices =
        file.integerValues(group, "repeat-cycle-index");
    result.firstSampleIndices =
        file.integerValues(group, "first-sample-index");
    result.sampleCounts = file.integerValues(group, "sample-count");
    result.committedSampleCounts =
        file.integerValues(group, "portableCommitted_sample");
    return result;
}

struct DifferenceMetrics final {
    double maximumAbsolute = 0.0;
    double maximumNormalized = 0.0;
};

DifferenceMetrics requireSameRealValues(const std::vector<double>& segmented,
                                        const std::vector<double>& continuous,
                                        const std::string& context) {
    require(segmented.size() == continuous.size(),
            context + " changed the persisted value extent");
    DifferenceMetrics metrics;
    for (std::size_t index = 0; index < segmented.size(); ++index) {
        const double difference =
            std::abs(segmented[index] - continuous[index]);
        const double normalized =
            difference / std::max(1.0, std::abs(continuous[index]));
        metrics.maximumAbsolute =
            std::max(metrics.maximumAbsolute, difference);
        metrics.maximumNormalized =
            std::max(metrics.maximumNormalized, normalized);
    }
    require(metrics.maximumNormalized <= 1.0e-12,
            context + " differs from continuous execution beyond 1e-12");
    return metrics;
}

void requireSameIntegerValues(const std::vector<long long>& segmented,
                              const std::vector<long long>& continuous,
                              const std::string& context) {
    require(segmented == continuous,
            context + " differs from continuous execution");
}

std::map<std::string, std::vector<std::uint8_t>> encodedSchemas(
    const runtime::WVModelOutputNetCDFInspection& inspection,
    const std::string& context) {
    std::map<std::string, std::vector<std::uint8_t>> result;
    for (const auto& schema : inspection.observationSchemas) {
        std::vector<std::uint8_t> encoded;
        requireStatus(runtime::encodeObservationSchemaManifest(schema.schema,
                                                               encoded),
                      context + " schema encoding");
        const auto inserted = result.emplace(schema.observerIdentifier,
                                             std::move(encoded));
        require(inserted.second,
                context + " contains duplicate observer schemas");
    }
    return result;
}

void requireSamePersistentContract(
    const runtime::WVModelOutputNetCDFInspection& first,
    const runtime::WVModelOutputNetCDFInspection& second,
    const std::string& context) {
    require(wavevortex::sameTransformConfiguration(
                first.latestRestart.configuration,
                second.latestRestart.configuration) &&
                first.latestRestart.coefficientShape.rows ==
                    second.latestRestart.coefficientShape.rows &&
                first.latestRestart.coefficientShape.columns ==
                    second.latestRestart.coefficientShape.columns &&
                first.latestRestart.t == second.latestRestart.t &&
                first.latestRestart.t0 == second.latestRestart.t0,
            context + " changed the persisted model configuration or restart");

    const auto& left = first.observerRecord;
    const auto& right = second.observerRecord;
    require(left.schemaIdentifier == right.schemaIdentifier &&
                left.schemaVersion == right.schemaVersion &&
                left.stateBlocks.size() == right.stateBlocks.size() &&
                left.observers.size() == right.observers.size() &&
                left.outputFiles.size() == right.outputFiles.size(),
            context + " changed the persisted output graph extent");
    for (std::size_t index = 0; index < left.stateBlocks.size(); ++index) {
        const auto& a = left.stateBlocks[index];
        const auto& b = right.stateBlocks[index];
        require(a.identifier == b.identifier && a.scalarType == b.scalarType &&
                    a.dimensions == b.dimensions &&
                    a.toleranceKind == b.toleranceKind &&
                    a.absoluteTolerance == b.absoluteTolerance &&
                    a.ownership == b.ownership &&
                    a.restartRequirement == b.restartRequirement,
                context + " changed a persisted state-block contract");
    }
    for (std::size_t index = 0; index < left.observers.size(); ++index) {
        require(runtime::sameOutputObserverSemanticIdentity(
                    left.observers[index], right.observers[index]),
                context + " changed a persisted observer identity/configuration");
    }
    for (std::size_t fileIndex = 0; fileIndex < left.outputFiles.size();
         ++fileIndex) {
        const auto& a = left.outputFiles[fileIndex];
        const auto& b = right.outputFiles[fileIndex];
        require(a.identifier == b.identifier &&
                    a.groups.size() == b.groups.size(),
                context + " changed a persisted file identity");
        for (std::size_t groupIndex = 0; groupIndex < a.groups.size();
             ++groupIndex) {
            require(runtime::sameLogicalOutputScheduleIdentity(
                        a.groups[groupIndex], b.groups[groupIndex]),
                    context +
                        " changed a persisted schedule identity/configuration");
        }
    }
    require(encodedSchemas(first, context + " first") ==
                encodedSchemas(second, context + " second"),
            context + " changed an encoded observation schema");

    require(first.scheduleContinuations.size() ==
                second.scheduleContinuations.size(),
            context + " changed the continuation set");
    for (const auto& continuation : first.scheduleContinuations) {
        const auto found = std::find_if(
            second.scheduleContinuations.begin(),
            second.scheduleContinuations.end(), [&](const auto& candidate) {
                return candidate.fileIdentifier == continuation.fileIdentifier &&
                       candidate.groupIdentifier == continuation.groupIdentifier;
            });
        require(found != second.scheduleContinuations.end() &&
                    found->cursor.committedOrdinal ==
                        continuation.cursor.committedOrdinal &&
                    runtime::samePortableTypedRecordValue(
                        found->cursor.values, continuation.cursor.values),
                context + " changed a complete schedule continuation cursor");
    }

    require(first.destinationProgress.size() ==
                second.destinationProgress.size(),
            context + " changed the destination-progress set");
    for (const auto& progress : first.destinationProgress) {
        const auto found = std::find_if(
            second.destinationProgress.begin(),
            second.destinationProgress.end(), [&](const auto& candidate) {
                return candidate.fileIdentifier == progress.fileIdentifier &&
                       candidate.groupIdentifier == progress.groupIdentifier;
            });
        require(found != second.destinationProgress.end() &&
                    found->recordCount == progress.recordCount &&
                    found->hasCommittedTime == progress.hasCommittedTime &&
                    found->lastCommittedTime == progress.lastCommittedTime &&
                    found->committedScheduleCursor.committedOrdinal ==
                        progress.committedScheduleCursor.committedOrdinal &&
                    runtime::samePortableTypedRecordValue(
                        found->committedScheduleCursor.values,
                        progress.committedScheduleCursor.values) &&
                    found->unlimitedAxes.size() ==
                        progress.unlimitedAxes.size(),
                context + " changed complete per-destination commit progress");
        for (std::size_t axis = 0; axis < progress.unlimitedAxes.size(); ++axis) {
            const auto& a = progress.unlimitedAxes[axis];
            const auto& b = found->unlimitedAxes[axis];
            require(a.axisIdentifier == b.axisIdentifier &&
                        a.committedCount == b.committedCount &&
                        a.physicalCount == b.physicalCount,
                    context + " changed an unlimited-axis commit offset");
        }
    }
}

runtime::WVCheckpoint restoreCheckpoint(
    const runtime::WVModelOutputNetCDFInspection& inspection,
    const std::shared_ptr<const runtime::WVExtensionCatalog>& catalog,
    const std::string& context) {
    runtime::WVIntegrationStateLayout layout;
    requireStatus(runtime::WVIntegrationStateLayout::create(
                      inspection.latestRestart.coefficientShape,
                      inspection.observerRecord, layout),
                  context + " state-layout reconstruction");
    runtime::WVCheckpoint checkpoint;
    runtime::WVAdditionalStateStorage additionalState;
    const auto status = runtime::WVModelOutputNetCDFSink::restoreState(
        inspection, *catalog, layout, checkpoint, additionalState);
    require(static_cast<bool>(status), context + ": " + status.message);
    return checkpoint;
}

DifferenceMetrics requireSameComplexValues(
    const std::vector<wavevortex::WVComplex64>& segmented,
    const std::vector<wavevortex::WVComplex64>& continuous,
    const std::string& context) {
    require(segmented.size() == continuous.size(),
            context + " changed the checkpoint coefficient extent");
    DifferenceMetrics metrics;
    for (std::size_t index = 0; index < segmented.size(); ++index) {
        const double realDifference =
            std::abs(segmented[index].real - continuous[index].real);
        const double imaginaryDifference =
            std::abs(segmented[index].imag - continuous[index].imag);
        metrics.maximumAbsolute = std::max(
            metrics.maximumAbsolute, std::max(realDifference,
                                               imaginaryDifference));
        metrics.maximumNormalized = std::max(
            metrics.maximumNormalized,
            std::max(realDifference /
                         std::max(1.0, std::abs(continuous[index].real)),
                     imaginaryDifference /
                         std::max(1.0, std::abs(continuous[index].imag))));
    }
    require(metrics.maximumNormalized <= 1.0e-12,
            context + " differs from continuous execution beyond 1e-12");
    return metrics;
}

void requireSamePersistedValuesAndState(
    const runtime::WVModelOutputNetCDFInspection& segmented,
    const runtime::WVModelOutputNetCDFInspection& continuous,
    const std::shared_ptr<const runtime::WVExtensionCatalog>& catalog,
    const std::string& context) {
    requireSamePersistentContract(segmented, continuous, context);
    require(segmented.observerRecord.outputFiles.size() ==
                continuous.observerRecord.outputFiles.size(),
            context + " changed the persisted file set");

    DifferenceMetrics aggregate;
    for (const auto& file : segmented.observerRecord.outputFiles) {
        const auto matchingFile = std::find_if(
            continuous.observerRecord.outputFiles.begin(),
            continuous.observerRecord.outputFiles.end(),
            [&](const auto& candidate) {
                return candidate.identifier == file.identifier;
            });
        require(matchingFile != continuous.observerRecord.outputFiles.end(),
                context + " lost a continuous output file");
        for (const auto& group : file.groups) {
            if (group.schedule.typeIdentifier != atwv::scheduleTypeIdentifier) {
                continue;
            }
            const auto matchingGroup = std::find_if(
                matchingFile->groups.begin(), matchingFile->groups.end(),
                [&](const auto& candidate) {
                    return candidate.identifier == group.identifier;
                });
            require(matchingGroup != matchingFile->groups.end(),
                    context + " lost an AlongTrack output group");
            const auto left = readAlongTrackValues(file.destination,
                                                   group.identifier);
            const auto right = readAlongTrackValues(matchingFile->destination,
                                                    matchingGroup->identifier);
            const auto compare = [&](const std::vector<double>& a,
                                     const std::vector<double>& b,
                                     const std::string& name) {
                const auto metric = requireSameRealValues(
                    a, b, context + " " + name);
                aggregate.maximumAbsolute = std::max(
                    aggregate.maximumAbsolute, metric.maximumAbsolute);
                aggregate.maximumNormalized = std::max(
                    aggregate.maximumNormalized, metric.maximumNormalized);
            };
            compare(left.triggerTimes, right.triggerTimes, "trigger times");
            compare(left.sampleTimes, right.sampleTimes, "sample times");
            compare(left.x, right.x, "projected x");
            compare(left.y, right.y, "projected y");
            compare(left.ssh, right.ssh, "ssh");
            compare(left.ssu, right.ssu, "ssu");
            compare(left.ssv, right.ssv, "ssv");
            requireSameIntegerValues(left.scheduleOrdinals,
                                     right.scheduleOrdinals,
                                     context + " schedule ordinals");
            requireSameIntegerValues(left.repeatCycleIndices,
                                     right.repeatCycleIndices,
                                     context + " repeat-cycle indices");
            requireSameIntegerValues(left.firstSampleIndices,
                                     right.firstSampleIndices,
                                     context + " first-sample indices");
            requireSameIntegerValues(left.sampleCounts, right.sampleCounts,
                                     context + " sample counts");
            requireSameIntegerValues(left.committedSampleCounts,
                                     right.committedSampleCounts,
                                     context + " committed sample offsets");
        }
    }

    const auto leftCheckpoint =
        restoreCheckpoint(segmented, catalog, context + " segmented restore");
    const auto rightCheckpoint =
        restoreCheckpoint(continuous, catalog, context + " continuous restore");
    require(leftCheckpoint.state.t == rightCheckpoint.state.t &&
                leftCheckpoint.state.t0 == rightCheckpoint.state.t0 &&
                wavevortex::sameTransformConfiguration(
                    leftCheckpoint.configuration,
                    rightCheckpoint.configuration),
            context + " changed the restored model state metadata");
    for (const auto& [name, left, right] :
         std::vector<std::tuple<
             const char*, const std::vector<wavevortex::WVComplex64>*,
             const std::vector<wavevortex::WVComplex64>*>>{
             {"Ap", &leftCheckpoint.state.coefficients.Ap,
              &rightCheckpoint.state.coefficients.Ap},
             {"Am", &leftCheckpoint.state.coefficients.Am,
              &rightCheckpoint.state.coefficients.Am},
             {"A0", &leftCheckpoint.state.coefficients.A0,
              &rightCheckpoint.state.coefficients.A0}}) {
        const auto metric = requireSameComplexValues(
            *left, *right, context + " restored " + name);
        aggregate.maximumAbsolute =
            std::max(aggregate.maximumAbsolute, metric.maximumAbsolute);
        aggregate.maximumNormalized =
            std::max(aggregate.maximumNormalized, metric.maximumNormalized);
    }
    std::cout << std::setprecision(17) << "METRIC " << context
              << " persisted-value-abs=" << aggregate.maximumAbsolute
              << " persisted-value-norm=" << aggregate.maximumNormalized
              << '\n';
}

void testRunnerCase(
    const std::filesystem::path& templatePath,
    const std::string& name,
    const std::string& expectedIntegrator,
    const std::shared_ptr<const runtime::WVExtensionCatalog>& catalog,
    const std::filesystem::path& root) {
    auto request = prepareRequest(templatePath, root, name);
    require(request.document.at("integration").at("method") ==
                expectedIntegrator,
            name + " request uses the wrong integration method");

    const auto baseOut = root / (name + "-base.stdout");
    const auto baseError = root / (name + "-base.stderr");
    const auto baseStatus =
        run(ATS4_BASE_RUNNER, request.path, baseOut, baseError);
    require(baseStatus == 6,
            name + " built-in runner returned " +
                std::to_string(baseStatus) +
                " instead of the output-capability exit code 6: " +
                text(baseError));
    const auto baseFailure = readJSON(baseError);
    require(baseFailure.at("status") == "failed" &&
                baseFailure.at("exitCode") == 6 &&
                baseFailure.at("failure").at("stage") ==
                    "model-output-preflight" &&
                baseFailure.at("failure").at("message") ==
                    "Unsupported observing-system identity or contract version.",
            name + " built-in runner did not reject the AlongTrack capability "
                   "during model-output preflight");
    for (const auto& destination : request.destinations) {
        require(!std::filesystem::exists(destination),
                name + " base rejection created a destination");
    }
    requireSourcesUnchanged(request, name + " base rejection");

    std::error_code removeError;
    std::filesystem::remove(request.report, removeError);
    const auto extendedOut = root / (name + "-extended.stdout");
    const auto extendedError = root / (name + "-extended.stderr");
    const auto extendedStatus = run(ATS4_EXTENDED_RUNNER, request.path,
                                    extendedOut, extendedError);
    require(extendedStatus == 0,
            name + " extended runner rejected the identical bundle: " +
                text(extendedError));
    const auto report = readJSON(request.report);
    require(report.at("status") == "complete" &&
                report.at("integrator").at("id") == expectedIntegrator,
            name + " runner report is incomplete");
    if (expectedIntegrator == "adaptive-rk23") {
        require(report.at("integrator")
                        .at("denseOutputEvaluationCount")
                        .get<std::size_t>() > 0,
                name + " adaptive run exercised no dense occurrence");
    } else {
        require(report.at("integrator")
                        .at("denseOutputEvaluationCount")
                        .get<std::size_t>() == 0,
                name + " fixed run did not keep occurrences on exact steps");
    }
    for (const auto& destination : request.destinations) {
        require(std::filesystem::is_regular_file(destination),
                name + " extended runner omitted a destination");
    }
    requireSourcesUnchanged(request, name + " create execution");
    const auto createdInspection = inspect(request.destinations, catalog,
                                           name + " create inspection");
    const auto createdProgress = requireAlongTrackPersistence(
        createdInspection, name + " create");

    std::vector<std::vector<char>> protectedBytes;
    for (const auto& destination : request.destinations) {
        protectedBytes.push_back(bytes(destination));
    }
    require(run(ATS4_EXTENDED_RUNNER, request.path, extendedOut,
                extendedError) != 0,
            name + " create policy replaced existing destinations");
    for (std::size_t index = 0; index < request.destinations.size(); ++index) {
        require(bytes(request.destinations[index]) == protectedBytes[index],
                name + " failed create mutated a prior destination");
    }

    auto failedReplace = request.document;
    failedReplace["output"]["policy"] = "replace";
    auto& failedDestinations = failedReplace["output"]["destinations"];
    const auto firstIdentifier = failedDestinations.begin().key();
    failedDestinations["not-a-model-file-identifier"] =
        failedDestinations.at(firstIdentifier);
    failedDestinations.erase(firstIdentifier);
    failedReplace["report"] =
        (root / (name + "-failed-replace-report.json")).string();
    const auto failedReplacePath = root / (name + "-failed-replace.json");
    writeJSON(failedReplacePath, failedReplace);
    require(run(ATS4_EXTENDED_RUNNER, failedReplacePath, extendedOut,
                extendedError) != 0,
            name + " invalid transactional replacement succeeded");
    for (std::size_t index = 0; index < request.destinations.size(); ++index) {
        require(bytes(request.destinations[index]) == protectedBytes[index],
                name + " failed replacement changed a prior destination");
    }

    std::vector<std::vector<char>> replacementSentinels;
    for (std::size_t index = 0; index < request.destinations.size(); ++index) {
        std::ofstream sentinel(request.destinations[index],
                               std::ios::binary | std::ios::trunc);
        require(static_cast<bool>(sentinel),
                name + " could not stage a replacement sentinel");
        sentinel << "ATS4 prior destination " << index;
        sentinel.close();
        replacementSentinels.push_back(bytes(request.destinations[index]));
    }

    auto replace = request.document;
    replace["output"]["policy"] = "replace";
    replace["report"] = (root / (name + "-replace-report.json")).string();
    const auto replacePath = root / (name + "-replace.json");
    writeJSON(replacePath, replace);
    const auto replaceStatus = run(ATS4_EXTENDED_RUNNER, replacePath,
                                   extendedOut, extendedError);
    require(replaceStatus == 0,
            name + " transactional replacement failed: " +
                text(extendedError));
    requireSourcesUnchanged(request, name + " replace execution");
    for (std::size_t index = 0; index < request.destinations.size(); ++index) {
        require(bytes(request.destinations[index]) !=
                    replacementSentinels[index],
                name + " replacement left the prior sentinel installed");
    }
    const auto replacedInspection = inspect(request.destinations, catalog,
                                            name + " replace inspection");
    const auto replacedProgress = requireAlongTrackPersistence(
        replacedInspection, name + " replace");
    requireSameProgress(createdProgress, replacedProgress,
                        name + " replace");
    requireSamePersistedValuesAndState(
        createdInspection, replacedInspection, catalog,
        name + " transactional replace parity");

    const auto appendFinalTime = nextAlongTrackTime(
        replacedInspection, catalog, name + " append");
    auto append = request.document;
    append["modelFiles"] = json::array();
    for (const auto& destination : request.destinations) {
        append["modelFiles"].push_back(destination.string());
    }
    append["integration"]["finalTime"] = appendFinalTime;
    append["output"]["policy"] = "append";
    append["output"]["destinations"] = json::object();
    append["report"] = (root / (name + "-append-report.json")).string();
    const auto appendPath = root / (name + "-append.json");
    writeJSON(appendPath, append);
    const auto appendStatus = run(ATS4_EXTENDED_RUNNER, appendPath,
                                  extendedOut, extendedError);
    require(appendStatus == 0,
            name + " append/restart failed: " + text(extendedError));
    const auto appendedInspection = inspect(
        request.destinations, catalog, name + " append inspection");
    const auto appendedProgress = requireAlongTrackPersistence(
        appendedInspection, name + " append");
    requireProgressAdvanced(replacedProgress, appendedProgress,
                            name + " append");
    requireSourcesUnchanged(request, name + " append execution");

    auto continuous = request.document;
    continuous["integration"]["finalTime"] = appendFinalTime;
    auto& continuousDestinations = continuous["output"]["destinations"];
    std::vector<std::filesystem::path> continuousPaths;
    for (auto& [identifier, value] : continuousDestinations.items()) {
        (void)value;
        const auto path = root / (name + "-continuous-" + identifier + ".nc");
        continuousPaths.push_back(path);
        continuousDestinations[identifier] = path.string();
    }
    continuous["report"] =
        (root / (name + "-continuous-report.json")).string();
    const auto continuousPath = root / (name + "-continuous.json");
    writeJSON(continuousPath, continuous);
    const auto continuousStatus = run(ATS4_EXTENDED_RUNNER, continuousPath,
                                      extendedOut, extendedError);
    require(continuousStatus == 0,
            name + " continuous control failed: " + text(extendedError));
    const auto continuousInspection = inspect(
        continuousPaths, catalog, name + " continuous inspection");
    const auto continuousProgress = requireAlongTrackPersistence(
        continuousInspection, name + " continuous");
    requireSameProgress(appendedProgress, continuousProgress,
                        name + " segmented continuation");
    requireSamePersistedValuesAndState(
        appendedInspection, continuousInspection, catalog,
        name + " segmented continuation");
    requireSourcesUnchanged(request, name + " continuous execution");
}

} // namespace

int main() {
    try {
        const auto identity = std::chrono::steady_clock::now()
                                  .time_since_epoch()
                                  .count();
        const auto root = std::filesystem::temp_directory_path() /
                          ("ats4-source-linked-runner-" +
                           std::to_string(identity));
        std::filesystem::create_directories(root);
        const auto catalog = extendedCatalog();
        testRunnerCase(ATS4_REPEATING_REQUEST, "repeating", "fixed-rk4",
                       catalog, root);
        testRunnerCase(ATS4_GEODETIC_REQUEST, "geodetic", "adaptive-rk23",
                       catalog, root);
        std::filesystem::remove_all(root);
        std::cout << "PASS: source-linked AlongTrack runner and persistence\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
