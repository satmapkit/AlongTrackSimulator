---
layout: default
title: Home
nav_order: 1
description: "Ground track simulator for altimetry missions"
permalink: /
---

## A ground track simulator for satellite altimetry missions

<img src="figure/groundtracks_splash.jpgl" alt="Ground tracks of two satellites for 5 orbital periods" width="300">

The AlongTrack Simulator returns the ground track sampling pattern for all of the current and historical altimetry missions. 


The figure above is reproduced with only a few lines of code,
```matlab
ats = AlongTrackSimulator();

fig = figure(Name="Ground tracks",Position=[100 100 500 500]);
[lat,lon,~] = ats.groundTrackForMissionWithName("s6a",N_orbits=5);
geoscatter(lat,lon,1.5^2,"blue","filled"), hold on
[lat,lon,~] = ats.groundTrackForMissionWithName("swon",N_orbits=5);
geoscatter(lat,lon,1.5^2,"red","filled")
legend(ats.missionParameters('s6a').name,ats.missionParameters('swon').name,Location="southeast")

geobasemap landcover
```