classdef TestPortableCoreParity < matlab.unittest.TestCase

    properties (SetAccess=private)
        ats
        reference
    end

    methods (TestClassSetup)
        function loadAuthoritativeReference(testCase)
            testCase.ats = AlongTrackSimulator();
            repositoryRoot = fileparts(fileparts(mfilename('fullpath')));
            fixturePath = fullfile(repositoryRoot,"cpp","tests","data","matlab_reference.csv");
            testCase.reference = readtable(fixturePath,TextType="string");
        end
    end

    methods (Test)
        function testResolvedMissionAndPeriodReference(testCase)
            rows = testCase.reference(testCase.reference.kind == "mission",:);
            for iRow = 1:height(rows)
                mission = rows.name(iRow);
                parameters = testCase.ats.missionParameters(mission);
                testCase.verifyEqual(parameters.semi_major_axis,rows.expected0(iRow),AbsTol=1e-12);
                testCase.verifyEqual(testCase.ats.orbitalPeriodForMissionWithName(mission),rows.expected1(iRow),AbsTol=1e-12);
                testCase.verifyEqual(testCase.ats.nodalPeriodForMissionWithName(mission),rows.expected2(iRow),AbsTol=1e-12);
                testCase.verifyEqual(testCase.ats.repeatCycleForMissionWithName(mission),rows.expected3(iRow),AbsTol=1e-9);
            end

            rows = testCase.reference(testCase.reference.kind == "orbit",:);
            for iRow = 1:height(rows)
                mission = rows.name(iRow);
                parameters = testCase.ats.missionParameters(mission);
                testCase.verifyEqual(parameters.eccentricity,rows.expected0(iRow),AbsTol=0);
                testCase.verifyEqual(parameters.inclination,rows.expected1(iRow),AbsTol=0);
                testCase.verifyEqual(parameters.longitude_at_equator,rows.expected2(iRow),AbsTol=0);
                testCase.verifyEqual(parameters.passes_per_cycle,rows.expected3(iRow),AbsTol=0);
            end
        end

        function testGroundTrackReference(testCase)
            rows = testCase.reference(testCase.reference.kind == "ground",:);
            for iRow = 1:height(rows)
                [latitude,longitude] = testCase.ats.groundTrackForMissionWithName(rows.name(iRow),time=rows.input0(iRow));
                testCase.verifyEqual(latitude,rows.expected0(iRow),AbsTol=1e-12);
                testCase.verifyEqual(longitude,rows.expected1(iRow),AbsTol=1e-12);
            end
        end

        function testCircularOrbitReference(testCase)
            rows = testCase.reference(testCase.reference.kind == "circular",:);
            for iRow = 1:height(rows)
                [latitude,longitude] = AlongTrackSimulator.computeGroundTrackCircularOrbit(rows.input1(iRow),0,rows.input2(iRow),0,0,0.2,rows.input0(iRow));
                testCase.verifyEqual(latitude,rows.expected0(iRow),AbsTol=1e-12);
                testCase.verifyEqual(longitude,rows.expected1(iRow),AbsTol=1e-12);
            end
        end

        function testProjectionReferenceAndBoundaries(testCase)
            rows = testCase.reference(testCase.reference.kind == "projection",:);
            for iRow = 1:height(rows)
                [x,y] = AlongTrackSimulator.LatitudeLongitudeToTransverseMercator(rows.input0(iRow),rows.input1(iRow),lon0=rows.input2(iRow));
                [latitude,longitude] = AlongTrackSimulator.TransverseMercatorToLatitudeLongitude(x,y,lon0=rows.input2(iRow));
                testCase.verifyEqual(x,rows.expected0(iRow),AbsTol=1e-9);
                testCase.verifyEqual(y,rows.expected1(iRow),AbsTol=1e-9);
                testCase.verifyEqual(latitude,rows.expected2(iRow),AbsTol=1e-12);
                testCase.verifyEqual(longitude,rows.expected3(iRow),AbsTol=1e-12);
            end

            Lx = 2e6;
            Ly = 1e6;
            lat0 = 24;
            lon0 = -123;
            [x0,y0] = AlongTrackSimulator.LatitudeLongitudeToTransverseMercator(lat0,lon0,lon0=lon0);
            [latitude,longitude] = AlongTrackSimulator.TransverseMercatorToLatitudeLongitude(x0+Lx/2,y0+Ly/2,lon0=lon0);
            [x,y] = AlongTrackSimulator.LatitudeLongitudeToTransverseMercator(latitude,longitude,lon0=lon0);
            testCase.verifyEqual(x,x0+Lx/2,AbsTol=0.05);
            testCase.verifyEqual(y,y0+Ly/2,AbsTol=0.05);
        end

        function testRepeatingAndGeodeticWindowReference(testCase)
            rows = testCase.reference(testCase.reference.kind == "window",:);
            for iRow = 1:height(rows)
                mission = rows.name(iRow);
                alongtrack = testCase.ats.projectedPointsForMissionWithName(mission,Lx=3e6,Ly=3e6,lat0=0,lon0=0,time=(rows.input0(iRow):rows.input1(iRow)));
                passes = AlongTrackSimulator.convertAlongTrackStructureToPass(alongtrack);
                triggerTimes = cellfun(@(pass) pass.t(1),passes);
                testCase.verifyNumElements(alongtrack.t,rows.expected0(iRow));
                testCase.verifyNumElements(passes,rows.expected1(iRow));
                testCase.verifyEqual(triggerTimes(1),rows.expected2(iRow),AbsTol=0);
                if isfinite(rows.expected3(iRow))
                    testCase.verifyEqual(triggerTimes(end),rows.expected3(iRow),AbsTol=0);
                end

                firstRow = testCase.reference(testCase.reference.kind == "window_first" & testCase.reference.name == mission,:);
                lastRow = testCase.reference(testCase.reference.kind == "window_last" & testCase.reference.name == mission,:);
                testCase.verifyEqual(alongtrack.t(1),firstRow.input0,AbsTol=0);
                testCase.verifyEqual(alongtrack.x(1),firstRow.expected0,AbsTol=1e-9);
                testCase.verifyEqual(alongtrack.y(1),firstRow.expected1,AbsTol=1e-9);
                testCase.verifyEqual(alongtrack.t(end),lastRow.input0,AbsTol=0);
                testCase.verifyEqual(alongtrack.x(end),lastRow.expected0,AbsTol=1e-9);
                testCase.verifyEqual(alongtrack.y(end),lastRow.expected1,AbsTol=1e-9);

                passRows = testCase.reference(testCase.reference.kind == "pass" & testCase.reference.name == mission,:);
                testCase.verifyEqual(height(passRows),numel(passes));
                for iPass = 1:numel(passes)
                    pass = passes{iPass};
                    passRow = passRows(passRows.input0 == iPass,:);
                    testCase.verifyEqual(numel(pass.t),passRow.input1,AbsTol=0);
                    testCase.verifyEqual(pass.t(1),passRow.expected0,AbsTol=0);
                    testCase.verifyEqual(pass.t(end),passRow.expected1,AbsTol=0);
                    testCase.verifyEqual(pass.x(end),passRow.expected2,AbsTol=1e-9);
                    testCase.verifyEqual(pass.y(end),passRow.expected3,AbsTol=1e-9);
                end
            end
        end

        function testEmptyWindowProducesNoPasses(testCase)
            alongtrack = testCase.ats.projectedPointsForMissionWithName("alg",Lx=1e3,Ly=1e3,lat0=0,lon0=0,time=(0:600));
            passes = AlongTrackSimulator.convertAlongTrackStructureToPass(alongtrack);
            outputGroupPasses = WVModelOutputGroupAlongTrack.convertTrackVectorToPassoverCellArray(alongtrack);
            testCase.verifyEmpty(alongtrack.t);
            testCase.verifyEmpty(passes);
            testCase.verifySize(passes,[0 1]);
            testCase.verifyEmpty(outputGroupPasses);
            testCase.verifySize(outputGroupPasses,[0 1]);
        end

        function testLongitudeWrappingAtAntimeridian(testCase)
            latitude = [0;0];
            longitude = [179.9;-179.9];
            [x,y] = AlongTrackSimulator.LatitudeLongitudeToTransverseMercator(latitude,longitude,lon0=179.9);
            testCase.verifyLessThan(abs(x(2)),30e3);
            [roundTripLatitude,roundTripLongitude] = AlongTrackSimulator.TransverseMercatorToLatitudeLongitude(x,y,lon0=179.9);
            testCase.verifyEqual(roundTripLatitude,latitude,AbsTol=1e-12);
            testCase.verifyEqual(roundTripLongitude,longitude,AbsTol=1e-12);

            [~,~,~,minLongitude,~,maxLongitude] = AlongTrackSimulator.LatitudeLongitudeBoundsForTransverseMercatorBox(lat0=0,lon0=179.9,Lx=100e3,Ly=100e3);
            testCase.verifyGreaterThan(maxLongitude-minLongitude,180);

            row = testCase.reference(testCase.reference.kind == "dateline",:);
            alongtrack = testCase.ats.projectedPointsForMissionWithName(row.name,Lx=100e3,Ly=100e3,lat0=row.input2,lon0=179.9,time=(row.input0:row.input1));
            [~,longitudes] = testCase.ats.groundTrackForMissionWithName(row.name,time=alongtrack.t);
            testCase.verifyNumElements(alongtrack.t,row.expected0);
            testCase.verifyEqual(alongtrack.t(1),row.expected1,AbsTol=0);
            testCase.verifyEqual(alongtrack.t(end),row.expected2,AbsTol=0);
            testCase.verifyEqual(nnz(longitudes<0),row.expected3,AbsTol=0);
            testCase.verifyTrue(any(longitudes<0) && any(longitudes>0));

            referenceOrbit = ATSTestReferenceOrbit();
            referenceTrack = referenceOrbit.projectedPointsForReferenceOrbit(Lx=100e3,Ly=100e3,lat0=0,lon0=179.9);
            testCase.verifyNumElements(referenceTrack.time,2);
        end

        function testLongGeodeticWorkingWindow(testCase)
            requestedTimes = (0:60:30*86400);
            alongtrack = testCase.ats.projectedPointsForMissionWithName("alg",Lx=3e6,Ly=3e6,lat0=0,lon0=0,time=requestedTimes);
            testCase.verifyLessThanOrEqual(numel(alongtrack.t),numel(requestedTimes));
            testCase.verifyGreaterThanOrEqual(alongtrack.t(1),requestedTimes(1));
            testCase.verifyLessThanOrEqual(alongtrack.t(end),requestedTimes(end));
        end
    end
end
