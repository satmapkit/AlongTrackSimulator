function generate_portable_core_reference()
% Generate the MATLAB-authoritative fixture consumed by the portable C++ parity tests.

repositoryRoot = fileparts(fileparts(mfilename('fullpath')));
fixturePath = fullfile(repositoryRoot,"cpp","tests","data","matlab_reference.csv");
fileID = fopen(fixturePath,"w");
if fileID == -1
    error("AlongTrackSimulator:ReferenceFixtureOpenFailed","Could not open '%s' for writing.",fixturePath);
end
fileCleanup = onCleanup(@() fclose(fileID));
fprintf(fileID,"kind,name,input0,input1,input2,expected0,expected1,expected2,expected3\n");

ats = AlongTrackSimulator();
for mission = sort(ats.missions()).'
    parameters = ats.missionParameters(mission);
    fprintf(fileID,"mission,%s,0,0,0,%.17g,%.17g,%.17g,%.17g\n",mission,parameters.semi_major_axis,ats.orbitalPeriodForMissionWithName(mission),ats.nodalPeriodForMissionWithName(mission),ats.repeatCycleForMissionWithName(mission));
    fprintf(fileID,"orbit,%s,0,0,0,%.17g,%.17g,%.17g,%.17g\n",mission,parameters.eccentricity,parameters.inclination,parameters.longitude_at_equator,parameters.passes_per_cycle);
end

groundTrackTimes = [0,1,1234.5,86400,31536000];
for mission = ["j3","alg"]
    [latitude,longitude] = ats.groundTrackForMissionWithName(mission,time=groundTrackTimes);
    for iTime = 1:numel(groundTrackTimes)
        fprintf(fileID,"ground,%s,%.17g,0,0,%.17g,%.17g,NaN,NaN\n",mission,groundTrackTimes(iTime),latitude(iTime),longitude(iTime));
    end
end

circularTimes = [0,1,1234.5,7000];
[latitude,longitude] = AlongTrackSimulator.computeGroundTrackCircularOrbit(7000,0,1.1,0,0,0.2,circularTimes);
for iTime = 1:numel(circularTimes)
    fprintf(fileID,"circular,none,%.17g,7000,1.1,%.17g,%.17g,NaN,NaN\n",circularTimes(iTime),latitude(iTime),longitude(iTime));
end

projectionCases = [0,0.25,0; 24,-122.75,-123; -45,12,10; 70,179.8,179.9];
for iCase = 1:size(projectionCases,1)
    sourceLatitude = projectionCases(iCase,1);
    sourceLongitude = projectionCases(iCase,2);
    centralMeridian = projectionCases(iCase,3);
    [x,y] = AlongTrackSimulator.LatitudeLongitudeToTransverseMercator(sourceLatitude,sourceLongitude,lon0=centralMeridian);
    [latitude,longitude] = AlongTrackSimulator.TransverseMercatorToLatitudeLongitude(x,y,lon0=centralMeridian);
    fprintf(fileID,"projection,none,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n",sourceLatitude,sourceLongitude,centralMeridian,x,y,latitude,longitude);
end

for mission = ["j3","alg"]
    alongtrack = ats.projectedPointsForMissionWithName(mission,Lx=3e6,Ly=3e6,lat0=0,lon0=0,time=(0:30000));
    passes = AlongTrackSimulator.convertAlongTrackStructureToPass(alongtrack);
    triggerTimes = cellfun(@(pass) pass.t(1),passes);
    if isscalar(triggerTimes)
        finalTrigger = NaN;
    else
        finalTrigger = triggerTimes(end);
    end
    fprintf(fileID,"window,%s,0,30000,0,%d,%d,%.17g,%.17g\n",mission,numel(alongtrack.t),numel(passes),triggerTimes(1),finalTrigger);
    fprintf(fileID,"window_first,%s,%.17g,0,0,%.17g,%.17g,NaN,NaN\n",mission,alongtrack.t(1),alongtrack.x(1),alongtrack.y(1));
    fprintf(fileID,"window_last,%s,%.17g,0,0,%.17g,%.17g,NaN,NaN\n",mission,alongtrack.t(end),alongtrack.x(end),alongtrack.y(end));
    for iPass = 1:numel(passes)
        pass = passes{iPass};
        fprintf(fileID,"pass,%s,%d,%d,0,%.17g,%.17g,%.17g,%.17g\n",mission,iPass,numel(pass.t),pass.t(1),pass.t(end),pass.x(end),pass.y(end));
    end
end

datelineLatitude = 66.0102829;
datelineTrack = ats.projectedPointsForMissionWithName("s6a",Lx=100e3,Ly=100e3,lat0=datelineLatitude,lon0=179.9,time=(3300:3400));
[~,datelineLongitudes] = ats.groundTrackForMissionWithName("s6a",time=datelineTrack.t);
fprintf(fileID,"dateline,s6a,3300,3400,%.17g,%d,%.17g,%.17g,%d\n",datelineLatitude,numel(datelineTrack.t),datelineTrack.t(1),datelineTrack.t(end),nnz(datelineLongitudes<0));
clear fileCleanup
end
