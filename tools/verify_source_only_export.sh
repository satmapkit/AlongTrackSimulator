#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    printf 'usage: %s <AlongTrack export root> <WaveVortexModel source root> [build root]\n' "$0" >&2
    exit 2
fi

export_root=$(cd "$1" && pwd -P)
wavevortex_root=$(cd "$2" && pwd -P)
temporary_root=""
if [[ $# -eq 3 ]]; then
    mkdir -p "$3"
    build_root=$(cd "$3" && pwd -P)
else
    temporary_root=$(mktemp -d "${TMPDIR:-/tmp}/alongtrack-source-export.XXXXXX")
    build_root="$temporary_root/build"
fi
working_directory=$(mktemp -d "${TMPDIR:-/tmp}/alongtrack-export-working-directory.XXXXXX")

case "$build_root/" in
    "$export_root/"*)
        printf 'The build root must be outside the exported source tree.\n' >&2
        exit 1
        ;;
esac

cleanup() {
    if [[ -d "$working_directory" ]]; then
        rm -rf -- "$working_directory"
    fi
    if [[ -n "$temporary_root" && -d "$temporary_root" ]]; then
        rm -rf -- "$temporary_root"
    fi
}
trap cleanup EXIT

if [[ ! -f "$export_root/resources/mpackage.json" ]]; then
    printf 'AlongTrack package manifest not found in %s\n' "$export_root" >&2
    exit 1
fi
if [[ ! -f "$wavevortex_root/PortableRuntime/CMakeLists.txt" ]]; then
    printf 'WaveVortexModel PortableRuntime source not found in %s\n' "$wavevortex_root" >&2
    exit 1
fi

if [[ -n "${ATS_EXPECTED_WVM_REVISION:-}" ]]; then
    actual_revision=$(git -C "$wavevortex_root" rev-parse HEAD)
    if [[ "$actual_revision" != "$ATS_EXPECTED_WVM_REVISION" ]]; then
        printf 'WaveVortexModel revision mismatch: expected %s, found %s\n' "$ATS_EXPECTED_WVM_REVISION" "$actual_revision" >&2
        exit 1
    fi
fi

required_paths=(
    CMakeLists.txt
    authorAlongTrackPortableRunBundle.m
    UnitTests/TestAlongTrackPortableRunBundle.m
    resources/mpackage.json
    cpp/include/alongtrack/portable_core.hpp
    cpp/src/portable_core.cpp
    cpp/extensions/wavevortex/include/alongtrack/wavevortex_extension.hpp
    cpp/extensions/wavevortex/src/wavevortex_extension.cpp
    cpp/extensions/wavevortex/app/AlongTrackWaveVortexRunMain.cpp
    cpp/extensions/wavevortex/tests/check_runner_composition.cmake
    cpp/extensions/wavevortex/tests/test_source_linked_runner.cpp
    cpp/tests/data/matlab_reference.csv
    cpp/tests/data/wavevortex_matlab_reference.csv
    cpp/tests/data/ats4/matlab-authored-repeating.nc
    cpp/tests/data/ats4/matlab-authored-repeating-request.json
    cpp/tests/data/ats4/matlab-authored-geodetic.nc
    cpp/tests/data/ats4/matlab-authored-geodetic-request.json
    cpp/tests/data/ats4/README.md
    cpp/tests/data/ats4/benchmark/README.md
    cpp/tests/data/ats4/benchmark/matlab-authored-builtin-output.nc
    cpp/tests/data/ats4/benchmark/matlab-authored-builtin-output-request.json
)
for relative_path in "${required_paths[@]}"; do
    if [[ ! -f "$export_root/$relative_path" ]]; then
        printf 'Required source-only export input is missing: %s\n' "$relative_path" >&2
        exit 1
    fi
done

if [[ -e "$export_root/cpp/tests/data/ats4/benchmark/raw" ]]; then
    printf 'Raw benchmark reports with host paths must remain authoring-only.\n' >&2
    exit 1
fi

for excluded_directory in .git .github .buildtool Documentation docs tools dist build CMakeFiles; do
    if [[ -e "$export_root/$excluded_directory" ]]; then
        printf 'Authoring or build directory must not be exported: %s\n' "$excluded_directory" >&2
        exit 1
    fi
done

forbidden_files=$(find "$export_root" -type f \( \
    -name '*.a' -o -name '*.dylib' -o -name '*.dll' -o -name '*.exe' -o \
    -name '*.exp' -o -name '*.lib' -o -name '*.mex*' -o -name '*.o' -o \
    -name '*.obj' -o -name '*.pdb' -o -name '*.so' -o -name '*.tar.gz' -o \
    -name '*.tar.xz' -o -name '*.tgz' -o -name '*.zip' -o \
    -name alongtrack-wave-vortex-run -o -name wave-vortex-run -o \
    -name CMakeCache.txt -o -name cmake_install.cmake -o -name build.ninja -o \
    -name .ninja_deps -o -name .ninja_log \) -print)
if [[ -n "$forbidden_files" ]]; then
    printf 'Compiled or downloaded artifacts are forbidden in the source export:\n%s\n' "$forbidden_files" >&2
    exit 1
fi

forbidden_directories=$(find "$export_root" -type d \( \
    -name build -o -name '_build' -o -name 'cmake-build-*' -o -name CMakeFiles -o \
    -name .buildtool -o \
    -name '*.dSYM' -o -name .compiled-backend-cache \) -print)
if [[ -n "$forbidden_directories" ]]; then
    printf 'Build or provider-cache directories are forbidden in the source export:\n%s\n' "$forbidden_directories" >&2
    exit 1
fi

machine_paths=$(grep -a -R -n -E '(/Users/[^/]+/|/home/[^/]+/|/private/tmp/|/private/var/folders/|/tmp/|[A-Za-z]:\\+Users\\+[^\\]+\\+)' "$export_root" || true)
if [[ -n "$machine_paths" ]]; then
    printf 'Machine-specific paths are forbidden in the source export:\n%s\n' "$machine_paths" >&2
    exit 1
fi

before_hash=$(tar -C "$export_root" -cf - . | git hash-object --stdin)
(
    cd "$working_directory"
    cmake -S "$export_root" -B "$build_root" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=ON \
        -DALONGTRACK_BUILD_WAVEVORTEX_EXTENSION=ON \
        -DALONGTRACK_WAVEVORTEX_SOURCE_DIR="$wavevortex_root" \
        -DALONGTRACK_WARNINGS_AS_ERRORS=ON
    cmake --build "$build_root" --parallel
    ctest --test-dir "$build_root" --output-on-failure
)

runner=$(find "$build_root" -type f \( -name alongtrack-wave-vortex-run -o -name alongtrack-wave-vortex-run.exe \) -print -quit)
if [[ -z "$runner" ]]; then
    printf 'The exported build did not produce alongtrack-wave-vortex-run.\n' >&2
    exit 1
fi
set +e
(cd "$working_directory" && "$runner" >/dev/null 2>&1)
runner_status=$?
set -e
if [[ $runner_status -ne 2 ]]; then
    printf 'The exported runner usage path returned %d instead of 2.\n' "$runner_status" >&2
    exit 1
fi

after_hash=$(tar -C "$export_root" -cf - . | git hash-object --stdin)
if [[ "$before_hash" != "$after_hash" ]]; then
    printf 'Configuring or testing mutated the exported source tree.\n' >&2
    exit 1
fi

printf 'Verified source-only AlongTrack export: %s\n' "$export_root"
printf 'WaveVortexModel source revision: %s\n' "$(git -C "$wavevortex_root" rev-parse HEAD)"
printf 'Runner: %s\n' "$runner"
