ats = AlongTrackSimulator();
missionName = "tp";
T_orbit = ats.orbitalPeriodForMissionWithName(missionName);
N = 9.9156*86400/ats.orbitalPeriodForMissionWithName("tp");
N = 127;
[lat,lon,time] = ats.pathForMissionWithName(missionName,N_orbits=N);

%%

lag = 3000;

figure
scatter(lon(1:lag),lat(1:lag),1.5^2,"red","filled"), hold on
scatter(lon((end-lag):end),lat((end-lag):end),1.5^2,"blue","filled")