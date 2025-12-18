---
layout: default
title: WVModelOutputGroupAlongTrack
has_children: false
has_toc: false
mathjax: true
parent: Class documentation
nav_order: 3
---

#  WVModelOutputGroupAlongTrack

Represent WaveVortexModel output for satellite along-track sampling.


---

## Declaration

<div class="language-matlab highlighter-rouge"><div class="highlight"><pre class="highlight"><code>classdef WVModelOutputGroupAlongTrack < WVModelOutputGroup</code></pre></div></div>

## Overview
 
  WVModelOutputGroupAlongTrack manages the time sampling and NetCDF output for a
  single satellite altimetry mission using an AlongTrackSimulator. The group
  precomputes mission pass-overs through the model domain and, when the model
  reaches a pass-over time, writes the full along-track sample sequence for that
  pass-over into the corresponding NetCDF group.
 
  Typical usage:
  - Create a WVModel NetCDF output file and initialize an AlongTrackSimulator.
  - Construct one WVModelOutputGroupAlongTrack per mission and attach it to the output file.
 
  The following code adds output groups for all current satellites
  ```matlab
  outputFile = model.createNetCDFFileForModelOutput('ModelOutput.nc',outputInterval=86400);
  ats = AlongTrackSimulator();
  currentMissions = ats.currentMissions;
  for iMission = 1:length(currentMissions)
      outputFile.addOutputGroup(WVModelOutputGroupAlongTrack(model,currentMissions(iMission),ats));
  end
  ```
 
  Major responsibilities:
  - Store mission metadata and repeat-cycle information.
  - Determine pass-over output times for a model integration window.
  - Write the complete along-track time series for each pass-over into NetCDF.
  - Provide class annotation metadata for property introspection.
 
           
  


## Topics
+ Initialization
  + [`WVModelOutputGroupAlongTrack`](/AlongTrackSimulator/classes/wvmodeloutputgroupalongtrack/wvmodeloutputgroupalongtrack.html) Create an along-track output group for a satellite mission.
+ Mission metadata
  + [`ats`](/AlongTrackSimulator/classes/wvmodeloutputgroupalongtrack/ats.html) AlongTrackSimulator used to compute and project mission ground tracks.
  + [`description`](/AlongTrackSimulator/classes/wvmodeloutputgroupalongtrack/description.html) Describe the sampling pattern represented by this output group.
  + [`missionName`](/AlongTrackSimulator/classes/wvmodeloutputgroupalongtrack/missionname.html) Mission identifier used to configure the along-track sampling.
  + [`repeatCycle`](/AlongTrackSimulator/classes/wvmodeloutputgroupalongtrack/repeatcycle.html) Mission repeat cycle (s).
+ Output scheduling
  + [`convertTrackVectorToPassoverCellArray`](/AlongTrackSimulator/classes/wvmodeloutputgroupalongtrack/converttrackvectortopassovercellarray.html) Convert a track time series into a cell array of pass-overs.
  + [`firstPassoverTime`](/AlongTrackSimulator/classes/wvmodeloutputgroupalongtrack/firstpassovertime.html) Model time of first sample for each pass-over (s).
  + [`tracks`](/AlongTrackSimulator/classes/wvmodeloutputgroupalongtrack/tracks.html) Pass-over tracks through the model domain.
+ Class annotations
  + [`classRequiredPropertyNames`](/AlongTrackSimulator/classes/wvmodeloutputgroupalongtrack/classrequiredpropertynames.html) Return names of required properties for this class.


---