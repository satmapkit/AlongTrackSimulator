---
layout: default
title: Portable WaveVortex runner
nav_order: 6
description: Build, author, and run the source-linked AlongTrack WaveVortex extension
permalink: /portable-wave-vortex-runner
---

# Portable WaveVortex runner

`alongtrack-wave-vortex-run` is the supported MATLAB-independent executor for portable WaveVortexModel bundles that contain `WVAlongTrackSchedule` and `WVAlongTrackObservingSystem` records. MATLAB remains the scientific and authoring authority. The executable is a source-built client of `wave-vortex-portable-source-api-v1`, not a second scientific configuration system or a stable binary plug-in interface. Compatibility is source-level: select the WaveVortexModel revision explicitly and recompile the application and extension together.

## Select the WaveVortexModel source dependency

The source checkout can live anywhere. Pass its location explicitly through `ALONGTRACK_WAVEVORTEX_SOURCE_DIR`; the build does not search for a machine-specific sibling directory. AlongTrackSimulator CI selects WaveVortexModel commit `46b4d787dab69803cc9340dfde67afa0c9c1eddc` as the source API v1 candidate. Selection does not itself record a pass: the required workflow run must qualify that exact revision before it becomes the accepted dependency. A later descendant is another deliberate dependency update and must pass the same compatibility, numerical, persistence, and performance gates.

```sh
git clone https://github.com/satmapkit/AlongTrackSimulator.git AlongTrackSimulator
git clone https://github.com/JeffreyEarly/wave-vortex-model.git WaveVortexModel
git -C WaveVortexModel checkout 46b4d787dab69803cc9340dfde67afa0c9c1eddc

cmake -S AlongTrackSimulator -B build/alongtrack-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DALONGTRACK_BUILD_WAVEVORTEX_EXTENSION=ON \
  -DALONGTRACK_WAVEVORTEX_SOURCE_DIR="$(pwd)/WaveVortexModel"
cmake --build build/alongtrack-release --parallel --target alongtrack-wave-vortex-run
ctest --test-dir build/alongtrack-release --output-on-failure
```

The reference build requires CMake 3.20, a C++17 compiler, and NetCDF C. `ALONGTRACK_WARNINGS_AS_ERRORS=ON` enables the warning policy used in CI. The source-linked workflow exercises the candidate on Ubuntu with GCC and Clang and on macOS with AppleClang. These are the CI qualification lanes for the pinned WaveVortexModel source dependency; their presence does not assert a pass independently of the required workflow result. The ordinary extension-disabled `AlongTrack::PortableCore` build remains independent of WaveVortexModel and NetCDF and is also qualified on Windows with MSVC. The pinned WaveVortexModel revision does not compile under MSVC, so a Windows source-linked runner is not supported.

## Catalog composition and runner ownership

The executable owns one `WVExtensionCatalogBuilder`. Its `main` performs the following sequence before inspecting a bundle:

```cpp
wavevortex::runtime::WVExtensionCatalogBuilder builder;
auto status = wavevortex::runtime::addBuiltInExtensions(builder);
if (status) {
    status = alongtrack::wavevortex_extension::registerAlongTrackExtensions(builder);
}
std::shared_ptr<const wavevortex::runtime::WVExtensionCatalog> catalog;
if (status) {
    status = builder.freeze(catalog);
}
return wavevortex::runtime::runWaveVortex(argc,argv,std::move(catalog));
```

This builds WaveVortexModel's forcing, schedule, and observer factories and the AlongTrack pair into one catalog, registers AlongTrack explicitly before inspection, freezes exactly once, and calls the same `runWaveVortex` entry point used by the built-in-only `wave-vortex-run`. Source API v1 observer registration has exactly five inputs: identity, contract version, factory, optional configuration resolver, and optional output-plan resolver. It exposes no legacy persistence callbacks. There is no static-initialization registration, process-global mutable registry, duplicate argument parser, duplicate integrator, duplicate output graph, dynamic discovery, binary plug-in ABI, or downloaded provider.

## Author a portable bundle in MATLAB

Use `authorAlongTrackPortableRunBundle` at the repository root to write the supported bundle. The helper writes a restart-capable WaveVortexModel NetCDF source and the strict `wave-vortex-run-request-v1` JSON request. The NetCDF files remain authoritative for model configuration, state, forcing, output groups, schedules, observers, schemas, continuation, and destination progress; JSON selects execution settings, integration options, destination paths, output policy, and report location only.

```matlab
bundle = authorAlongTrackPortableRunBundle( ...
    model,"portable-jason3.nc","j3", ...
    initialTime=0, ...
    integrationFinalTime=0.5, ...
    finalTime=50000, ...
    missionEpochSeconds=0, ...
    fieldNames=["ssh","ssu","ssv"], ...
    interpolation="linear", ...
    integrationMethod="adaptive-rk23");
```

The required inputs are the authoritative scalar `WVModel`, the source NetCDF path, and a mission-catalog key. `initialTime` and `finalTime` define the complete persisted schedule horizon. `integrationFinalTime` selects the first runner segment endpoint and defaults to `finalTime`; keeping it earlier than the schedule horizon creates the source for a later restart or segmented-continuation request without shortening the schedule contract. The mission epoch, cadence, fields, interpolation, stable file/logical-group/observer identifiers, request/destination/report paths, create/replace/append policy, integrator, tolerances, FFT provider, and thread count are explicit name-value options. The logical group identifier is the schedule-instance identity supported by the reusable runtime; the paired schedule type identifier, contract version, and typed configuration complete its semantic identity. The returned `bundle` struct records `modelFile`, `requestFile`, `destinationFile`, `reportFile`, every supported stable identity and paired type/version, the exact resolved `sourceConfiguration`, ordered `fieldNames`, interpolation, and decoded request. Use those returned paths rather than reconstructing companion filenames in automation.

The helper resolves the mission catalog while authoring and writes exact typed construction records. Both schedule and observer records retain the resolved orbital elements and repeat count, the mission epoch and time convention, projection window and origin, model domain, sampling cadence, stable identities and contract versions, and construction identity. The observer record additionally retains the ordered requested field identities and interpolation choice. A saved run therefore cannot change when a later MATLAB mission catalog is edited.

The authoring path stores no complete future trajectory or pass sequence. Repeating and nonrepeating/geodetic missions use the same lazy pass source; `passesPerCycle` is the exact positive repeat count for a repeating mission and zero for a geodetic mission.

Run the identical request with either executable:

```sh
wave-vortex-run --request portable-jason3.run.json
alongtrack-wave-vortex-run --request portable-jason3.run.json
```

The built-in-only executable rejects the AlongTrack schedule/observer pair during allocation-light semantic capability preflight. Rejection precedes AlongTrack provider construction, FFT provider construction, state-sized allocation, integration advancement, and destination creation or mutation. The extended executable resolves and executes that same bundle because its frozen catalog contains the explicit AlongTrack registrations.

## Portable contracts

The extension contract version is 1. Construction and persistence use named `WVPortableTypedRecord` values; event execution uses construction-resolved numeric slots and contains no per-sample string lookup.

| Contract | Persisted or retained content |
| --- | --- |
| Schedule configuration | `wv-along-track-schedule-configuration-v1`: mission key; source and resolved semimajor axes; eccentricity; inclination; ascending-node longitude; exact repeat pass count or zero; epoch seconds; projection width, height, center, and origin; sample interval. |
| Observer configuration | `wv-along-track-observer-configuration-v1`: the same resolved source configuration plus ordered, unique field names and linear or spline interpolation. |
| Schedule cursor | Committed ordinal plus `wv-along-track-pass-cursor-v1` descriptor: repeat-cycle index, first cadence-lattice sample index, sample count, and exact construction-configuration identity. A fresh cursor has ordinal `-1` and an empty typed record. |
| Occurrence payload | `wv-along-track-pass-payload-v1`: the same four integer descriptor values in resolved slots. The compact payload identifies one semantic pass without named event-loop lookup. |
| Observer occurrence | Evaluator-owned, in-flight sample times, projected `x` and `y`, pass identity, extents, and resolved position-set key. Geometry is prepared only for the current occurrence. |
| Observation batch | Generic data-only values for `sample-time`, `x`, `y`, `repeat-cycle-index`, `first-sample-index`, `sample-count`, and each requested sampled field. NetCDF operations remain in WaveVortexModel's generic sink. |
| Persistence | Exact schedule and observer records, schema, minimal continuation cursor, and sink-owned per-destination committed record and ragged-axis progress. Rebuildable geometry, evaluated fields, batches, integration scratch, and future passes are not checkpoint state. |

Version 1 evaluates one accepted or dense-interpolated model state at the pass trigger time. Per-sample times are observation coordinates and metadata; they do not request a separate dense-state evaluation for every sample.

## Output, restart, continuation, and retry

The request runner preserves WaveVortexModel's graph-wide destination policies:

| Policy | Behavior |
| --- | --- |
| `create` | Requires a complete destination map, refuses existing destinations, starts destination commit progress empty, and may resume scheduling from a noninitial source cursor. |
| `replace` | Requires a complete destination map. Failed replacement preflight, staging, or atomic installation preserves every prior destination. The generic WaveVortex runner commits the installed replacement set before integration; a later integration or destination-route failure leaves that installed set in place and does not restore prior files. |
| `append` | Validates every existing destination read-only, including graph, schema, complete typed cursor, committed counts, time-last evidence, and ragged offsets, before reopening any destination for mutation. |
| restart or segmented continuation | Restores the minimal schedule cursor separately from destination commit progress. The next pass follows the cursor while each sink route resumes from its own validated offsets, so prior passes are neither duplicated nor omitted. |

A semantic occurrence is keyed by the resolved observer, logical group/schedule instance, schedule type/version/configuration, trigger time, ordinal/cursor, payload, prepared geometry, and resolved field plan. Destination identity is not part of that scientific identity. If a later destination route fails, the driver retains the complete compatible occurrence workspace until retry: successful routes are not repeated, the failed route's committed offsets remain unchanged, the pass is not rediscovered, fields are not reevaluated, and the accepted integration state is committed once. Coincident destinations share prepared work only under exact compatible occurrence, geometry, and field-plan keys; distinct logical schedule instances remain distinct.

Source checkpoint and model files are always read-only inputs for `create` and `replace`. Append mutates only destinations that passed complete preflight. Path aliases between protected sources and create/replace destinations are rejected before mutation.

## Missions, numerical agreement, and storage

The portable pair supports repeating missions and nonrepeating/geodetic missions, fixed RK4 and adaptive RK3(2) integration, exact-time events, and dense-interpolated events. Portable comparisons cover schedule trigger times and payloads, pass identity and boundaries, per-sample time, projected coordinates, `ssh`, `ssu`, `ssv`, requested additional position-sampleable fields, NetCDF schema and metadata, persisted values, restart, and segmented continuation.

MATLAB/C++ comparisons use an absolute and normalized/scientific maximum no greater than `1e-12`, or the tighter algorithm-specific tolerance already recorded by the portable-core and extension parity fixtures. Tolerances are not widened to accommodate a failing case.

Retained schedule and observer storage consists of the immutable catalog/configuration, minimal cursor and route progress, and current in-flight occurrence workspaces. It is independent of integration duration and contains no complete future trajectory or pass list. Performance qualification compares serialized, matched base and extended runs with identical source revision, compiler, FFT provider, fixture, integration options, thread count, and host. The extended runner must remain within 3% of the built-in runner for complete integration runtime and exact retained memory after accounting for catalog, schedules, observers, plans, workspaces, sink, driver, integrator, system, state, and model storage exactly once.

The ATS #4 qualification used seven serialized, alternating base/extended samples. Median complete runtime was 1.315276 s for the base runner and 1.315780416 s for the extended runner (+0.0383506%); median integration runtime was 1.307509458 s and 1.307882583 s (+0.0285371%). Exact retained storage was 151,759 B and 152,136 B (+377 B, +0.248420%), while maximum-live storage was 167,658 B and 168,035 B (+377 B, +0.224863%); the catalog contribution was 10,068 B and 10,445 B. These totals exercise an active built-in model-output configuration, evaluator, sink, driver, orchestration workspace, and occurrence workspace. The separate `scheduledOutput` and `outputPlanMaximumLive` report fields remain zero because they describe the legacy explicit command-line checkpoint route, not omitted model-output-graph storage. The source package preserves the path-safe fixture, request, and frozen summary in [`cpp/tests/data/ats4/benchmark`](https://github.com/satmapkit/AlongTrackSimulator/tree/main/cpp/tests/data/ats4/benchmark); the authoring checkout preserves the exact, unmodified raw reports in [`Documentation/Verification/ATS4/benchmark`](https://github.com/satmapkit/AlongTrackSimulator/tree/main/Documentation/Verification/ATS4/benchmark).

## Source-only distribution and unsupported behavior

The package/export contains the MATLAB author, portable core, extension and runner sources, CMake inputs, tests, and parity fixtures. It contains no executable, library, object, MEX file, build directory, machine-specific path, FFTW download, or other provider artifact. CI creates an unpublished MPM-style export with isolated MATLAB preferences and a temporary add-on installation, scans that export, and builds and tests it from an unrelated working directory against the pinned WaveVortexModel source selection.

The portable runner does not support arbitrary MATLAB subclasses, state-triggered schedules, multi-state model sampling within one occurrence, dynamic or binary plug-ins, or compiled distribution. These exclusions are contract boundaries, not silent fallbacks. Use MATLAB for scientific configurations outside the explicitly paired portable identities.
