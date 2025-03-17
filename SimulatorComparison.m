atse = AlongTrackSimulatorEmpirical();
missionName = atse.missions{24}

ats = AlongTrackSimulator();
T_orbit = ats.orbitalPeriodForMissionWithName(missionName);

[lat_e,lon_e,time_e] = atse.pathForMissionWithName(missionName);
t_e = seconds(time_e - time_e(1));

%%
figure

scatter(lon_e(t_e<T_orbit),lat_e(t_e<T_orbit),1.5^2,"blue","filled"), hold on
[lat,lon,time] = ats.pathForMissionWithName(missionName);
scatter(lon,lat,1.5^2,"red","filled")