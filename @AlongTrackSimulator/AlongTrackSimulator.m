classdef AlongTrackSimulator < AlongTrackSimulatorBase
    %UNTITLED Summary of this class goes here
    %   Detailed explanation goes here


    methods

        function [lat,lon,time] = groundTrackForMissionWithName(self,mission,options)
            arguments
                self AlongTrackSimulator
                mission {mustBeText}
                options.time
                options.N_orbits = 1
            end
            missionParametersDict = AlongTrackSimulator.missionParameters;
            p = missionParametersDict(mission);
            p.semi_major_axis = self.semimajorAxisForExactRepeatForMission(mission);
            % p.eccentricity = self.eccentricityForExactRepeatForMission(mission);
            % p.inclination = self.inclinationForExactRepeatForMission(mission);

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
            % [lat, lon] = AlongTrackSimulator.computeGroundTrack(p.semi_major_axis, p.eccentricity, p.inclination, RAAN, argPeriapsis, M0, time_shifted);
            [lat, lon] = AlongTrackSimulator.computeGroundTrackWithNodalPrecession(p.semi_major_axis, p.eccentricity, p.inclination, RAAN, argPeriapsis, M0, time_shifted);
        end

        function inclination = inclinationForExactRepeatForMission(self,mission)
            mu = 398600.4418; % Earth's gravitational parameter (km^3/s^2)
            J2 = 1.08262668e-3;      % Earth's J2 coefficient
            RE = 6378.1363;        % Earth's equatorial radius (km)

            missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);

            a = p.semi_major_axis; e = p.eccentricity; i = p.inclination;
            N_orbits = p.passes_per_cycle/2;
            T_orbit = 2*pi*sqrt(a^3 / mu);
            p = a * (1 - e^2);
            gamma = (3/2) * J2 * (RE / p)^2;
            T_sidereal = 86164;

            M = round(N_orbits * (T_orbit/T_sidereal + gamma*cosd(i)));
            inclination = acosd((M/N_orbits - T_orbit/T_sidereal)/gamma);
        end

        function a = semimajorAxisForExactRepeatForMission(self,mission)
            mu = 398600.4418; % Earth's gravitational parameter (km^3/s^2)
            J2 = 1.08262668e-3;      % Earth's J2 coefficient
            RE = 6378.1363;        % Earth's equatorial radius (km)

            missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);

            a = p.semi_major_axis, e = p.eccentricity; i = p.inclination;
            N_orbits = p.passes_per_cycle/2;
            T_orbit = 2*pi*sqrt(a^3 / mu);
            p = a * (1 - e^2);
            gamma = (3/2) * J2 * (RE / p)^2;
            T_sidereal = 86164;
            M = round(N_orbits * (T_orbit/T_sidereal + gamma*cosd(i)));

            f = @(a) abs(((2*pi/T_sidereal) * sqrt(a^3/mu) + (3/2) * cosd(i) * J2 * (RE / (a * (1 - e^2)))^2)*N_orbits - M);

            a = fminsearch(f,a);            
        end

        function J2 = j2ForExactRepeatForMission(self,mission)
            mu = 398600.4418; % Earth's gravitational parameter (km^3/s^2)
            J2 = 1.08262668e-3;      % Earth's J2 coefficient
            RE = 6378.1363;        % Earth's equatorial radius (km)

            missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);

            a = p.semi_major_axis; e = p.eccentricity; i = p.inclination;
            N_orbits = p.passes_per_cycle/2;
            T_orbit = 2*pi*sqrt(a^3 / mu);
            p = a * (1 - e^2);
            gamma = (3/2) * J2 * (RE / p)^2;
            T_sidereal = 86164;
            M = round(N_orbits * (T_orbit/T_sidereal + gamma*cosd(i)));

            f = @(J2) abs(((2*pi/T_sidereal) * sqrt(a^3/mu) + (3/2) * cosd(i) * (J2*1e-4) * (RE / (a * (1 - e^2)))^2)*N_orbits - M);

            J2 = fminsearch(f,J2*1e4)/1e4;
        end

        function e = eccentricityForExactRepeatForMission(self,mission)
            mu = 398600.4418; % Earth's gravitational parameter (km^3/s^2)
            J2 = 1.08262668e-3;      % Earth's J2 coefficient
            RE = 6378.1363;        % Earth's equatorial radius (km)

            missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);

            a = p.semi_major_axis; e = p.eccentricity, i = p.inclination;
            N_orbits = p.passes_per_cycle/2;
            T_orbit = 2*pi*sqrt(a^3 / mu);
            p = a * (1 - e^2);
            gamma = (3/2) * J2 * (RE / p)^2;
            T_sidereal = 86164;
            M = round(N_orbits * (T_orbit/T_sidereal + gamma*cosd(i)));

            f = @(e) abs(((2*pi/T_sidereal) * sqrt(a^3/mu) + (3/2) * cosd(i) * J2 * (RE / (a * (1 - (1e-5*e)^2)))^2)*N_orbits - M);

            e = fminsearch(f,e*1e5)/1e5;            
        end

        function [lat,lon,time] = repeatGroundTrackForMissionWithName(self,mission)
           missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);
            N_orbits = p.passes_per_cycle/2;
            if isinf(N_orbits)
                error("The mission " + mission + " does not have a repeat cycle.")
            end
            [lat,lon,time] = self.groundTrackForMissionWithName(mission,N_orbits=N_orbits);
        end

        function T = orbitalPeriodForMissionWithName(self,mission)
            missionParametersDict = self.missionParameters;
            p = missionParametersDict(mission);
            p.semi_major_axis = self.semimajorAxisForExactRepeatForMission(mission);
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

        function alongtrack = projectedPointsForRepeatMissionWithName(self,missionName,options)
            arguments
                self AlongTrackSimulator
                missionName string
                options.Lx
                options.Ly
                options.lat0
                options.lon0
            end
            optionsArgs = namedargs2cell(options);
            [x0, y0, minLat, minLon, maxLat, maxLon] = self.LatitudeLongitudeBoundsForTransverseMercatorBox(optionsArgs{:});
            [lat,lon,time] = self.repeatGroundTrackForMissionWithName(missionName);

            % 1) apply crude filter
            withinBox = lat >= minLat & lat <= maxLat & lon >= minLon & lon <= maxLon;
            lat(~withinBox) = [];
            lon(~withinBox) = [];
            time(~withinBox) = [];

            % 2) project points
            use options;
            [x,y] = AlongTrackSimulatorBase.LatitudeLongitudeToTransverseMercator(lat,lon,lon0=lon0);

            % 3) apply more precise filter
            out_of_bounds = (x < x0 - Lx/2) | (x > x0 + Lx/2) | (y < y0 - Ly/2) | (y > y0 + Ly/2);
            alongtrack.x = x(~out_of_bounds);
            alongtrack.y = y(~out_of_bounds)-y0;
            alongtrack.time = time(~out_of_bounds);

            [alongtrack.time,I] = sort(alongtrack.time);
            alongtrack.x = alongtrack.x(I);
            alongtrack.y = alongtrack.y(I);
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

        function E = kepler1(M,e)
            % Solve Kepler's Equation for Eccentric Anomaly using Newton-Raphson
            E = M; % Initial guess
            for j = 1:10 % Iterate to improve accuracy
                E = E - (E - e * sin(E) - M) / (1 - e * cos(E));
            end
        end

        function E = kepler2(M,e)
            % Solve Kepler's Equation: M = E - e*sin(E)
            % Using Newton-Raphson method for iterative solution
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

        function E = kepler3(M,e)
            % Define Kepler's Equation as a function to minimize
            kepler_eq = @(E) abs(E - e * sin(E) - M); % Absolute error in Kepler's equation

            % Use fminsearch to find E that minimizes the function
            % optimset('TolFun', 1e-10, 'TolX', 1e-10,'MaxIter',10)
            E = fminsearch(kepler_eq, M,optimset('TolFun', 1e-10, 'TolX', 1e-10));
        end

        function E = kepler4(M,e)
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
            % Hybrid Newton-Raphson & Halley's Method
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