---
layout: default
title: AlongTrackSimulator
has_children: false
has_toc: false
mathjax: true
parent: Class documentation
nav_order: 1
---

#  AlongTrackSimulator

Simulate and manipulate satellite along-track ground tracks for altimetry missions.


---

## Declaration

<div class="language-matlab highlighter-rouge"><div class="highlight"><pre class="highlight"><code>classdef AlongTrackSimulator < AlongTrackSimulatorBase</code></pre></div></div>

## Overview
 
  AlongTrackSimulator provides utilities to compute orbit ground tracks (optionally with
  nodal precession), enforce exact repeat cycles for supported missions, and project
  tracks into a local Cartesian box for along-track sampling and modeling workflows.
 
      - Properties missionParameters: mission parameter catalog (containers.Map)
  - Properties (Constant) mu, J2, RE, T_sidereal: physical constants used by orbital routines


## Topics
+ AlongTrack
+ Initialization
  + [`AlongTrackSimulator`](/AlongTrackSimulator/classes/alongtracksimulator/alongtracksimulator.html) Create an AlongTrackSimulator instance.
+ Orbital mechanics
  + [`J2`](/AlongTrackSimulator/classes/alongtracksimulator/j2.html) Earth's second zonal harmonic coefficient J2.
  + [`RE`](/AlongTrackSimulator/classes/alongtracksimulator/re.html) Earth's equatorial radius (km).
  + [`T_sidereal`](/AlongTrackSimulator/classes/alongtracksimulator/t_sidereal.html) Length of a sidereal day (seconds).
  + [`kepler1`](/AlongTrackSimulator/classes/alongtracksimulator/kepler1.html) Solve Kepler's equation for eccentric anomaly E.
  + [`kepler2`](/AlongTrackSimulator/classes/alongtracksimulator/kepler2.html) Solve Kepler's equation for eccentric anomaly E.
  + [`kepler2vec`](/AlongTrackSimulator/classes/alongtracksimulator/kepler2vec.html) Solve Kepler's equation for eccentric anomaly E.
  + [`kepler3`](/AlongTrackSimulator/classes/alongtracksimulator/kepler3.html) Solve Kepler's equation for eccentric anomaly E.
  + [`kepler4`](/AlongTrackSimulator/classes/alongtracksimulator/kepler4.html) Solve Kepler's equation for eccentric anomaly E.
  + [`kepler5`](/AlongTrackSimulator/classes/alongtracksimulator/kepler5.html) Solve Kepler's equation for eccentric anomaly E.
  + [`mu`](/AlongTrackSimulator/classes/alongtracksimulator/mu.html) Earth's standard gravitational parameter μ (km^3/s^2).
+ Groundtrack Algorithms
  + [`computeGroundTrack`](/AlongTrackSimulator/classes/alongtracksimulator/computegroundtrack.html) computes the ground track (latitude and longitude) of a satellite given
  + [`computeGroundTrackCircularOrbit`](/AlongTrackSimulator/classes/alongtracksimulator/computegroundtrackcircularorbit.html) computes the ground track (latitude and longitude) of a satellite given
  + [`computeGroundTrackWithNodalPrecession`](/AlongTrackSimulator/classes/alongtracksimulator/computegroundtrackwithnodalprecession.html) computes the ground track (latitude and longitude) of a satellite given
  + [`computeGroundTrackWithNodalPrecessionSimple`](/AlongTrackSimulator/classes/alongtracksimulator/computegroundtrackwithnodalprecessionsimple.html) computes the ground track (latitude and longitude) of a satellite given
+ Other
  + [`computeNodalPeriod`](/AlongTrackSimulator/classes/alongtracksimulator/computenodalperiod.html) COMPUTE_NODAL_PERIOD Computes the nodal precession period
+ Along-track sampling
  + [`convertAlongTrackStructureToPass`](/AlongTrackSimulator/classes/alongtracksimulator/convertalongtrackstructuretopass.html) Convert a flat alongtrack struct into a cell array of passes.
  + [`projectedPointsForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/projectedpointsformissionwithname.html) Project mission ground-track points into a local Transverse Mercator box.
  + [`projectedPointsForRepeatMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/projectedpointsforrepeatmissionwithname.html) Project repeat-cycle ground-track points into a local Transverse Mercator box.
+ Mission catalog
  + [`currentMissions`](/AlongTrackSimulator/classes/alongtracksimulator/currentmissions.html) List missions whose end_date is infinite (assumed currently operating).
  + [`missionParameters`](/AlongTrackSimulator/classes/alongtracksimulator/missionparameters.html) Mission parameter catalog keyed by mission abbreviation.
  + [`missionParametersCatalog`](/AlongTrackSimulator/classes/alongtracksimulator/missionparameterscatalog.html) Create a dictionary for satellite altimetry missions
  + [`missions`](/AlongTrackSimulator/classes/alongtracksimulator/missions.html) List all mission keys in the catalog.
  + [`summarizeMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/summarizemissionwithname.html) Display a table summarizing one or more missions.
+ Exact repeat orbits
  + [`eccentricityForExactRepeatForMission`](/AlongTrackSimulator/classes/alongtracksimulator/eccentricityforexactrepeatformission.html) Return the eccentricity used in exact-repeat calculations.
  + [`inclinationForExactRepeatForMission`](/AlongTrackSimulator/classes/alongtracksimulator/inclinationforexactrepeatformission.html) Compute inclination required for an exact repeat orbit for the mission.
  + [`j2ForExactRepeatForMission`](/AlongTrackSimulator/classes/alongtracksimulator/j2forexactrepeatformission.html) Return the J2 value used in exact-repeat calculations.
  + [`semimajorAxisForExactRepeatForMission`](/AlongTrackSimulator/classes/alongtracksimulator/semimajoraxisforexactrepeatformission.html) Solve for semi-major axis that yields an exact repeat cycle for the mission.
+ Ground tracks
  + [`groundTrackForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/groundtrackformissionwithname.html) Compute the ground track for a mission over one or more orbits.
  + [`repeatGroundTrackForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/repeatgroundtrackformissionwithname.html) Compute the full repeat-cycle ground track for a mission.
+ Orbital periods
  + [`nodalPeriodForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/nodalperiodformissionwithname.html) Return the nodal period for a mission (including J2 nodal precession).
  + [`orbitalPeriodForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/orbitalperiodformissionwithname.html) Return the orbital period for a mission.
+ Repeat cycles
  + [`repeatCycleForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/repeatcycleformissionwithname.html) Return the mission repeat cycle length in seconds.
+ WaveVortexModel integration
  + [`wvmOutputGroupForRepeatMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/wvmoutputgroupforrepeatmissionwithname.html) Build a WaveVortexModel output group for an along-track sampling pattern.
+ Other
  + [`computeGroundTrackWithNodalPrecessionVectorized`](/AlongTrackSimulator/classes/alongtracksimulator/computegroundtrackwithnodalprecessionvectorized.html) 


---