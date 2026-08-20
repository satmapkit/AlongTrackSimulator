# ATS #4 matched runner benchmark

This directory contains the frozen MATLAB-authored built-in output-graph
fixture and byte-identical run request used for the ATS #4 acceptance
comparison. Exact unmodified reports are retained in the authoring checkout at
`Documentation/Verification/ATS4/benchmark`; they are deliberately excluded
from the source-only MATLAB package because a raw runner report records its
absolute transient paths.

## Frozen inputs and environment

- AlongTrackSimulator base revision:
  `bf6f8299ed2fa64163e256ecf2e29e8c9c1588ce`
- WaveVortexModel revision:
  `84dc1b53093650fdbc9be62b637cf298c46a95d3`
- Base executable SHA-256:
  `3d8a9af6bc7ca295010d4f72b9726569140dffa947c760df8244e8fa31cb4cbb`
- Extended executable SHA-256:
  `4e81048a4bb68e384385b35b193e2b48cb648ec9c70b5b95e5f14ca98deb07da`
- `matlab-authored-builtin-output.nc` SHA-256:
  `40c1925a39a0b4d298dc2a33b2174b8791ad4de8521ddfe88ea4b4ee640f2626`
- `matlab-authored-builtin-output-request.json` SHA-256:
  `58b2c9c0a282a97ef9298e26d5f6d8c3e4bc5c40b05dbf8f370327e08deb87f2`
- Build: Release, AppleClang 21.0.0, CMake 4.4.2, NetCDF C 4.9.3
- Host: `donut`, arm64 macOS 26.5.2 (Darwin 25.5.0)
- Execution: reference FFT, one thread, fixed RK4, `initialStep=1e-6`,
  `finalTime=2e-4`, 200 accepted steps, 800 right-hand-side evaluations,
  create policy, and no provider fallback

The fixture contains one built-in `WVModelOutputGroupEvenlySpaced` with a
built-in `WVEulerianFields` coefficient observer. It commits the initial
record and four integration-time occurrences, so the measurement includes a
real immutable schedule/observer graph, output plan, central evaluation
service, occurrence workspace, NetCDF sink, and output driver. The input hash
was unchanged after all runs.

After one excluded warm-up per executable, seven pairs ran serially. Odd pairs
ran base then extended; even pairs ran extended then base. Both executables
were invoked from the same working directory with the same request bytes and
the same source, destination, and report names:

```sh
wave-vortex-run --request matlab-authored-builtin-output-request.json
alongtrack-wave-vortex-run --request matlab-authored-builtin-output-request.json
```

Each completed report was copied to the authoring evidence tree before the
shared transient destination and report were removed for the other runner.

## Matched results

| Measurement | Base | Extended | Change |
| --- | ---: | ---: | ---: |
| Complete runtime median | 1.315276000 s | 1.315780416 s | +0.0383506% |
| Integration runtime median | 1.307509458 s | 1.307882583 s | +0.0285371% |
| Exact retained storage | 151,759 B | 152,136 B | +377 B (+0.248420%) |
| Exact maximum-live storage | 167,658 B | 168,035 B | +377 B (+0.224863%) |
| Immutable extension catalog | 10,068 B | 10,445 B | +377 B |

The complete-runtime and retained-storage regressions are both below the 3%
acceptance limit. Every storage and liveness component other than the catalog
is identical between executables.

The non-double-counted retained formula reported by the reusable runner is:

```text
known = model facade + catalog + checkpoint state + integration system
      + integrator + model-output configuration
full  = known + (model state - checkpoint state)
      + model-output evaluation + model-output sink
```

For the base/extended pair, model-output configuration is 9,943/9,943 B,
evaluation 12,101/12,101 B, sink 5,459/5,459 B, and the combined diagnostic
is 27,503/27,503 B. Configuration owns the immutable `WVOutputPlan`, shared
descriptor graph, built-in schedule and observer, continuation, and
destination progress. The occurrence workspace is 4,920 B retained and
5,753 B maximum-live. The driver is zero after its bounded lifetime and
15,066 B maximum-live; output orchestration is likewise 15,066 B
maximum-live. Integration-system, integrator-persistent, integrator-workspace,
model-facade, model-state, and checkpoint storage are respectively
87,010 B, 20,184 B, 19,200 B, 801 B, 6,193 B, and 4,800 B for both runners.

`outputPlanMaximumLive=0` and `scheduledOutput=0` name the separate legacy
explicit CLI-checkpoint path; they are not the portable model graph's plan or
schedule and are correctly zero in request mode. The model graph's plan and
schedule are included in `modelOutputConfiguration` above. Summary fields
such as `modelOutput` and occurrence-workspace diagnostics overlap their
owning components and are not added again.

All fourteen complete output files had identical dimensions, variables,
attributes other than volatile creation time, and persisted values. The
comparison normalized only the NetCDF display name and removed
`date_created`; every resulting dump had SHA-256
`4a164e072cc5a249dd7be98b886e6e4808e8a7e078cba1270755ae91497e1275`.

The separate pre-edit base capture is preserved with its seven exact reports
under `Documentation/Verification/ATS4/benchmark/pre-edit-base`. Its median
complete/integration times were 1.142781917/1.132721666 s and exact
retained/maximum-live storage was 92,989/92,989 B on the original canonical
non-output fixture.
