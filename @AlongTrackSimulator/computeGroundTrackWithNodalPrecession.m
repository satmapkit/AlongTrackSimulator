function [lat, lon] = computeGroundTrackWithNodalPrecession(semi_major_axis, e, incl, RAAN, omega, M0, t)
    % COMPUTE_GROUND_TRACK Computes the satellite ground track including nodal precession
    % 
    % Inputs:
    %   a      - Semi-major axis (km)
    %   e      - Eccentricity
    %   i      - Inclination (degrees)
    %   RAAN_0 - Initial Right Ascension of Ascending Node (degrees)
    %   omega  - Argument of Perigee (degrees)
    %   M0     - Initial Mean Anomaly (degrees)
    %   t      - Time array (seconds)
    %   mu     - Gravitational parameter (default: Earth's, 398600.4418 km^3/s^2)
    %
    % Outputs:
    %   lat    - Latitude (degrees)
    %   lon    - Longitude (degrees)
    
    % Constants
    mu = 398600.4418; % Earth's gravitational parameter (km^3/s^2)
    J2 = 1.08263e-3;      % Earth's J2 coefficient
    RE = 6378.137;        % Earth's equatorial radius (km)
    
    % Convert inclination to radians
    incl = deg2rad(incl);
    
    % Compute Mean Motion
    n = sqrt(mu / semi_major_axis^3); % Mean motion (rad/s)
    
    % Compute Nodal Precession Rate
    p = semi_major_axis * (1 - e^2); % Semi-latus rectum
    RAAN_dot = - (3/2) * J2 * (RE / p)^2 * n * cos(incl); % RAAN precession rate (rad/s)
    
    % Initialize outputs
    lat = zeros(size(t));
    lon = zeros(size(t));
    
    % Loop through each time step
    for k = 1:length(t)
        % Compute updated RAAN due to nodal precession
        RAAN = deg2rad(RAAN) + RAAN_dot * t(k);
        
        % Compute Mean Anomaly at time t
        M = deg2rad(M0) + n * t(k);
        
        % Solve Kepler's Equation for Eccentric Anomaly using Newton-Raphson
        E = M; % Initial guess
        for j = 1:10 % Iterate to improve accuracy
            E = E - (E - e * sin(E) - M) / (1 - e * cos(E));
        end
        
        % Compute True Anomaly
        theta = 2 * atan2(sqrt(1 + e) * sin(E / 2), sqrt(1 - e) * cos(E / 2));
        
        % Compute Position in Orbital Plane
        r = p / (1 + e * cos(theta)); % Radius (km)
        x_orb = r * cos(theta);
        y_orb = r * sin(theta);
        
        % Rotation Matrices to transform to ECI
        R3_W = [cos(RAAN), -sin(RAAN), 0; sin(RAAN), cos(RAAN), 0; 0, 0, 1]; % Rotate by RAAN
        R1_i = [1, 0, 0; 0, cos(incl), -sin(incl); 0, sin(incl), cos(incl)]; % Rotate by inclination
        R3_w = [cos(deg2rad(omega)), -sin(deg2rad(omega)), 0; sin(deg2rad(omega)), cos(deg2rad(omega)), 0; 0, 0, 1]; % Rotate by argument of perigee
        
        % Compute ECI coordinates
        r_eci = R3_W * R1_i * R3_w * [x_orb; y_orb; 0];
        
        % Convert to ECEF (Earth rotates at ω_E)
        omega_E = 7.2921159e-5; % Earth's rotation rate (rad/s)
        theta_GMST = omega_E * t(k); % Earth's rotation angle
        
        R3_E = [cos(theta_GMST), sin(theta_GMST), 0; -sin(theta_GMST), cos(theta_GMST), 0; 0, 0, 1]; % Earth rotation matrix
        r_ecef = R3_E * r_eci;
        
        % Convert ECEF to Latitude and Longitude
        lon(k) = atan2(r_ecef(2), r_ecef(1));
        lat(k) = atan2(r_ecef(3), sqrt(r_ecef(1)^2 + r_ecef(2)^2));
    end
    
    % Convert output to degrees
    lat = rad2deg(lat);
    lon = rad2deg(lon);
    
    % Ensure longitude is within -180 to 180 range
    lon = mod(lon + 180, 360) - 180;
end