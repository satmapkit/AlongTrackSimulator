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
 
                         
  


## Topics
+ Initialization
  + [`AlongTrackSimulator`](/AlongTrackSimulator/classes/alongtracksimulator/alongtracksimulator.html) Create an AlongTrackSimulator instance.
+ Constant orbital parameters
  + [`J2`](/AlongTrackSimulator/classes/alongtracksimulator/j2.html) Earth's second zonal harmonic coefficient J2.
  + [`RE`](/AlongTrackSimulator/classes/alongtracksimulator/re.html) Earth's equatorial radius (km).
  + [`T_sidereal`](/AlongTrackSimulator/classes/alongtracksimulator/t_sidereal.html) Length of a sidereal day (seconds).
  + [`mu`](/AlongTrackSimulator/classes/alongtracksimulator/mu.html) Earth's standard gravitational parameter μ ($$km^3/s^2$$).
+ Mission catalog
  + [`currentMissions`](/AlongTrackSimulator/classes/alongtracksimulator/currentmissions.html) List mission keys for currently operating satellites.
  + [`missionParameters`](/AlongTrackSimulator/classes/alongtracksimulator/missionparameters.html) Mission parameter dictionary keyed by mission abbreviation.
  + [`missions`](/AlongTrackSimulator/classes/alongtracksimulator/missions.html) List all mission keys in the catalog.
  + [`summarizeMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/summarizemissionwithname.html) Display a table summarizing one or more missions.
  + Fetching parameters
    + [`nodalPeriodForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/nodalperiodformissionwithname.html) Return the nodal period for a mission (including J2 nodal precession).
    + [`orbitalPeriodForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/orbitalperiodformissionwithname.html) Return the orbital period for a mission.
    + [`repeatCycleForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/repeatcycleformissionwithname.html) Return the mission repeat cycle length in seconds.
+ Ground tracks
  + [`groundTrackForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/groundtrackformissionwithname.html) Compute the ground track for a mission over one or more orbits.
  + [`projectedPointsForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/projectedpointsformissionwithname.html) Project mission ground-track points into a local Transverse Mercator box.
  + [`projectedPointsForRepeatMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/projectedpointsforrepeatmissionwithname.html) Project repeat-cycle ground-track points into a local Transverse Mercator box.
  + [`repeatGroundTrackForMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/repeatgroundtrackformissionwithname.html) Compute the full repeat-cycle ground track for a mission.
+ Utilities
  + Working with alongtrack data
    + [`convertAlongTrackStructureToPass`](/AlongTrackSimulator/classes/alongtracksimulator/convertalongtrackstructuretopass.html) Convert a flat alongtrack struct into a cell array of passes.
  + WaveVortexModel integration
    + [`wvmOutputGroupForRepeatMissionWithName`](/AlongTrackSimulator/classes/alongtracksimulator/wvmoutputgroupforrepeatmissionwithname.html) Build a WaveVortexModel output group for an along-track sampling pattern.
  + Computing orbital parameters
    + [`computeNodalPeriod`](/AlongTrackSimulator/classes/alongtracksimulator/computenodalperiod.html) Computes the nodal precession period from orbital parameters
  + Forcing exact repeat orbits
    + [`eccentricityForExactRepeatForMission`](/AlongTrackSimulator/classes/alongtracksimulator/eccentricityforexactrepeatformission.html) Return the eccentricity used in exact-repeat calculations.
    + [`inclinationForExactRepeatForMission`](/AlongTrackSimulator/classes/alongtracksimulator/inclinationforexactrepeatformission.html) Compute inclination required for an exact repeat orbit for the mission.
    + [`j2ForExactRepeatForMission`](/AlongTrackSimulator/classes/alongtracksimulator/j2forexactrepeatformission.html) Return the J2 value used in exact-repeat calculations.
    + [`semimajorAxisForExactRepeatForMission`](/AlongTrackSimulator/classes/alongtracksimulator/semimajoraxisforexactrepeatformission.html) Solve for semi-major axis that yields an exact repeat cycle for the mission.
  + Groundtrack algorithms
    + [`computeGroundTrack`](/AlongTrackSimulator/classes/alongtracksimulator/computegroundtrack.html) computes the ground track of a satellite given including the nodal precession (unoptimized version)
    + [`computeGroundTrackCircularOrbit`](/AlongTrackSimulator/classes/alongtracksimulator/computegroundtrackcircularorbit.html) computes the ground track assuming a circular orbit
    + [`computeGroundTrackWithNodalPrecession`](/AlongTrackSimulator/classes/alongtracksimulator/computegroundtrackwithnodalprecession.html) computes the ground track of a satellite given including the nodal precession
    + [`computeGroundTrackWithNodalPrecessionSimple`](/AlongTrackSimulator/classes/alongtracksimulator/computegroundtrackwithnodalprecessionsimple.html) computes the ground track of a satellite given including the nodal precession (unoptimized version)
  + Solving Kepler's equation
    + [`kepler1`](/AlongTrackSimulator/classes/alongtracksimulator/kepler1.html) Solve Kepler's equation for eccentric anomaly E.
    + [`kepler2`](/AlongTrackSimulator/classes/alongtracksimulator/kepler2.html) Solve Kepler's equation for eccentric anomaly E.
    + [`kepler2vec`](/AlongTrackSimulator/classes/alongtracksimulator/kepler2vec.html) Solve Kepler's equation for eccentric anomaly E (fastest version)
    + [`kepler3`](/AlongTrackSimulator/classes/alongtracksimulator/kepler3.html) Solve Kepler's equation for eccentric anomaly E.
    + [`kepler4`](/AlongTrackSimulator/classes/alongtracksimulator/kepler4.html) Solve Kepler's equation for eccentric anomaly E.
    + [`kepler5`](/AlongTrackSimulator/classes/alongtracksimulator/kepler5.html) Solve Kepler's equation for eccentric anomaly E.
  + Mission catalog
    + [`missionParametersCatalog`](/AlongTrackSimulator/classes/alongtracksimulator/missionparameterscatalog.html) Create a dictionary for satellite altimetry missions
+ Other
  + [`InverseMeridionalArcPROJ4`](/AlongTrackSimulator/classes/alongtracksimulator/inversemeridionalarcproj4.html) These are the *defined* values for WGS84
  + [`LatitudeLongitudeBoundsForTransverseMercatorBox`](/AlongTrackSimulator/classes/alongtracksimulator/latitudelongitudeboundsfortransversemercatorbox.html) 
  + [`LatitudeLongitudeToTransverseMercator`](/AlongTrackSimulator/classes/alongtracksimulator/latitudelongitudetotransversemercator.html) 
  + [`MeridionalArcPROJ4`](/AlongTrackSimulator/classes/alongtracksimulator/meridionalarcproj4.html) These are the *defined* values for WGS84
  + [`TransverseMercatorToLatitudeLongitude`](/AlongTrackSimulator/classes/alongtracksimulator/transversemercatortolatitudelongitude.html) 
  + [`projectedPointsForReferenceOrbit`](/AlongTrackSimulator/classes/alongtracksimulator/projectedpointsforreferenceorbit.html) 


---