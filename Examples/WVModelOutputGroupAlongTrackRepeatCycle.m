classdef WVModelOutputGroupAlongTrackRepeatCycle < WVModelOutputGroup
    %UNTITLED Summary of this class goes here
    %   Detailed explanation goes here

    properties
        missionName
        passoverTimes
        repeatCycle

        firstPassoverTime
    end

    methods
        function self = WVModelOutputGroupAlongTrackRepeatCycle(model,missionName,passOverTimes,repeatCycle)
            arguments
                model WVModel
                missionName {mustBeText}
                passOverTimes
                repeatCycle
            end
            self@WVModelOutputGroup(model,missionName);
            self.missionName = missionName;
            self.passoverTimes = passOverTimes;
            self.repeatCycle = repeatCycle;

            % We treat the entire passover as one instant in time, and thus
            % only stop the model at the first t in the passover.
            self.firstPassoverTime = zeros(length(self.passoverTimes),1);
            for iPassover = 1:length(self.passoverTimes)
                self.firstPassoverTime(iPassover) = self.passoverTimes{iPassover}.t(1);
            end
        end

        function aString = description(self)
            aString = "Sampling pattern for the " + self.missionName + " mission with a repeat cycle of " + string(self.repeatCycle) + "s.";
        end

        function t = outputTimesForIntegrationPeriod(self,initialTime,finalTime)
            arguments (Input)
                self WVModelOutputGroup
                initialTime (1,1) double
                finalTime (1,1) double
            end
            arguments (Output)
                t (:,1) double
            end
            iCycle = floor(initialTime/self.repeatCycle);
            fCycle = floor(finalTime/self.repeatCycle);
            t = reshape(self.firstPassoverTime + (iCycle:fCycle)*self.repeatCycle,[],1);
            
            t(t<initialTime) = [];
            t(t>finalTime) = [];
            t = setdiff(t,self.timeOfLastIncrementWrittenToGroup);
        end

    end
end