classdef AlongTrackSimulator < AlongTrackSimulatorBase
    %UNTITLED Summary of this class goes here
    %   Detailed explanation goes here
    properties
        missionParameters
    end

    properties (Constant)
        mu = 398600.4418; % Earth's gravitational parameter (km^3/s^2)
        J2 = 1.08262668e-3;      % Earth's J2 coefficient
        RE = 6378.1363;        % Earth's equatorial radius (km)
        T_sidereal = 86164;
    end

    methods

        function self = AlongTrackSimulator()
            %Construct an AlongTrackSimulator with a mission catalog.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Setup
            % - Declaration: self = AlongTrackSimulator()
            % - Returns self: value
            self.missionParameters = AlongTrackSimulator.missionParametersCatalog();
            missions = self.missionParameters.keys;
            for i=1:numel(missions)
                % We force perfect repeat cycles by adjusting the semi-major axis.
                if isfinite(self.missionParameters(missions(i)).passes_per_cycle)
                    semi_major_axis = AlongTrackSimulator.semimajorAxisForExactRepeatForMission(missions(i));
                    % pctChange = abs(1-self.missionParameters(missions(i)).semi_major_axis/semi_major_axis)*100;
                    % disp("Changed semi-major axis from " + self.missionParameters(missions(i)).semi_major_axis + " to " + semi_major_axis + " (" + pctChange + "%) " + "for mission " + missions(i));
                    self.missionParameters(missions(i)).semi_major_axis = semi_major_axis;
                    T_orbit = self.orbitalPeriodForMissionWithName(missions(i));
                    self.missionParameters(missions(i)).repeat_cycle = T_orbit*self.missionParameters(missions(i)).passes_per_cycle/2/86400;
                end
            end
        end

        function [lat,lon,time] = groundTrackForMissionWithName(self,mission,options)
            %Compute the latitude/longitude ground track for a mission.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Ground tracks
            % - Declaration: [lat,lon,time] = groundTrackForMissionWithName(self,mission,options)
            % - Parameter self: AlongTrackSimulator
            % - Parameter mission: string | char
            % - Parameter options: struct
            % - Returns lat: value
            % - Returns lon: value
            % - Returns time: value
            arguments
                self AlongTrackSimulator
                mission {mustBeText}
                options.time
                options.N_orbits = 1
            end
            p = self.missionParameters(mission);

            T_orbit = 2*pi*sqrt((p.semi_major_axis)^3/self.mu);  % Orbital period [s]
            if isfield(options,"time")
                time = options.time;
            else
                dt = 1;                    % Time step [s]
                time = 0:dt:options.N_orbits*T_orbit;           % Time vector
            end
            time_shifted = time-T_orbit/4;

            % RAAN = pi/2;              % Right Ascension of Ascending Node [rad], or longitude of the ascending node
            % argPerigee = -pi/2;       % Argument of perigee [rad]. -pi/2 starts the orbit on the ascending track
            % M0 = pi/2;                   % Initial mean anomaly [rad]
            RAAN = p.longitude_at_equator;    % Right Ascension of Ascending Node [rad], or longitude of the ascending node
            argPeriapsis = 0;         % Argument of periapsis [rad].
            M0 = 0;                   % Initial mean anomaly [rad]
            % [lat, lon] = AlongTrackSimulator.computeGroundTrack(p.semi_major_axis, p.eccentricity, p.inclination, RAAN, argPeriapsis, M0, time_shifted);
            [lat, lon] = AlongTrackSimulator.computeGroundTrackWithNodalPrecession(p.semi_major_axis, p.eccentricity, p.inclination, RAAN, argPeriapsis, M0, time_shifted);
        end

        function [lat,lon,time] = repeatGroundTrackForMissionWithName(self,mission)
            %Compute one full repeat-cycle ground track for a repeat-orbit mission.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Repeat orbits
            % - Declaration: [lat,lon,time] = repeatGroundTrackForMissionWithName(self,mission)
            % - Parameter self: AlongTrackSimulator
            % - Parameter mission: string | char
            % - Returns lat: value
            % - Returns lon: value
            % - Returns time: value
            arguments
                self AlongTrackSimulator
                mission {mustBeText}
            end
            missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);
            N_orbits = p.passes_per_cycle/2;
            if isinf(N_orbits)
                error("The mission " + mission + " does not have a repeat cycle.")
            end
            [lat,lon,time] = self.groundTrackForMissionWithName(mission,N_orbits=N_orbits);
        end

        function T = orbitalPeriodForMissionWithName(self,mission)
            %Return the orbital period for a mission.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Periods
            % - Declaration: T = orbitalPeriodForMissionWithName(self,mission)
            % - Parameter self: AlongTrackSimulator
            % - Parameter mission: string | char
            % - Returns T: value
            arguments
                self AlongTrackSimulator
                mission {mustBeText}
            end
            a = self.missionParameters(mission).semi_major_axis;
            T = 2*pi*sqrt(a^3/self.mu);  % Orbital period [s]
        end

        function T = nodalPeriodForMissionWithName(self,mission)
            %Return the nodal (draconitic) period including J2 nodal precession effects.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Periods
            % - Declaration: T = nodalPeriodForMissionWithName(self,mission)
            % - Parameter self: AlongTrackSimulator
            % - Parameter mission: string | char
            % - Returns T: value
            arguments
                self AlongTrackSimulator
                mission {mustBeText}
            end
            p = self.missionParameters(mission);
            T = self.computeNodalPeriod(p.semi_major_axis,p.eccentricity, p.inclination);  % Orbital period [s]
        end

        function T = repeatCycleForMissionWithName(self,mission)
            %Return the repeat-cycle duration (seconds) for a mission if defined.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Repeat orbits
            % - Declaration: T = repeatCycleForMissionWithName(self,mission)
            % - Parameter self: AlongTrackSimulator
            % - Parameter mission: string | char
            % - Returns T: value
            arguments
                self AlongTrackSimulator
                mission {mustBeText}
            end
            p = self.missionParameters(mission);
            T = p.repeat_cycle*86400;
        end

        function missions = missions(self)
            %List all mission keys available in the mission catalog.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Missions
            % - Declaration: missions = missions(self)
            % - Parameter self: AlongTrackSimulator
            % - Returns missions: value
            arguments
                self AlongTrackSimulator
            end
            missions = self.missionParameters.keys;
        end

        function missions = currentMissions(self)
            %List missions that are active on the current date.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Missions
            % - Declaration: missions = currentMissions(self)
            % - Parameter self: AlongTrackSimulator
            % - Returns missions: value
            arguments
                self AlongTrackSimulator
            end
            allMissions = self.missionParameters.keys;
            missions = string.empty(0,0);
            for i=1:numel(allMissions)
                if isinf(self.missionParameters(allMissions(i)).end_date)
                    missions(end+1) = allMissions(i);
                end
            end
            missions = reshape(missions,[],1);
        end

        function summarizeMissionWithName(self,missionNames)
            %Display a summary table for one or more missions.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Missions
            % - Declaration: summarizeMissionWithName(self,missionNames)
            % - Parameter self: AlongTrackSimulator
            % - Parameter missionNames: string | char
            arguments
                self AlongTrackSimulator
                missionNames string
            end
            Name = strings(numel(missionNames),1);
            Abbreviation = strings(numel(missionNames),1);
            Start = strings(numel(missionNames),1);
            End = strings(numel(missionNames),1);
            RepeatCycle = strings(numel(missionNames),1);
            for i=1:numel(missionNames)
                Abbreviation(i) = missionNames(i);
                Name(i) = self.missionParameters(missionNames(i)).name;
                Start(i) = string(self.missionParameters(missionNames(i)).start_date);
                End(i) = string(self.missionParameters(missionNames(i)).end_date);
                RepeatCycle(i) = string(self.missionParameters(missionNames(i)).repeat_cycle);
            end
            T = table(Name,Abbreviation,Start,End,RepeatCycle);
            disp(T);
        end

        function alongtrack = projectedPointsForMissionWithName(self,missionName,requiredOptions,options)
            %Project mission ground-track points into a local tangent-plane window.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Projection
            % - Declaration: alongtrack = projectedPointsForMissionWithName(self,missionName,requiredOptions,options)
            % - Parameter self: AlongTrackSimulator
            % - Parameter missionName: string | char
            % - Parameter requiredOptions: struct
            % - Parameter options: struct
            % - Returns alongtrack: value
            arguments
                self AlongTrackSimulator
                missionName string
                requiredOptions.Lx
                requiredOptions.Ly
                requiredOptions.lat0
                requiredOptions.lon0
                options.time
                options.origin {mustBeMember(options.origin,{'lower-left','center'})} = 'lower-left'
            end
            optionsArgs = namedargs2cell(requiredOptions);
            [x0, y0, minLat, minLon, maxLat, maxLon] = self.LatitudeLongitudeBoundsForTransverseMercatorBox(optionsArgs{:});
            [lat,lon,time] = self.groundTrackForMissionWithName(missionName,time=options.time);

            % 1) apply crude filter
            withinBox = lat >= minLat & lat <= maxLat & lon >= minLon & lon <= maxLon;
            lat(~withinBox) = [];
            lon(~withinBox) = [];
            time(~withinBox) = [];

            % 2) project points
            lon0 = requiredOptions.lon0;
            Lx = requiredOptions.Lx;
            Ly = requiredOptions.Ly;
            [x,y] = AlongTrackSimulatorBase.LatitudeLongitudeToTransverseMercator(lat,lon,lon0=lon0);

            % 3) apply more precise filter
            out_of_bounds = (x < x0 - Lx/2) | (x > x0 + Lx/2) | (y < y0 - Ly/2) | (y > y0 + Ly/2);
            alongtrack.x = x(~out_of_bounds);
            alongtrack.y = y(~out_of_bounds)-y0;
            alongtrack.t = reshape(time(~out_of_bounds),[],1);

            [alongtrack.t,I] = sort(alongtrack.t);

            if options.origin == "lower-left"
                alongtrack.x = alongtrack.x(I) + Lx/2;
                alongtrack.y = alongtrack.y(I) + Ly/2;
            else
                alongtrack.x = alongtrack.x(I);
                alongtrack.y = alongtrack.y(I);
            end
        end

        
        function alongtrack = projectedPointsForRepeatMissionWithName(self,missionName,requiredOptions,options)
            %Project repeat-orbit ground-track points into a local window.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Projection
            % - Declaration: alongtrack = projectedPointsForRepeatMissionWithName(self,missionName,requiredOptions,options)
            % - Parameter self: AlongTrackSimulator
            % - Parameter missionName: string | char
            % - Parameter requiredOptions: struct
            % - Parameter options: struct
            % - Returns alongtrack: value
            arguments
                self AlongTrackSimulator
                missionName string
                requiredOptions.Lx
                requiredOptions.Ly
                requiredOptions.lat0
                requiredOptions.lon0
                options.origin {mustBeMember(options.origin,{'lower-left','center'})} = 'lower-left'
            end
            optionsArgs = namedargs2cell(requiredOptions);
            [x0, y0, minLat, minLon, maxLat, maxLon] = self.LatitudeLongitudeBoundsForTransverseMercatorBox(optionsArgs{:});
            [lat,lon,time] = self.repeatGroundTrackForMissionWithName(missionName);

            % 1) apply crude filter
            withinBox = lat >= minLat & lat <= maxLat & lon >= minLon & lon <= maxLon;
            lat(~withinBox) = [];
            lon(~withinBox) = [];
            time(~withinBox) = [];

            % 2) project points
            lon0 = requiredOptions.lon0;
            Lx = requiredOptions.Lx;
            Ly = requiredOptions.Ly;
            [x,y] = AlongTrackSimulatorBase.LatitudeLongitudeToTransverseMercator(lat,lon,lon0=lon0);

            % 3) apply more precise filter
            out_of_bounds = (x < x0 - Lx/2) | (x > x0 + Lx/2) | (y < y0 - Ly/2) | (y > y0 + Ly/2);
            alongtrack.x = x(~out_of_bounds);
            alongtrack.y = y(~out_of_bounds)-y0;
            alongtrack.t = reshape(time(~out_of_bounds),[],1);

            [alongtrack.t,I] = sort(alongtrack.t);

            if options.origin == "lower-left"
                alongtrack.x = alongtrack.x(I) + Lx/2;
                alongtrack.y = alongtrack.y(I) + Ly/2;
            else
                alongtrack.x = alongtrack.x(I);
                alongtrack.y = alongtrack.y(I);
            end
            alongtrack.repeatCycle = self.repeatCycleForMissionWithName(missionName);
        end

        % function alongtrack = projectedPointsForReferenceOrbit(self,options)
        %     arguments
        %         self AlongTrackSimulatorEmpirical
        %         options.Lx
        %         options.Ly
        %         options.lat0
        %         options.lon0
        %     end
        %     optionsArgs = namedargs2cell(options);
        %     [x0, y0, minLat, minLon, maxLat, maxLon] = AlongTrackSimulatorEmpirical.LatitudeLongitudeBoundsForTransverseMercatorBox(optionsArgs{:});
        %     [lat,lon,time] = self.pathForMission("s6a");
        %     withinBox = lat >= minLat & lat <= maxLat & lon >= minLon & lon <= maxLon;
        %     lat(~withinBox) = [];
        %     lon(~withinBox) = [];
        %     time(~withinBox) = [];
        %     use options;
        %     [x,y] = AlongTrackSimulatorEmpirical.LatitudeLongitudeToTransverseMercator(lat,lon,lon0=lon0);
        %     out_of_bounds = (x < x0 - Lx/2) | (x > x0 + Lx/2) | (y < y0 - Ly/2) | (y > y0 + Ly/2);
        %     alongtrack.x = x(~out_of_bounds);
        %     alongtrack.y = y(~out_of_bounds)-y0;
        %     alongtrack.time = time(~out_of_bounds);
        %     [alongtrack.time,I] = sort(alongtrack.time);
        %     alongtrack.x = alongtrack.x(I);
        %     alongtrack.y = alongtrack.y(I);
        % end
    end

    methods (Static)
        missionData = missionParametersCatalog();
        %missionParametersCatalog method.
        %
        % Concise, but complete description of this function and how to use it.
        %
        % - Topic: Along-track simulation
        % - Declaration: missionData = missionParametersCatalog()
        % - Returns missionData: value
        [lat, lon] = computeGroundTrack(altitude, e, incl, RAAN, argPerigee, M0, t)
        %computeGroundTrack method.
        %
        % Concise, but complete description of this function and how to use it.
        %
        % - Topic: Along-track simulation
        % - Declaration: [lat, lon] = computeGroundTrack(altitude, e, incl, RAAN, argPerigee, M0, t)
        % - Parameter altitude: value
        % - Parameter e: double
        % - Parameter incl: value
        % - Parameter RAAN: value
        % - Parameter argPerigee: value
        % - Parameter M0: value
        % - Parameter t: value
        % - Returns lat: value
        % - Returns lon: value
        [lat, lon] = computeGroundTrackWithNodalPrecession(semi_major_axis, e, incl, RAAN, omega, M0, t)
        %computeGroundTrackWithNodalPrecession method.
        %
        % Concise, but complete description of this function and how to use it.
        %
        % - Topic: Along-track simulation
        % - Declaration: [lat, lon] = computeGroundTrackWithNodalPrecession(semi_major_axis, e, incl, RAAN, omega, M0, t)
        % - Parameter semi_major_axis: value
        % - Parameter e: double
        % - Parameter incl: value
        % - Parameter RAAN: value
        % - Parameter omega: value
        % - Parameter M0: value
        % - Parameter t: value
        % - Returns lat: value
        % - Returns lon: value
        [lat, lon] = computeGroundTrackWithNodalPrecessionSimple(semi_major_axis, e, incl, RAAN, omega, M0, t)
        %computeGroundTrackWithNodalPrecessionSimple method.
        %
        % Concise, but complete description of this function and how to use it.
        %
        % - Topic: Along-track simulation
        % - Declaration: [lat, lon] = computeGroundTrackWithNodalPrecessionSimple(semi_major_axis, e, incl, RAAN, omega, M0, t)
        % - Parameter semi_major_axis: value
        % - Parameter e: double
        % - Parameter incl: value
        % - Parameter RAAN: value
        % - Parameter omega: value
        % - Parameter M0: value
        % - Parameter t: value
        % - Returns lat: value
        % - Returns lon: value
        [lat, lon] = computeGroundTrackWithNodalPrecessionVectorized(semi_major_axis, e, incl, RAAN, omega, M0, t)
        %computeGroundTrackWithNodalPrecessionVectorized method.
        %
        % Concise, but complete description of this function and how to use it.
        %
        % - Topic: Along-track simulation
        % - Declaration: [lat, lon] = computeGroundTrackWithNodalPrecessionVectorized(semi_major_axis, e, incl, RAAN, omega, M0, t)
        % - Parameter semi_major_axis: value
        % - Parameter e: double
        % - Parameter incl: value
        % - Parameter RAAN: value
        % - Parameter omega: value
        % - Parameter M0: value
        % - Parameter t: value
        % - Returns lat: value
        % - Returns lon: value
        T_nodal = computeNodalPeriod(a, e, i)
        %computeNodalPeriod method.
        %
        % Concise, but complete description of this function and how to use it.
        %
        % - Topic: Along-track simulation
        % - Declaration: T_nodal = computeNodalPeriod(a, e, i)
        % - Parameter a: value
        % - Parameter e: double
        % - Parameter i: value
        % - Returns T_nodal: value


        function outputGroup = wvmOutputGroupForRepeatMissionWithName(model, missionName)
            %Create a WVM output group / observing-system tracks for a repeat mission.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: WaveVortexModel — Observing system
            % - Declaration: outputGroup = wvmOutputGroupForRepeatMissionWithName(model, missionName)
            % - Parameter model: object
            % - Parameter missionName: string | char
            % - Returns outputGroup: value
            arguments
                model
                missionName string
            end
            ats = AlongTrackSimulator();
            wvt = model.wvt;
            alongtrack = ats.projectedPointsForRepeatMissionWithName(missionName,Lx=wvt.Lx,Ly=wvt.Ly,lat0=wvt.latitude,lon0=0);

            trackIndices = find(diff(alongtrack.t)>1);
            trackIndices(end+1) = length(alongtrack.t);
            startIndex = 1;
            tracks = cell(length(trackIndices),1);
            for i=1:length(trackIndices)
                endIndex = trackIndices(i);
                tracks{i}.x = alongtrack.x(startIndex:endIndex);
                tracks{i}.y = alongtrack.y(startIndex:endIndex);
                tracks{i}.t = alongtrack.t(startIndex:endIndex);
                startIndex = endIndex+1;
            end
            % figure
            % for iPassover=1:length(tracks)
            %     scatter(tracks{iPassover}.x/1e3,tracks{iPassover}.y/1e3), hold on
            % end
            repeatCycle = ats.repeatCycleForMissionWithName(missionName);
            outputGroup = WVModelOutputGroupAlongTrackRepeatCycle(model,missionName,tracks,repeatCycle);
        end

        % function addObservingSystemToModelForRepeatMissionWithName(model, missionName)
        %     outputGroup = self.wvmOutputGroupForRepeatMissionWithName(model, missionName);
        % 
        % end

        function tracks = convertAlongTrackStructureToPass(alongtrack)
            %Split an along-track time series into individual passes.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Along-track simulation — Post-processing
            % - Declaration: tracks = convertAlongTrackStructureToPass(alongtrack)
            % - Parameter alongtrack: struct
            % - Returns tracks: value
            arguments
                alongtrack struct
            end
            trackIndices = find(diff(alongtrack.t)>1);
            trackIndices(end+1) = length(alongtrack.t);
            startIndex = 1;
            tracks = cell(length(trackIndices),1);
            for i=1:length(trackIndices)
                endIndex = trackIndices(i);
                tracks{i}.x = alongtrack.x(startIndex:endIndex);
                tracks{i}.y = alongtrack.y(startIndex:endIndex);
                tracks{i}.t = alongtrack.t(startIndex:endIndex);
                startIndex = endIndex+1;
            end
        end

        function inclination = inclinationForExactRepeatForMission(mission)
            %Compute inclination used for exact repeat-orbit tuning for a mission.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Exact repeat
            % - Declaration: inclination = inclinationForExactRepeatForMission(mission)
            % - Parameter mission: string | char
            % - Returns inclination: value
            arguments
                mission {mustBeText}
            end
            mu = AlongTrackSimulator.mu;
            J2 = AlongTrackSimulator.J2;
            RE = AlongTrackSimulator.RE;
            T_sidereal = AlongTrackSimulator.T_sidereal;

            missionParametersDict = AlongTrackSimulator.missionParametersCatalog;
            p = missionParametersDict(mission);

            a = p.semi_major_axis; e = p.eccentricity; i = p.inclination;
            N_orbits = p.passes_per_cycle/2;
            T_orbit = 2*pi*sqrt(a^3 / mu);
            p = a * (1 - e^2);
            gamma = (3/2) * J2 * (RE / p)^2;

            M = round(N_orbits * (T_orbit/T_sidereal + gamma*cosd(i)));
            inclination = acosd((M/N_orbits - T_orbit/T_sidereal)/gamma);
        end

        function a = semimajorAxisForExactRepeatForMission(mission)
            %Compute tuned semi-major axis that enforces an exact repeat cycle.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Exact repeat
            % - Declaration: a = semimajorAxisForExactRepeatForMission(mission)
            % - Parameter mission: string | char
            % - Returns a: value
            arguments
                mission {mustBeText}
            end
            mu = AlongTrackSimulator.mu;
            J2 = AlongTrackSimulator.J2;
            RE = AlongTrackSimulator.RE;
            T_sidereal = AlongTrackSimulator.T_sidereal;

            missionParametersDict = AlongTrackSimulator.missionParametersCatalog;
            p = missionParametersDict(mission);

            a = p.semi_major_axis; e = p.eccentricity; i = p.inclination;
            N_orbits = p.passes_per_cycle/2;
            T_orbit = 2*pi*sqrt(a^3 / mu);
            p = a * (1 - e^2);
            gamma = (3/2) * J2 * (RE / p)^2;
            M = round(N_orbits * (T_orbit/T_sidereal + gamma*cosd(i)));

            f = @(a) abs(((2*pi/T_sidereal) * sqrt(a^3/mu) + (3/2) * cosd(i) * J2 * (RE / (a * (1 - e^2)))^2)*N_orbits - M);

            a = fminsearch(f,a);
        end

        function J2 = j2ForExactRepeatForMission(mission)
            %Return the J2 value used in exact repeat-orbit calculations.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Exact repeat
            % - Declaration: J2 = j2ForExactRepeatForMission(mission)
            % - Parameter mission: string | char
            % - Returns J2: value
            arguments
                mission {mustBeText}
            end
            mu = AlongTrackSimulator.mu;
            J2 = AlongTrackSimulator.J2;
            RE = AlongTrackSimulator.RE;
            T_sidereal = AlongTrackSimulator.T_sidereal;

            missionParametersDict = AlongTrackSimulator.missionParametersCatalog;
            p = missionParametersDict(mission);

            a = p.semi_major_axis; e = p.eccentricity; i = p.inclination;
            N_orbits = p.passes_per_cycle/2;
            T_orbit = 2*pi*sqrt(a^3 / mu);
            p = a * (1 - e^2);
            gamma = (3/2) * J2 * (RE / p)^2;
            M = round(N_orbits * (T_orbit/T_sidereal + gamma*cosd(i)));

            f = @(J2) abs(((2*pi/T_sidereal) * sqrt(a^3/mu) + (3/2) * cosd(i) * (J2*1e-4) * (RE / (a * (1 - e^2)))^2)*N_orbits - M);

            J2 = fminsearch(f,J2*1e4)/1e4;
        end

        function e = eccentricityForExactRepeatForMission(mission)
            %Return the eccentricity used for exact repeat-orbit calculations.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Exact repeat
            % - Declaration: e = eccentricityForExactRepeatForMission(mission)
            % - Parameter mission: string | char
            % - Returns e: value
            arguments
                mission {mustBeText}
            end
            mu = AlongTrackSimulator.mu;
            J2 = AlongTrackSimulator.J2;
            RE = AlongTrackSimulator.RE;
            T_sidereal = AlongTrackSimulator.T_sidereal;

            missionParametersDict = AlongTrackSimulator.missionParametersCatalog;
            p = missionParametersDict(mission);

            a = p.semi_major_axis; e = p.eccentricity; i = p.inclination;
            N_orbits = p.passes_per_cycle/2;
            T_orbit = 2*pi*sqrt(a^3 / mu);
            p = a * (1 - e^2);
            gamma = (3/2) * J2 * (RE / p)^2;
            M = round(N_orbits * (T_orbit/T_sidereal + gamma*cosd(i)));

            f = @(e) abs(((2*pi/T_sidereal) * sqrt(a^3/mu) + (3/2) * cosd(i) * J2 * (RE / (a * (1 - (1e-5*e)^2)))^2)*N_orbits - M);

            e = fminsearch(f,e*1e5)/1e5;
        end

        function E = kepler1(M,e)
            %Solve Kepler's equation (M = E - e sin E) using a simple fixed-point/Newton iteration.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Kepler solvers
            % - Declaration: E = kepler1(M,e)
            % - Parameter M: double
            % - Parameter e: double
            % - Returns E: value
            arguments
                M double
                e double
            end
            E = M; % Initial guess
            for j = 1:10 % Iterate to improve accuracy
                E = E - (E - e * sin(E) - M) / (1 - e * cos(E));
            end
        end

        function E = kepler2(M,e)
            %Solve Kepler's equation using Newton iteration (scalar).
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Kepler solvers
            % - Declaration: E = kepler2(M,e)
            % - Parameter M: double
            % - Parameter e: double
            % - Returns E: value
            arguments
                M double
                e double
            end
            E = M;            % Initial guess
            tol = 1e-8;       % Convergence tolerance
            maxIter = 100;    % Maximum iterations
            for iter = 1:maxIter
                f = E - e*sin(E) - M;
                fprime = 1 - e*cos(E);
                deltaE = -f / fprime;
                E = E + deltaE;
                if abs(deltaE) < tol
                    break;
                end
            end
        end

        function E = kepler2vec(M, e)
            %Solve Kepler's equation using a vectorized Newton iteration.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Kepler solvers
            % - Declaration: E = kepler2vec(M, e)
            % - Parameter M: double
            % - Parameter e: double
            % - Returns E: value
            arguments
                M double
                e double
            end
            tol = 1e-8;       % tolerance on deltaE
            maxIter = 100;    % maximum number of Newton-Raphson iterations

            % Initial guess (good for e < 1)
            E = M;
            % Track which elements have converged
            converged = false(size(M));

            for iter = 1:maxIter
                % Compute residuals and derivative
                f = E - e .* sin(E) - M;
                fprime = 1 - e .* cos(E);
                % Compute update
                deltaE = -f ./ fprime;
                % Update only unconverged elements
                E(~converged) = E(~converged) + deltaE(~converged);
                % Check convergence for each element
                newlyConverged = abs(deltaE) < tol;
                converged = converged | newlyConverged;
                % If all have converged, exit early
                if all(converged, 'all')
                    break;
                end
            end
        end


        function E = kepler3(M,e)
            %Solve Kepler's equation by minimizing absolute equation error (scalar).
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Kepler solvers
            % - Declaration: E = kepler3(M,e)
            % - Parameter M: double
            % - Parameter e: double
            % - Returns E: value
            arguments
                M double
                e double
            end
            kepler_eq = @(E) abs(E - e * sin(E) - M); % Absolute error in Kepler's equation

            % Use fminsearch to find E that minimizes the function
            % optimset('TolFun', 1e-10, 'TolX', 1e-10,'MaxIter',10)
            E = fminsearch(kepler_eq, M,optimset('TolFun', 1e-10, 'TolX', 1e-10));
        end

        function E = kepler4(M,e)
            %Solve Kepler's equation with Newton iteration and a different stopping criterion (scalar).
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Kepler solvers
            % - Declaration: E = kepler4(M,e)
            % - Parameter M: double
            % - Parameter e: double
            % - Returns E: value
            arguments
                M double
                e double
            end
            E = M; % Initial guess
            tol = 1e-8;
            max_iter = 10;

            for j = 1:max_iter
                f = E - e * sin(E) - M;
                fp = 1 - e * cos(E);
                fpp = e * sin(E);

                dE = -f / (fp - 0.5 * f * fpp / fp); % Householder's update
                E = E + dE;

                if abs(dE) < tol
                    break;
                end
            end

        end


        function E = kepler5(M,e)
            %Solve Kepler's equation using a piecewise initial guess followed by Newton refinement.
            %
            % Concise, but complete description of this function and how to use it.
            %
            % - Topic: Orbital mechanics — Kepler solvers
            % - Declaration: E = kepler5(M,e)
            % - Parameter M: double
            % - Parameter e: double
            % - Returns E: value
            arguments
                M double
                e double
            end
            if e < 0.8
                E = M + e * sin(M); % Good initial guess for low e
            else
                E = pi; % Better guess for high e
            end

            for j = 1:4  % Converges even faster than Newton
                f_E = E - e * sin(E) - M;
                f_E_deriv = 1 - e * cos(E);
                f_E_2nd_deriv = e * sin(E);

                % Halley's method update
                E = E - (2 * f_E * f_E_deriv) / (2 * f_E_deriv^2 - f_E * f_E_2nd_deriv);
            end
        end

    end

end