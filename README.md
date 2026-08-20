# AlongTrack Simulator

A MATLAB ground-track simulator for satellite altimetry missions, with a source-linked portable C++ extension for WaveVortexModel.

AlongTrackSimulator samples numerical data with the ground-track patterns of real altimetry missions. MATLAB remains the scientific authority for mission authoring and parity; the optional portable implementation executes supported MATLAB-authored bundles through WaveVortexModel's ordinary reusable C++ runner.

See the [documentation website](https://satmapkit.github.io/AlongTrackSimulator/) for complete information on installation and usage.

## Installation

Install AlongTrackSimulator through [OceanKit](https://github.com/JeffreyEarly/OceanKit), or clone this repository:

```sh
git clone https://github.com/satmapkit/AlongTrackSimulator.git
```

From MATLAB, install the checkout in authoring mode:

```matlab
mpminstall("local/path/to/AlongTrackSimulator",Authoring=true);
```

## Portable WaveVortex runner

The optional `alongtrack-wave-vortex-run` executable is built from source by linking this repository with an explicitly selected WaveVortexModel source checkout. It registers WaveVortexModel's built-ins and the AlongTrack schedule/observer pair in one catalog, freezes that catalog once, and delegates to WaveVortexModel's reusable runner. See the [portable runner guide](https://satmapkit.github.io/AlongTrackSimulator/portable-wave-vortex-runner) for dependency selection, MATLAB bundle authoring, persistence policies, compatibility boundaries, and source-only verification.
