function [lat, lon] = computeGroundTrackWithNodalPrecession(semi_major_axis, e, incl, RAAN_0, omega, M0, t)
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
    %
    % Outputs:
    %   lat    - Latitude (degrees)
    %   lon    - Longitude (degrees)
    
    % Constants
    mu = 398600.4418; % Earth's gravitational parameter (km^3/s^2)
    J2 = 1.08263e-3;      % Earth's J2 coefficient
    RE = 6378.137;        % Earth's equatorial radius (km)
    omega_E = 7.2921159e-5; % Earth's rotation rate (rad/s)
    
    % Convert inclination to radians
    RAAN_0 = deg2rad(RAAN_0);
    incl = deg2rad(incl);
    omega = deg2rad(omega);
    M0 = deg2rad(M0);
    
    % Compute Mean Motion
    n = sqrt(mu / semi_major_axis^3); % Mean motion (rad/s)

    % Compute Nodal Precession Rate
    p = semi_major_axis * (1 - e^2); % Semi-latus rectum
    RAAN_dot = - (3/2) * J2 * (RE / p)^2 * n * cos(incl); % RAAN precession rate (rad/s)

    % Compute Mean Anomaly at time t
    M = M0 + n * t;

    % kepler2 appears to be the fastest E =
    % AlongTrackSimulator.kepler2(M,e);
    E = AlongTrackSimulator.kepler2vec(M, e);

    % Compute True Anomaly
    theta = 2 * atan2(sqrt(1 + e) * sin(E / 2), sqrt(1 - e) * cos(E / 2));
    cosTheta = cos( theta + omega);
    sinTheta = sin( theta + omega);

    cosOmega = cos( RAAN_0 + (-omega_E + RAAN_dot) * t );
    sinOmega = sin( RAAN_0 + (-omega_E + RAAN_dot) * t );

    x_prime =(cosTheta .* cosOmega - cos(incl)*sinTheta.*sinOmega);
    y_prime =(cosTheta .* sinOmega + cos(incl)*sinTheta.*cosOmega);
    z_prime =sinTheta * sin(incl);

    % Convert ECEF to Latitude and Longitude
    lon = atan2(y_prime, x_prime);
    lat = atan2(z_prime, sqrt(x_prime.^2 + y_prime.^2));

    % Convert output to degrees
    lat = rad2deg(lat);
    lon = rad2deg(lon);
    
    % Ensure longitude is within -180 to 180 range
    lon = mod(lon + 180, 360) - 180;
end
