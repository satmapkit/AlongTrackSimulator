classdef AlongTrackSimulator < AlongTrackSimulatorBase
    %UNTITLED Summary of this class goes here
    %   Detailed explanation goes here


    methods

        function [lat,lon,time] = pathForMissionWithName(self,mission,options)
            arguments
                self AlongTrackSimulator
                mission {mustBeText}
                options.time
                options.N_orbits = 1
            end
            missionParametersDict = AlongTrackSimulator.missionParameters;
            p = missionParametersDict(mission);
            
            mu = 398600.4418;         % Earth's gravitational parameter [km^3/s^2]
            T_orbit = 2*pi*sqrt((p.semi_major_axis)^3/mu);  % Orbital period [s]
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
            RAAN = deg2rad(p.longitude_at_equator);    % Right Ascension of Ascending Node [rad], or longitude of the ascending node
            argPeriapsis = 0;         % Argument of periapsis [rad].
            M0 = 0;                   % Initial mean anomaly [rad]
                        % [lat, lon] = AlongTrackSimulator.computeGroundTrack(p.semi_major_axis, p.eccentricity, deg2rad(p.inclination), RAAN, argPeriapsis, M0, time_shifted);
            [lat, lon] = AlongTrackSimulator.computeGroundTrackWithNodalPrecession(p.semi_major_axis, p.eccentricity, p.inclination, RAAN, argPeriapsis, M0, time_shifted);
        end

        function T = orbitalPeriodForMissionWithName(self,mission)
            missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);
            
            mu = 398600.4418;         % Earth's gravitational parameter [km^3/s^2]
            T = 2*pi*sqrt((p.semi_major_axis)^3/mu);  % Orbital period [s]
        end

        function T = nodalPeriodForMissionWithName(self,mission)
            missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);
            
            T = self.computeNodalPeriod(p.semi_major_axis,p.eccentricity, p.inclination);  % Orbital period [s]
        end

        function T = repeatCycleForMissionWithName(self,mission)
            missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);
            T = p.repeat_cycle*86400;
        end

        function missions = missions(self)
            missionData = AlongTrackSimulator.missionParameters();
            missions = missionData.keys;
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
        missionData = missionParameters();
        [lat, lon] = computeGroundTrack(altitude, e, incl, RAAN, argPerigee, M0, t)
        [lat, lon] = computeGroundTrackWithNodalPrecession(semi_major_axis, e, incl, RAAN, omega, M0, t)
        T_nodal = computeNodalPeriod(a, e, i)
    end

end