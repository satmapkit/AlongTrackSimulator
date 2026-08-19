classdef ATSTestReferenceOrbit < AlongTrackSimulatorBase
    methods
        function missionNames = missions(~)
            missionNames = "s6a";
        end

        function [latitude,longitude,time] = groundTrackForMissionWithName(~,~)
            latitude = [0;0];
            longitude = [179.9;-179.9];
            time = [1;2];
        end
    end
end
