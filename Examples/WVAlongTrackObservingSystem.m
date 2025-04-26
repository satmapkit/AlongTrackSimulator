classdef WVAlongTrackObservingSystem < WVObservingSystem
    %UNTITLED2 Summary of this class goes here
    %   Detailed explanation goes here

    properties (GetAccess=public, SetAccess=protected)
        missionName
        tracks
        repeatCycle

        firstPassoverTime
    end

    methods
        function self = WVAlongTrackObservingSystem(model,missionName,tracks,repeatCycle)
            %create a new observing system
            %
            % This class is intended to be subclassed, so it generally
            % assumed that this initialization will not be called directly.
            %
            % - Topic: Initialization
            % - Declaration: self = WVObservingSystem(model,name)
            % - Parameter model: the WVModel instance
            % - Parameter name: name of the observing system
            % - Returns self: a new instance of WVObservingSystem
            arguments
                model WVModel
                missionName {mustBeText}
                tracks
                repeatCycle
            end
            self@WVObservingSystem(model,missionName);
            self.missionName = missionName;
            self.tracks = tracks;
            self.repeatCycle = repeatCycle;

            % We treat the entire passover as one instant in time, and thus
            % only stop the model at the first t in the passover.
            self.firstPassoverTime = zeros(length(self.tracks),1);
            for iPassover = 1:length(self.tracks)
                self.firstPassoverTime(iPassover) = self.tracks{iPassover}.t(1);
            end
        end


        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        %
        % Read and write to file
        %
        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        function initializeStorage(self,group)
            spatialDimensionNames = self.model.wvt.spatialDimensionNames;
            for iVar=1:2
                attributes = containers.Map(KeyType='char',ValueType='any');
                attributes('units') = self.model.wvt.dimensionAnnotationWithName(spatialDimensionNames{iVar}).units;
                attributes('long_name') = strcat(self.model.wvt.dimensionAnnotationWithName(spatialDimensionNames{iVar}).description,' position of observation');
                group.addVariable(strcat("track",'_',spatialDimensionNames{iVar}),{"t"},type="double",attributes=attributes,isComplex=false);
            end

            varAnnotation = self.model.wvt.propertyAnnotationWithName("ssh");
            attributes = containers.Map(KeyType='char',ValueType='any');
            attributes('units') = varAnnotation.units;
            attributes('long_name') = strcat(varAnnotation.description,', as observed by a nadir altimeter.');
            group.addVariable("ssh",{'t'},type="double",attributes=attributes,isComplex=false);
        end

        function writeTimeStepToFile(self,group,outputIndices)
            iPassover = find( abs(self.firstPassoverTime - mod(self.wvt.t,self.repeatCycle)) < 1, 1, 'first');

            group.variableWithName("track_x").setValueAlongDimensionAtIndex(self.tracks{iPassover}.x,'t',outputIndices);
            group.variableWithName("track_y").setValueAlongDimensionAtIndex(self.tracks{iPassover}.y,'t',outputIndices);
            ssh = reshape(self.model.wvt.variableAtPositionWithName(self.tracks{iPassover}.x,self.tracks{iPassover}.y,[],'ssh'),[],1);
            group.variableWithName("ssh").setValueAlongDimensionAtIndex(ssh,'t',outputIndices);
        end

        function os = observingSystemWithResolutionOfTransform(self,wvtX2)
            %create a new WVObservingSystem with a new resolution
            %
            % Subclasses to should override this method an implement the
            % correct logic.
            %
            % - Topic: Initialization
            % - Declaration: os = observingSystemWithResolutionOfTransform(self,wvtX2)
            % - Parameter wvtX2: the WVTransform with increased resolution
            % - Returns force: a new instance of WVObservingSystem
            os = WVAlongTrackObservingSystem(wvtX2,self.name);
            error('this needs to be implemented');
        end
    end
end