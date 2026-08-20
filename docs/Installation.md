---
layout: default
title: Installation
nav_order: 2
description: Installation instructions
permalink: /installation
---

# Installation

Install AlongTrackSimulator through [OceanKit](https://github.com/JeffreyEarly/OceanKit), or clone the authoring repository:

```sh
git clone https://github.com/satmapkit/AlongTrackSimulator.git
```

Install the checkout from MATLAB with authoring enabled:

```matlab
mpminstall("local/path/to/AlongTrackSimulator",Authoring=true);
```

The MATLAB package declares WaveVortexModel as a dependency. To use only the standalone orbit and projection calculations from a source checkout, installation without dependencies is also supported:

```matlab
mpminstall("local/path/to/AlongTrackSimulator",InstallDependencies=false,Authoring=true);
```

The optional portable C++ runner is not a downloaded package binary. It is built by source-linking this checkout with an explicitly selected WaveVortexModel source checkout. Continue with the [portable WaveVortex runner guide](portable-wave-vortex-runner).
