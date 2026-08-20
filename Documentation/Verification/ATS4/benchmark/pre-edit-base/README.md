# Frozen pre-edit WaveVortex baseline

These seven unmodified `wave-vortex-run-v1` reports were captured before the
ATS #4 worktree was edited. They freeze the immediately available reusable
runner and establish the pre-change reference independently of the final
matched base/extended comparison in the parent directory.

- WaveVortexModel revision: `84dc1b53093650fdbc9be62b637cf298c46a95d3`
- Base executable SHA-256: `3d8a9af6bc7ca295010d4f72b9726569140dffa947c760df8244e8fa31cb4cbb`
- Source fixture: `PortableRuntime/tests/fixtures/forcing-mixed-hydrostatic.nc`
- Source fixture SHA-256: `57b7c90655d5896d065ba41c2c7b24b9a1e083e5b1cb8abe4196282e3248e999`
- Build: Release, AppleClang 21.0.0, CMake 4.4.2, NetCDF C 4.9.3
- Execution: reference FFT, one thread, coefficient restart, fixed RK4,
  `deltaT=1e-6`, 200 steps, one unique replace destination per sample
- Host: `donut`, arm64 macOS 26.5.2 (Darwin 25.5.0)

The seven-sample medians were 1.142781917 s complete and 1.132721666 s in
integration. Exact full-model retained and maximum-live storage were both
92,989 B in every report; the immutable built-in catalog was 10,068 B. The
source fixture hash was checked before and after the series. Each output path
was a transient `/private/tmp/ats4-baseline-output-NN.nc` file; no benchmark
output is distributed.

The raw reports are in `raw/`. They have not been normalized or rewritten.
