---
layout: default
title: Installation
nav_order: 2
description: Installation instructions"
permalink: /installation
---

## Installation

You may install this as part of [OceanKit](https://github.com/JeffreyEarly/OceanKit), or directly from [the github repo](https://github.com/satmapkit/AlongTrackSimulator) using the command-line,
```
git clone https://github.com/satmapkit/AlongTrackSimulator.git
```
to clone the repo, and then installing package from within MATLAB
```matlab
mpminstall("local/path/to/AlongTrackSimulator");
```
with authoring enabled.

By default the AlongTrackSimulator installs the WaveVortexModel as a dependency, however, unless you are running the model, you may simply wish to install the the simulator. In that case, run the installer without dependencies,
```matlab
mpminstall("local/path/to/AlongTrackSimulator", InstallDependencies=false);
```
