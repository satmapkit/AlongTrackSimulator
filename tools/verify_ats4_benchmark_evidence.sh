#!/usr/bin/env bash
set -euo pipefail

if [[ $# -gt 1 ]]; then
    printf 'usage: %s [AlongTrack source root]\n' "$0" >&2
    exit 2
fi

if [[ $# -eq 1 ]]; then
    source_root=$(cd "$1" && pwd -P)
else
    source_root=$(cd "$(dirname "$0")/.." && pwd -P)
fi

final_root="$source_root/Documentation/Verification/ATS4/benchmark/final-matched"
baseline_root="$source_root/Documentation/Verification/ATS4/benchmark/pre-edit-base"
fixture_root="$source_root/cpp/tests/data/ats4/benchmark"

hash_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

verify_manifest() {
    local root=$1
    local manifest="$root/SHA256SUMS"
    if [[ ! -f "$manifest" ]]; then
        printf 'Missing ATS #4 evidence manifest: %s\n' "$manifest" >&2
        exit 1
    fi
    while read -r expected relative_path; do
        local report="$root/$relative_path"
        if [[ ! -f "$report" ]]; then
            printf 'Missing ATS #4 raw report: %s\n' "$report" >&2
            exit 1
        fi
        local actual
        actual=$(hash_file "$report")
        if [[ "$actual" != "$expected" ]]; then
            printf 'ATS #4 raw report hash mismatch: %s\n' "$report" >&2
            exit 1
        fi
    done < "$manifest"
}

verify_manifest "$final_root"
verify_manifest "$baseline_root"

json_unsigned_integer() {
    local report=$1
    local field=$2
    awk -v marker="\"$field\"" '
        BEGIN { RS = "\034" }
        {
            position = index($0,marker)
            if (position == 0) exit 1
            value = substr($0,position + length(marker))
            sub(/^[[:space:]]*:[[:space:]]*/,"",value)
            if (match(value,/^[0-9]+/) == 0) exit 1
            print substr(value,RSTART,RLENGTH)
            exit
        }
    ' "$report"
}

json_nonnegative_number() {
    local report=$1
    local field=$2
    awk -v marker="\"$field\"" '
        BEGIN { RS = "\034" }
        {
            position = index($0,marker)
            if (position == 0) exit 1
            value = substr($0,position + length(marker))
            sub(/^[[:space:]]*:[[:space:]]*/,"",value)
            if (match(value,/^[0-9]+([.][0-9]+)?([eE][+-]?[0-9]+)?/) == 0) exit 1
            print substr(value,RSTART,RLENGTH)
            exit
        }
    ' "$report"
}

median_matched_field() {
    local runner_kind=$1
    local field=$2
    local report_index report value
    local values=()

    for report_index in 01 02 03 04 05 06 07; do
        report="$final_root/raw/$runner_kind-report-$report_index.json"
        if ! value=$(json_nonnegative_number "$report" "$field"); then
            printf 'Missing or invalid ATS #4 benchmark field %s: %s\n' "$field" "$report" >&2
            return 1
        fi
        values+=("$value")
    done

    printf '%s\n' "${values[@]}" | LC_ALL=C sort -n | sed -n '4p'
}

format_decimal() {
    local value=$1
    local digits=$2
    LC_ALL=C awk -v value="$value" -v digits="$digits" '
        BEGIN {
            format = "%." digits "f"
            printf format, value + 0
        }
    '
}

percent_change() {
    local base=$1
    local candidate=$2
    LC_ALL=C awk -v base="$base" -v candidate="$candidate" '
        BEGIN {
            if (base <= 0) exit 1
            printf "%.17g", 100 * (candidate - base) / base
        }
    '
}

require_value() {
    local label=$1
    local actual=$2
    local expected=$3
    if [[ "$actual" != "$expected" ]]; then
        printf 'ATS #4 %s mismatch: expected %s, computed %s\n' "$label" "$expected" "$actual" >&2
        exit 1
    fi
}

require_regression_within_limit() {
    local label=$1
    local base=$2
    local candidate=$3
    if ! LC_ALL=C awk -v base="$base" -v candidate="$candidate" '
            BEGIN {
                if (base <= 0) exit 1
                exit (100 * (candidate - base) / base <= 3.0) ? 0 : 1
            }
        '
    then
        printf 'ATS #4 %s regression exceeds 3%%: base=%s candidate=%s\n' \
            "$label" "$base" "$candidate" >&2
        exit 1
    fi
}

verify_storage_accounting() {
    local report=$1
    local checkpoint_state model_facade model_state extension_catalog
    local integration_system integrator_persistent output_configuration
    local output_evaluation output_sink model_output scheduled_output
    local known_persistent full_model_persistent output_driver_retained
    local output_driver_maximum output_plan_maximum orchestration_maximum
    local occurrence_storage occurrence_retained occurrence_maximum
    local known_retained known_maximum full_model_retained full_model_maximum

    checkpoint_state=$(json_unsigned_integer "$report" checkpointState)
    model_facade=$(json_unsigned_integer "$report" modelFacade)
    model_state=$(json_unsigned_integer "$report" modelState)
    extension_catalog=$(json_unsigned_integer "$report" extensionCatalog)
    integration_system=$(json_unsigned_integer "$report" integrationSystem)
    integrator_persistent=$(json_unsigned_integer "$report" integratorPersistent)
    output_configuration=$(json_unsigned_integer "$report" modelOutputConfiguration)
    output_evaluation=$(json_unsigned_integer "$report" modelOutputEvaluation)
    output_sink=$(json_unsigned_integer "$report" modelOutputSink)
    model_output=$(json_unsigned_integer "$report" modelOutput)
    scheduled_output=$(json_unsigned_integer "$report" scheduledOutput)
    known_persistent=$(json_unsigned_integer "$report" knownPersistent)
    full_model_persistent=$(json_unsigned_integer "$report" fullModelPersistent)
    output_driver_retained=$(json_unsigned_integer "$report" outputDriverRetained)
    output_driver_maximum=$(json_unsigned_integer "$report" outputDriverMaximumLive)
    output_plan_maximum=$(json_unsigned_integer "$report" outputPlanMaximumLive)
    orchestration_maximum=$(json_unsigned_integer "$report" outputOrchestrationMaximumLive)
    occurrence_storage=$(json_unsigned_integer "$report" occurrenceWorkspace)
    occurrence_retained=$(json_unsigned_integer "$report" occurrenceWorkspaceRetained)
    occurrence_maximum=$(json_unsigned_integer "$report" occurrenceWorkspaceMaximumLive)
    known_retained=$(json_unsigned_integer "$report" knownRetained)
    known_maximum=$(json_unsigned_integer "$report" knownMaximumLive)
    full_model_retained=$(json_unsigned_integer "$report" fullModelRetained)
    full_model_maximum=$(json_unsigned_integer "$report" fullModelMaximumLive)

    local state_excess=0
    if (( model_state > checkpoint_state )); then
        state_excess=$((model_state - checkpoint_state))
    fi
    local expected_known=$((model_facade + extension_catalog + checkpoint_state + integration_system + integrator_persistent + output_configuration + scheduled_output))
    local expected_full=$((expected_known + state_excess + output_evaluation + output_sink))
    local occurrence_peak=$occurrence_retained
    if (( occurrence_maximum > occurrence_peak )); then
        occurrence_peak=$occurrence_maximum
    fi
    local expected_maximum=$((expected_full - occurrence_retained + occurrence_peak + orchestration_maximum))

    if (( model_output != output_configuration + output_evaluation + output_sink ||
          known_persistent != expected_known ||
          full_model_persistent != expected_full ||
          output_driver_retained != 0 ||
          output_plan_maximum != 0 ||
          orchestration_maximum != output_driver_maximum ||
          occurrence_storage != occurrence_retained ||
          known_retained != expected_known ||
          known_maximum != expected_known + orchestration_maximum ||
          full_model_retained != expected_full ||
          full_model_maximum != expected_maximum )); then
        printf 'ATS #4 storage ownership or maximum-live accounting is inconsistent: %s\n' "$report" >&2
        exit 1
    fi
}

if [[ $(find "$final_root/raw" -maxdepth 1 -type f -name '*.json' | wc -l | tr -d ' ') -ne 14 ]]; then
    printf 'The matched ATS #4 evidence must contain exactly 14 reports.\n' >&2
    exit 1
fi
if [[ $(find "$baseline_root/raw" -maxdepth 1 -type f -name '*.json' | wc -l | tr -d ' ') -ne 7 ]]; then
    printf 'The pre-edit ATS #4 baseline must contain exactly seven reports.\n' >&2
    exit 1
fi

for report_index in 01 02 03 04 05 06 07; do
    for runner_kind in base candidate; do
        report="$final_root/raw/$runner_kind-report-$report_index.json"
        if [[ ! -f "$report" ]] || \
                ! grep -Fq '"schemaVersion":"wave-vortex-run-v1"' "$report" || \
                ! grep -Fq '"status":"complete"' "$report" || \
                ! grep -Fq '"request":{"active":true' "$report" || \
                ! grep -Fq '"id":"reference"' "$report" || \
                ! grep -Fq '"threads":1' "$report" || \
                ! grep -Fq '"stepCount":200' "$report" || \
                ! grep -Fq '"rhsEvaluationCount":800' "$report" || \
                ! grep -Fq '"modelOutputConfiguration":9943' "$report" || \
                ! grep -Fq '"modelOutputEvaluation":12101' "$report" || \
                ! grep -Fq '"modelOutputSink":5459' "$report" || \
                ! grep -Fq '"modelOutput":27503' "$report" || \
                ! grep -Fq '"occurrenceWorkspace":4920' "$report" || \
                ! grep -Fq '"outputDriverMaximumLive":15066' "$report" || \
                ! grep -Fq '"outputOrchestrationMaximumLive":15066' "$report"; then
            printf 'Incomplete active-output ATS #4 report: %s\n' "$report" >&2
            exit 1
        fi
        if [[ "$runner_kind" == base ]]; then
            expected_catalog=10068
            expected_retained=151759
            expected_maximum=167658
        else
            expected_catalog=10445
            expected_retained=152136
            expected_maximum=168035
        fi
        if ! grep -Fq "\"extensionCatalog\":$expected_catalog" "$report" || \
                ! grep -Fq "\"fullModelRetained\":$expected_retained" "$report" || \
                ! grep -Fq "\"fullModelMaximumLive\":$expected_maximum" "$report"; then
            printf 'ATS #4 report has unexpected exact storage: %s\n' "$report" >&2
            exit 1
        fi
        verify_storage_accounting "$report"
    done

    baseline_report="$baseline_root/raw/base-report-$report_index.json"
    if [[ ! -f "$baseline_report" ]] || \
            ! grep -Fq '"schemaVersion":"wave-vortex-run-v1"' "$baseline_report" || \
            ! grep -Fq '"status":"complete"' "$baseline_report" || \
            ! grep -Fq '"request":{"active":false' "$baseline_report" || \
            ! grep -Fq '"extensionCatalog":10068' "$baseline_report" || \
            ! grep -Fq '"fullModelRetained":92989' "$baseline_report" || \
            ! grep -Fq '"fullModelMaximumLive":92989' "$baseline_report"; then
        printf 'Incomplete pre-edit ATS #4 report: %s\n' "$baseline_report" >&2
        exit 1
    fi
    verify_storage_accounting "$baseline_report"
done

complete_base=$(median_matched_field base total)
complete_candidate=$(median_matched_field candidate total)
integrate_base=$(median_matched_field base integrate)
integrate_candidate=$(median_matched_field candidate integrate)
retained_base=$(median_matched_field base fullModelRetained)
retained_candidate=$(median_matched_field candidate fullModelRetained)
maximum_base=$(median_matched_field base fullModelMaximumLive)
maximum_candidate=$(median_matched_field candidate fullModelMaximumLive)

complete_change=$(percent_change "$complete_base" "$complete_candidate")
integrate_change=$(percent_change "$integrate_base" "$integrate_candidate")
retained_change=$(percent_change "$retained_base" "$retained_candidate")
maximum_change=$(percent_change "$maximum_base" "$maximum_candidate")

require_value 'complete-runtime base median' "$(format_decimal "$complete_base" 9)" '1.315276000'
require_value 'complete-runtime candidate median' "$(format_decimal "$complete_candidate" 9)" '1.315780416'
require_value 'complete-runtime change (percent)' "$(format_decimal "$complete_change" 9)" '0.038350582'
require_value 'integration-runtime base median' "$(format_decimal "$integrate_base" 9)" '1.307509458'
require_value 'integration-runtime candidate median' "$(format_decimal "$integrate_candidate" 9)" '1.307882583'
require_value 'integration-runtime change (percent)' "$(format_decimal "$integrate_change" 9)" '0.028537078'
require_value 'retained-storage base median' "$retained_base" '151759'
require_value 'retained-storage candidate median' "$retained_candidate" '152136'
require_value 'retained-storage change (percent)' "$(format_decimal "$retained_change" 6)" '0.248420'
require_value 'maximum-live base median' "$maximum_base" '167658'
require_value 'maximum-live candidate median' "$maximum_candidate" '168035'
require_value 'maximum-live change (percent)' "$(format_decimal "$maximum_change" 5)" '0.22486'

require_regression_within_limit 'complete-runtime median' "$complete_base" "$complete_candidate"
require_regression_within_limit 'integration-runtime median' "$integrate_base" "$integrate_candidate"
require_regression_within_limit 'exact retained-storage median' "$retained_base" "$retained_candidate"
require_regression_within_limit 'maximum-live-storage median' "$maximum_base" "$maximum_candidate"

summary_file="$fixture_root/README.md"
for expected_summary_row in \
        '| Complete runtime median | 1.315276000 s | 1.315780416 s | +0.0383506% |' \
        '| Integration runtime median | 1.307509458 s | 1.307882583 s | +0.0285371% |' \
        '| Exact retained storage | 151,759 B | 152,136 B | +377 B (+0.248420%) |' \
        '| Exact maximum-live storage | 167,658 B | 168,035 B | +377 B (+0.224863%) |'
do
    if ! grep -Fqx "$expected_summary_row" "$summary_file"; then
        printf 'ATS #4 benchmark summary does not match computed raw evidence: %s\n' \
            "$expected_summary_row" >&2
        exit 1
    fi
done

if [[ $(hash_file "$fixture_root/matlab-authored-builtin-output.nc") != \
        40c1925a39a0b4d298dc2a33b2174b8791ad4de8521ddfe88ea4b4ee640f2626 ]] || \
        [[ $(hash_file "$fixture_root/matlab-authored-builtin-output-request.json") != \
        58b2c9c0a282a97ef9298e26d5f6d8c3e4bc5c40b05dbf8f370327e08deb87f2 ]]; then
    printf 'The frozen ATS #4 benchmark input hashes changed.\n' >&2
    exit 1
fi

printf 'Verified exact ATS #4 pre-edit and matched benchmark evidence.\n'
