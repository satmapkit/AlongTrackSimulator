function bundle = authorAlongTrackPortableRunBundle(model,bundlePath,missionName,options)
% Author a source-linked WaveVortex portable run bundle with AlongTrack output.
%
% This function writes a MATLAB-authoritative WaveVortex model/checkpoint NetCDF
% file and adds portable `WVAlongTrackSchedule` and
% `WVAlongTrackObservingSystem` records. The companion JSON file contains only
% runner execution, routing, and destination policy; all scientific
% construction data remains in NetCDF.
%
% The authoring path stores immutable orbital, epoch, projection, cadence,
% field, interpolation, schedule, and observer configuration. It does not
% precompute or serialize future passes or trajectories.
%
% ```matlab
% bundle = authorAlongTrackPortableRunBundle( ...
%     model,"portable-jason3.nc","j3", ...
%     finalTime=30000,missionEpochSeconds=0, ...
%     fieldNames=["ssh","ssu","ssv"], ...
%     integrationMethod="adaptive-rk23");
% ```
%
% - Topic: Portable WaveVortex authoring
% - Declaration: bundle = authorAlongTrackPortableRunBundle(model,bundlePath,missionName,options)
% - Parameter model: WVModel scalar — authoritative MATLAB model and checkpoint state
% - Parameter bundlePath: text scalar — source NetCDF bundle to create
% - Parameter missionName: text scalar — AlongTrack mission-catalog key
% - Parameter options.finalTime: double scalar — schedule and integration upper bound [s]
% - Parameter options.integrationFinalTime: double scalar — first runner segment upper bound [s]; default `finalTime`
% - Parameter options.initialTime: double scalar — schedule lower bound [s]; default `model.t`
% - Parameter options.missionEpochSeconds: double scalar — model time corresponding to mission time zero [s]
% - Parameter options.sampleIntervalSeconds: positive double scalar — ground-track cadence [s]
% - Parameter options.checkpointIntervalSeconds: positive double scalar — coefficient restart cadence [s]
% - Parameter options.fieldNames: string row vector — requested portable horizontal fields
% - Parameter options.interpolation: `"linear"` or `"spline"` — spatial interpolation
% - Parameter options.centerLongitudeDegrees: double scalar — projection central meridian [deg]
% - Parameter options.lowerLeftOrigin: logical scalar — use a lower-left rather than centered origin
% - Parameter options.observerIdentifier: text scalar — stable observer identity
% - Parameter options.fileIdentifier: text scalar — stable source/destination file identity
% - Parameter options.groupIdentifier: text scalar — stable logical schedule/output-group identity
% - Parameter options.requestPath: text scalar — companion run-request JSON path
% - Parameter options.destinationPath: text scalar — requested output NetCDF path
% - Parameter options.reportPath: text scalar — requested runner report path
% - Parameter options.outputPolicy: `"create"`, `"replace"`, or `"append"`
% - Parameter options.integrationMethod: `"fixed-rk4"` or `"adaptive-rk23"`
% - Parameter options.initialStep: positive double scalar — requested first integration step [s]
% - Parameter options.maximumStep: positive double scalar — adaptive maximum step [s]
% - Parameter options.relativeTolerance: positive double scalar — adaptive relative tolerance
% - Parameter options.absoluteToleranceScale: positive double scalar — adaptive absolute-tolerance scale
% - Parameter options.fftProvider: `"reference"` or `"native-fftw"`
% - Parameter options.threads: positive integer scalar — runner thread count
% - Parameter options.shouldOverwriteExisting: logical scalar — replace only the authored source/request files
% - Returns bundle: struct — resolved paths, identities, versions, and exact construction configuration

arguments
    model (1,1) WVModel
    bundlePath (1,1) string
    missionName (1,1) string
    options.finalTime (1,1) double {mustBeFinite}
    options.integrationFinalTime (1,1) double = NaN
    options.initialTime (1,1) double = NaN
    options.missionEpochSeconds (1,1) double {mustBeFinite} = 0
    options.sampleIntervalSeconds (1,1) double {mustBePositive,mustBeFinite} = 1
    options.checkpointIntervalSeconds (1,1) double {mustBePositive,mustBeFinite} = 60
    options.fieldNames (1,:) string = ["ssh","ssu","ssv"]
    options.interpolation (1,1) string {mustBeMember(options.interpolation,["linear","spline"])} = "linear"
    options.centerLongitudeDegrees (1,1) double {mustBeFinite} = 0
    options.lowerLeftOrigin (1,1) logical = true
    options.observerIdentifier (1,1) string = ""
    options.fileIdentifier (1,1) string = "primary"
    options.groupIdentifier (1,1) string = ""
    options.requestPath (1,1) string = ""
    options.destinationPath (1,1) string = ""
    options.reportPath (1,1) string = ""
    options.outputPolicy (1,1) string {mustBeMember(options.outputPolicy,["create","replace","append"])} = "create"
    options.integrationMethod (1,1) string {mustBeMember(options.integrationMethod,["fixed-rk4","adaptive-rk23"])} = "fixed-rk4"
    options.initialStep (1,1) double {mustBePositive,mustBeFinite} = 1
    options.maximumStep (1,1) double {mustBePositive,mustBeFinite} = 60
    options.relativeTolerance (1,1) double {mustBePositive,mustBeFinite} = 1e-3
    options.absoluteToleranceScale (1,1) double {mustBePositive,mustBeFinite} = 1e-6
    options.fftProvider (1,1) string {mustBeMember(options.fftProvider,["reference","native-fftw"])} = "reference"
    options.threads (1,1) double {mustBeInteger,mustBePositive} = 1
    options.shouldOverwriteExisting (1,1) logical = false
end

if isnan(options.integrationFinalTime)
    options.integrationFinalTime = options.finalTime;
else
    mustBeFinite(options.integrationFinalTime);
end
if isnan(options.initialTime)
    options.initialTime = model.t;
else
    mustBeFinite(options.initialTime);
end
if options.finalTime < options.integrationFinalTime || ...
        options.integrationFinalTime < options.initialTime || options.initialTime < model.t
    error("AlongTrackSimulator:InvalidPortableRunWindow", ...
        "The authoring window must satisfy model.t <= initialTime <= " + ...
        "integrationFinalTime <= finalTime.");
end
mustBePortableIdentifier(options.fileIdentifier,"fileIdentifier");

ats = AlongTrackSimulator();
catalog = AlongTrackSimulator.missionParametersCatalog();
if ~isKey(catalog,missionName)
    error("AlongTrackSimulator:UnknownMission", ...
        "Mission '%s' is not present in the AlongTrack mission catalog.",missionName);
end
sourceMission = catalog(missionName);
resolvedMission = ats.missionParameters(missionName);
fieldNames = validateFields(model,options.fieldNames);

[bundleFolder,bundleStem,bundleExtension] = fileparts(bundlePath);
if bundleExtension == ""
    bundleExtension = ".nc";
    bundlePath = fullfile(bundleFolder,bundleStem + bundleExtension);
elseif ~strcmpi(bundleExtension,".nc")
    error("AlongTrackSimulator:InvalidPortableBundleExtension", ...
        "bundlePath must use the .nc extension.");
end
if bundleFolder == ""
    bundleFolder = string(pwd);
    bundlePath = fullfile(bundleFolder,bundleStem + bundleExtension);
end
if ~isfolder(bundleFolder)
    error("AlongTrackSimulator:MissingPortableBundleFolder", ...
        "The bundle folder '%s' does not exist.",bundleFolder);
end
bundlePath = absolutePath(bundlePath);

if options.observerIdentifier == ""
    options.observerIdentifier = portableIdentifier(missionName + "-along-track-observer");
end
if options.groupIdentifier == ""
    options.groupIdentifier = portableIdentifier(missionName + "-along-track-output");
end
mustBePortableIdentifier(options.observerIdentifier,"observerIdentifier");
mustBePortableIdentifier(options.groupIdentifier,"groupIdentifier");

if options.requestPath == ""
    options.requestPath = fullfile(bundleFolder,bundleStem + ".run.json");
end
if options.destinationPath == ""
    options.destinationPath = fullfile(bundleFolder,bundleStem + ".output.nc");
end
if options.reportPath == ""
    options.reportPath = fullfile(bundleFolder,bundleStem + ".report.json");
end
requestPath = absolutePath(options.requestPath);
destinationPath = absolutePath(options.destinationPath);
reportPath = absolutePath(options.reportPath);

if bundlePath == requestPath || bundlePath == destinationPath || ...
        bundlePath == reportPath || requestPath == destinationPath || ...
        requestPath == reportPath || destinationPath == reportPath
    error("AlongTrackSimulator:AliasedPortableRunPath", ...
        "Source, request, destination, and report paths must be distinct.");
end
if ~options.shouldOverwriteExisting && (isfile(bundlePath) || isfile(requestPath))
    error("AlongTrackSimulator:PortableAuthoringTargetExists", ...
        "The authored source and request paths must not already exist.");
end
if options.outputPolicy == "create" && isfile(destinationPath)
    error("AlongTrackSimulator:PortableDestinationExists", ...
        "Create policy requires the destination to be absent.");
end
if options.fftProvider == "reference" && options.threads ~= 1
    error("AlongTrackSimulator:InvalidPortableExecution", ...
        "The reference FFT provider requires exactly one thread.");
end

passesPerCycle = resolvedMission.passes_per_cycle;
if isinf(passesPerCycle)
    passesPerCycle = 0;
end
source = struct( ...
    missionKey=missionName, ...
    semiMajorAxisKm=sourceMission.semi_major_axis, ...
    resolvedSemiMajorAxisKm=resolvedMission.semi_major_axis, ...
    eccentricity=resolvedMission.eccentricity, ...
    inclinationDegrees=resolvedMission.inclination, ...
    ascendingNodeLongitudeDegrees=resolvedMission.longitude_at_equator, ...
    passesPerCycle=passesPerCycle, ...
    missionEpochSeconds=options.missionEpochSeconds, ...
    windowWidthMeters=model.wvt.Lx, ...
    windowHeightMeters=model.wvt.Ly, ...
    centerLatitudeDegrees=model.wvt.latitude, ...
    centerLongitudeDegrees=options.centerLongitudeDegrees, ...
    lowerLeftOrigin=options.lowerLeftOrigin, ...
    sampleIntervalSeconds=options.sampleIntervalSeconds);

scheduleRecord = constructionRecord( ...
    "wv-along-track-schedule-configuration-v1",source,string.empty(1,0),0);
interpolationCode = int64(options.interpolation == "spline");
observerRecord = constructionRecord( ...
    "wv-along-track-observer-configuration-v1",source,fieldNames,interpolationCode);
schema = observationSchema(options.observerIdentifier,missionName,fieldNames,model.wvt);
scheduleHex = hexEncode(encodeTypedRecord(scheduleRecord));
observerHex = hexEncode(encodeTypedRecord(observerRecord));
schemaHex = hexEncode(encodeObservationSchemaManifest(schema));

stagedSourcePath = bundlePath + ".authoring-tmp";
stagedRequestPath = requestPath + ".authoring-tmp";
deleteIfPresent(stagedSourcePath);
deleteIfPresent(stagedRequestPath);
authoringCleanup = onCleanup(@() deleteAuthoredStaging( ...
    stagedSourcePath,stagedRequestPath));
writeCheckpoint(model,stagedSourcePath,options.checkpointIntervalSeconds,options.finalTime);
appendAlongTrackGraph(stagedSourcePath,options.fileIdentifier,options.groupIdentifier, ...
    options.observerIdentifier,missionName, ...
    options.initialTime,options.finalTime,options.sampleIntervalSeconds, ...
    scheduleHex,observerHex,schemaHex,schema);

request = runRequest(bundlePath,requestPath,destinationPath,reportPath, ...
    options.fileIdentifier,options);
writeJSON(stagedRequestPath,request);
installAuthoredFile(stagedSourcePath,bundlePath,options.shouldOverwriteExisting);
try
    installAuthoredFile(stagedRequestPath,requestPath,options.shouldOverwriteExisting);
catch exception
    if ~options.shouldOverwriteExisting
        deleteIfPresent(bundlePath);
    end
    rethrow(exception)
end

bundle = struct( ...
    modelFile=bundlePath, ...
    requestFile=requestPath, ...
    destinationFile=destinationPath, ...
    reportFile=reportPath, ...
    fileIdentifier=options.fileIdentifier, ...
    groupIdentifier=options.groupIdentifier, ...
    scheduleTypeIdentifier="WVAlongTrackSchedule", ...
    scheduleContractVersion=uint32(1), ...
    observerIdentifier=options.observerIdentifier, ...
    observerTypeIdentifier="WVAlongTrackObservingSystem", ...
    observerContractVersion=uint32(1), ...
    sourceConfiguration=source, ...
    fieldNames=fieldNames, ...
    interpolation=options.interpolation, ...
    request=request);
clear authoringCleanup
end

function fieldNames = validateFields(model,fieldNames)
fieldNames = reshape(fieldNames,1,[]);
if isempty(fieldNames)
    error("AlongTrackSimulator:EmptyPortableFieldNames", ...
        "fieldNames must contain at least one registered horizontal field.");
end
if numel(unique(fieldNames)) ~= numel(fieldNames)
    error("AlongTrackSimulator:DuplicatePortableFieldNames", ...
        "fieldNames must not contain duplicates.");
end
for iField = 1:numel(fieldNames)
    fieldName = fieldNames(iField);
    if ~model.wvt.hasVariableWithName(char(fieldName))
        error("AlongTrackSimulator:UnknownPortableField", ...
            "The WaveVortex transform has no registered variable named '%s'.",fieldName);
    end
    annotation = model.wvt.propertyAnnotationWithName(fieldName);
    if ~isequal(annotation.dimensions,{'x','y'})
        error("AlongTrackSimulator:InvalidPortableFieldDimensions", ...
            "Portable AlongTrack field '%s' must have dimensions {'x','y'}.",fieldName);
    end
end
end

function record = constructionRecord(schemaIdentifier,source,fieldNames,interpolation)
values = sourceValues(source);
if ~isempty(fieldNames)
    values(end+1) = namedValue("fieldNames",3,uint64(numel(fieldNames)),fieldNames);
    values(end+1) = namedValue("interpolation",1,uint64.empty(1,0),int64(interpolation));
end
record = struct(schemaIdentifier=schemaIdentifier,schemaVersion=uint32(1),values=values);
end

function values = sourceValues(source)
values = [ ...
    namedValue("missionKey",3,uint64.empty(1,0),source.missionKey), ...
    namedValue("semiMajorAxisKm",2,uint64.empty(1,0),source.semiMajorAxisKm), ...
    namedValue("resolvedSemiMajorAxisKm",2,uint64.empty(1,0),source.resolvedSemiMajorAxisKm), ...
    namedValue("eccentricity",2,uint64.empty(1,0),source.eccentricity), ...
    namedValue("inclinationDegrees",2,uint64.empty(1,0),source.inclinationDegrees), ...
    namedValue("ascendingNodeLongitudeDegrees",2,uint64.empty(1,0),source.ascendingNodeLongitudeDegrees), ...
    namedValue("passesPerCycle",2,uint64.empty(1,0),source.passesPerCycle), ...
    namedValue("missionEpochSeconds",2,uint64.empty(1,0),source.missionEpochSeconds), ...
    namedValue("windowWidthMeters",2,uint64.empty(1,0),source.windowWidthMeters), ...
    namedValue("windowHeightMeters",2,uint64.empty(1,0),source.windowHeightMeters), ...
    namedValue("centerLatitudeDegrees",2,uint64.empty(1,0),source.centerLatitudeDegrees), ...
    namedValue("centerLongitudeDegrees",2,uint64.empty(1,0),source.centerLongitudeDegrees), ...
    namedValue("lowerLeftOrigin",0,uint64.empty(1,0),uint8(source.lowerLeftOrigin)), ...
    namedValue("sampleIntervalSeconds",2,uint64.empty(1,0),source.sampleIntervalSeconds)];
end

function value = namedValue(name,type,dimensions,values)
value = struct(name=name,type=uint8(type),dimensions=uint64(dimensions),values=values);
end

function schema = observationSchema(observerIdentifier,missionName,fieldNames,wvt)
variables = [ ...
    schemaVariable("sample-time",0,"sample",3,"s", ...
        "mission sample time; the model state is evaluated at the pass trigger",2), ...
    schemaVariable("x",0,"sample",3,"m","projected along-track x coordinate",3), ...
    schemaVariable("y",0,"sample",3,"m","projected along-track y coordinate",4), ...
    schemaVariable("repeat-cycle-index",2,string.empty(1,0),2,"", ...
        "repeat-cycle component of the exact pass identity",8), ...
    schemaVariable("first-sample-index",2,string.empty(1,0),2,"", ...
        "cadence-lattice start of the exact pass identity",0), ...
    schemaVariable("sample-count",2,string.empty(1,0),2,"", ...
        "sample extent of the exact pass identity",0)];
fieldVariables = repmat(variables(1),1,numel(fieldNames));
for iField = 1:numel(fieldNames)
    annotation = wvt.propertyAnnotationWithName(fieldNames(iField));
    fieldVariables(iField) = schemaVariable(fieldNames(iField),0,"sample",3, ...
        string(annotation.units),string(annotation.description),0);
end
variables = [variables fieldVariables];
schema = struct( ...
    identifier=observerIdentifier + "-along-track-observation-v1", ...
    version=uint32(1), ...
    preservesLegacyEncoding=false, ...
    metadataAttributes=[ ...
        struct(name="provider",value="WVAlongTrackObservingSystem"), ...
        struct(name="mission",value=missionName), ...
        struct(name="contract",value="generic-along-track-occurrence-v1")], ...
    axes=struct(identifier="sample",name="sample",kind=int64(1), ...
        extent=int64(0),role=int64(6)), ...
    variables=variables);
end

function variable = schemaVariable(identifier,type,dimensions,layout,units,description,role)
variable = struct( ...
    identifier=identifier,name=identifier,type=int64(type), ...
    dimensions=reshape(string(dimensions),1,[]),layout=int64(layout), ...
    units=units,description=description,role=int64(role), ...
    raggedRole=int64(0),raggedChild="",attributes=struct('name',{},'value',{}));
end

function bytes = encodeObservationSchemaManifest(schema)
metadataNames = string({schema.metadataAttributes.name});
metadataValues = string({schema.metadataAttributes.value});
axes = schema.axes;
variables = schema.variables;
dimensionCounts = arrayfun(@(value) numel(value.dimensions),variables);
dimensions = string.empty(1,0);
for iVariable = 1:numel(variables)
    dimensions = [dimensions variables(iVariable).dimensions]; %#ok<AGROW>
end
recordValues = [ ...
    namedValue("schemaIdentifier",3,uint64.empty(1,0),schema.identifier), ...
    namedValue("schemaVersion",1,uint64.empty(1,0),int64(schema.version)), ...
    namedValue("preservesLegacyEncoding",0,uint64.empty(1,0),uint8(schema.preservesLegacyEncoding)), ...
    namedValue("metadataAttributeNames",3,uint64(numel(metadataNames)),metadataNames), ...
    namedValue("metadataAttributeValues",3,uint64(numel(metadataValues)),metadataValues), ...
    namedValue("metadataStringListNames",3,uint64(0),string.empty(1,0)), ...
    namedValue("metadataStringListCounts",1,uint64(0),int64.empty(1,0)), ...
    namedValue("metadataStringListValues",3,uint64(0),string.empty(1,0)), ...
    namedValue("metadataVariableNames",3,uint64(0),string.empty(1,0)), ...
    namedValue("metadataVariableIdentifiers",3,uint64(0),string.empty(1,0)), ...
    namedValue("metadataVariableTypes",1,uint64(0),int64.empty(1,0)), ...
    namedValue("metadataVariableLogical",0,uint64(0),uint8.empty(1,0)), ...
    namedValue("metadataVariableDimensionCounts",1,uint64(0),int64.empty(1,0)), ...
    namedValue("metadataVariableExtents",1,uint64(0),int64.empty(1,0)), ...
    namedValue("axisIdentifiers",3,uint64(numel(axes)),string({axes.identifier})), ...
    namedValue("axisNames",3,uint64(numel(axes)),string({axes.name})), ...
    namedValue("axisKinds",1,uint64(numel(axes)),int64([axes.kind])), ...
    namedValue("axisExtents",1,uint64(numel(axes)),int64([axes.extent])), ...
    namedValue("axisRoles",1,uint64(numel(axes)),int64([axes.role])), ...
    namedValue("variableIdentifiers",3,uint64(numel(variables)),string({variables.identifier})), ...
    namedValue("variableNames",3,uint64(numel(variables)),string({variables.name})), ...
    namedValue("variableTypes",1,uint64(numel(variables)),int64([variables.type])), ...
    namedValue("variableLayouts",1,uint64(numel(variables)),int64([variables.layout])), ...
    namedValue("variableUnits",3,uint64(numel(variables)),string({variables.units})), ...
    namedValue("variableDescriptions",3,uint64(numel(variables)),string({variables.description})), ...
    namedValue("variableRoles",1,uint64(numel(variables)),int64([variables.role])), ...
    namedValue("variableRaggedRoles",1,uint64(numel(variables)),int64([variables.raggedRole])), ...
    namedValue("variableRaggedChildren",3,uint64(numel(variables)),string({variables.raggedChild})), ...
    namedValue("dimensionCounts",1,uint64(numel(variables)),int64(dimensionCounts)), ...
    namedValue("dimensions",3,uint64(numel(dimensions)),dimensions), ...
    namedValue("attributeCounts",1,uint64(numel(variables)),zeros(1,numel(variables),'int64')), ...
    namedValue("attributeNames",3,uint64(0),string.empty(1,0)), ...
    namedValue("attributeValues",3,uint64(0),string.empty(1,0))];
record = struct(schemaIdentifier="portable-observation-schema-manifest-v1", ...
    schemaVersion=uint32(1),values=recordValues);
bytes = encodeTypedRecord(record);
end

function bytes = encodeTypedRecord(record)
chunks = cell(1,numel(record.values)+1);
chunks{1} = [encodeText(record.schemaIdentifier),littleEndian(record.schemaVersion), ...
    littleEndian(uint64(numel(record.values)))];
for iValue = 1:numel(record.values)
    value = record.values(iValue);
    count = valueCount(value.values);
    header = [encodeText(value.name),uint8(value.type), ...
        littleEndian(uint64(numel(value.dimensions))), ...
        littleEndian(uint64(value.dimensions)),littleEndian(uint64(count))];
    switch value.type
        case 0
            payload = uint8(value.values);
        case 1
            payload = littleEndian(int64(value.values));
        case 2
            payload = littleEndian(double(value.values));
        case 3
            texts = reshape(string(value.values),1,[]);
            textChunks = cell(1,numel(texts));
            for iText = 1:numel(texts)
                textChunks{iText} = encodeText(texts(iText));
            end
            payload = [textChunks{:}];
        otherwise
            error("AlongTrackSimulator:InvalidPortableValueType", ...
                "Portable typed-record value types must be in [0,3].");
    end
    chunks{iValue+1} = [header,payload];
end
bytes = uint8([chunks{:}]);
end

function count = valueCount(values)
count = numel(values);
end

function bytes = encodeText(value)
native = unicode2native(char(value),'UTF-8');
bytes = [littleEndian(uint64(numel(native))),reshape(uint8(native),1,[])];
end

function bytes = littleEndian(value)
bytes = reshape(typecast(value(:),'uint8'),1,[]);
[~,~,endian] = computer;
if endian == 'B'
    width = numel(typecast(value(1),'uint8'));
    bytes = reshape(flipud(reshape(bytes,width,[])),1,[]);
end
end

function text = hexEncode(bytes)
text = lower(reshape(dec2hex(uint8(bytes),2).',1,[]));
end

function writeCheckpoint(model,path,checkpointInterval,finalTime)
coefficients = model.eulerianObservingSystem;
requiredCoefficients = ["Ap","Am","A0"];
if isempty(coefficients) || ~all(ismember(requiredCoefficients,coefficients.fieldNames))
    error("AlongTrackSimulator:MissingCoefficientObserver", ...
        "A portable source bundle requires a model with coefficient restart state.");
end
outputFile = WVModelOutputFile(model,char(path),shouldOverwriteExisting=false);
outputGroup = WVModelOutputGroupEvenlySpaced(model,name="portable-restart", ...
    initialTime=model.t,finalTime=finalTime,outputInterval=checkpointInterval);
outputGroup.addObservingSystem(coefficients);
outputFile.addOutputGroup(outputGroup);
cleanup = onCleanup(@() outputFile.closeNetCDFFile());
times = outputFile.outputTimesForIntegrationPeriod(model.t,model.t);
if ~isequal(times,model.t)
    error("AlongTrackSimulator:InvalidPortableRestartSchedule", ...
        "The coefficient restart group did not schedule the current model time.");
end
outputFile.writeTimeStepToOutputFile(model.t);
outputFile.closeNetCDFFile();
clear cleanup
end

function appendAlongTrackGraph(path,fileIdentifier,groupIdentifier, ...
        observerIdentifier,missionName,initialTime,finalTime, ...
        sampleInterval,scheduleHex,observerHex,schemaHex,schema)
fileID = netcdf.open(path,'NC_WRITE');
cleanup = onCleanup(@() closeNetCDF(fileID));
netcdf.reDef(fileID);
netcdf.putAtt(fileID,netcdf.getConstant('NC_GLOBAL'),'portableFileIdentifier',char(fileIdentifier));
groupID = netcdf.defGrp(fileID,char(groupIdentifier));
globalID = netcdf.getConstant('NC_GLOBAL');
netcdf.putAtt(groupID,globalID,'AnnotatedClass','WVModelOutputGroupEvenlySpaced');
netcdf.putAtt(groupID,globalID,'portableIdentifier',char(groupIdentifier));
netcdf.putAtt(groupID,globalID,'portableScheduleTypeIdentifier','WVAlongTrackSchedule');
netcdf.putAtt(groupID,globalID,'portableScheduleContractVersion',uint32(1),'NC_UINT');
netcdf.putAtt(groupID,globalID,'portableScheduleConfiguration',scheduleHex);

timeDimension = netcdf.defDim(groupID,'t',netcdf.getConstant('NC_UNLIMITED'));
sampleDimension = netcdf.defDim(groupID,'sample',netcdf.getConstant('NC_UNLIMITED'));
timeVariable = netcdf.defVar(groupID,'t','NC_DOUBLE',timeDimension);
ordinalVariable = netcdf.defVar(groupID,'portableScheduleOrdinal','NC_INT64',timeDimension); %#ok<NASGU>
cursorVariable = netcdf.defVar(groupID,'portableScheduleCursor','NC_STRING',timeDimension); %#ok<NASGU>
progressVariable = netcdf.defVar(groupID,'portableCommitted_sample','NC_INT64',timeDimension); %#ok<NASGU>
outputIntervalVariable = netcdf.defVar(groupID,'outputInterval','NC_DOUBLE',[]);
initialTimeVariable = netcdf.defVar(groupID,'initialTime','NC_DOUBLE',[]);
finalTimeVariable = netcdf.defVar(groupID,'finalTime','NC_DOUBLE',[]);
netcdf.putAtt(groupID,timeVariable,'units','s');

metadataRoot = netcdf.defGrp(groupID,'observingSystems');
metadataGroup = netcdf.defGrp(metadataRoot,'observingSystems-1');
netcdf.putAtt(metadataGroup,globalID,'AnnotatedClass','WVAlongTrackObservingSystem');
netcdf.putAtt(metadataGroup,globalID,'name',char(observerIdentifier));
netcdf.putAtt(metadataGroup,globalID,'portableIdentifier',char(observerIdentifier));
netcdf.putAtt(metadataGroup,globalID,'portableObserverContractVersion',uint32(1),'NC_UINT');
netcdf.putAtt(metadataGroup,globalID,'portableObserverConfiguration',observerHex);
netcdf.putAtt(metadataGroup,globalID,'portableObservationSchemaIdentifier',char(schema.identifier));
netcdf.putAtt(metadataGroup,globalID,'portableObservationSchemaVersion',uint32(1),'NC_UINT');
netcdf.putAtt(metadataGroup,globalID,'portableObservationSchemaManifest',schemaHex);
netcdf.putAtt(metadataGroup,globalID,'mission',char(missionName));

for iVariable = 1:numel(schema.variables)
    variable = schema.variables(iVariable);
    if variable.layout == 2
        dimensions = timeDimension;
    else
        dimensions = sampleDimension;
    end
    if variable.type == 0
        netCDFType = 'NC_DOUBLE';
    elseif variable.type == 2
        netCDFType = 'NC_INT64';
    else
        error("AlongTrackSimulator:UnsupportedPortableSchemaType", ...
            "The v1 AlongTrack schema contains an unsupported physical type.");
    end
    variableID = netcdf.defVar(groupID,char(variable.name),netCDFType,dimensions);
    putObservationVariableAttributes(groupID,variableID,observerIdentifier,schema,variable);
end
netcdf.endDef(fileID);
netcdf.putVar(groupID,outputIntervalVariable,sampleInterval);
netcdf.putVar(groupID,initialTimeVariable,initialTime);
netcdf.putVar(groupID,finalTimeVariable,finalTime);
netcdf.sync(fileID);
netcdf.close(fileID);
clear cleanup
end

function putObservationVariableAttributes(groupID,variableID,observerIdentifier,schema,variable)
netcdf.putAtt(groupID,variableID,'portableObservationVariableIdentifier',char(variable.identifier));
netcdf.putAtt(groupID,variableID,'portableObservationObserverIdentifier',char(observerIdentifier));
netcdf.putAtt(groupID,variableID,'portableObservationSchemaIdentifier',char(schema.identifier));
netcdf.putAtt(groupID,variableID,'portableObservationSchemaVersion',uint32(schema.version),'NC_UINT');
layouts = ["static","initial","record","flat"];
netcdf.putAtt(groupID,variableID,'portableObservationValueLayout',char(layouts(variable.layout+1)));
if ~isempty(variable.dimensions)
    netcdf.putAtt(groupID,variableID,'portableObservationDimensionIdentifiers', ...
        char(variable.dimensions(1)),'NC_STRING');
    netcdf.putAtt(groupID,variableID,'portableObservationAxisCoordinateRoles', ...
        'identifier','NC_STRING');
end
if variable.units ~= ""
    netcdf.putAtt(groupID,variableID,'units',char(variable.units));
end
if variable.description ~= ""
    netcdf.putAtt(groupID,variableID,'long_name',char(variable.description));
end
roles = ["none","record-time","sample-time","x","y","z", ...
    "identifier","depth","pass","profile"];
if variable.role ~= 0
    netcdf.putAtt(groupID,variableID,'portableCoordinateRole',char(roles(variable.role+1)));
end
end

function request = runRequest(bundlePath,requestPath,destinationPath,reportPath,fileIdentifier,options)
integration = struct(method=options.integrationMethod, ...
    finalTime=options.integrationFinalTime,initialStep=options.initialStep);
if options.integrationMethod == "adaptive-rk23"
    integration.maximumStep = options.maximumStep;
    integration.relativeTolerance = options.relativeTolerance;
    integration.absoluteToleranceScale = options.absoluteToleranceScale;
end
destinations = containers.Map(char(fileIdentifier), ...
    char(relativePath(destinationPath,fileparts(requestPath))));
request = struct( ...
    schemaIdentifier="wave-vortex-run-request-v1", ...
    schemaVersion=1, ...
    modelFiles={{relativePath(bundlePath,fileparts(requestPath))}}, ...
    integration=integration, ...
    output=struct(policy=options.outputPolicy,destinations=destinations), ...
    execution=struct(fftProvider=options.fftProvider,threads=options.threads), ...
    report=relativePath(reportPath,fileparts(requestPath)));
end

function writeJSON(path,value)
temporaryPath = path + ".authoring-tmp";
cleanup = onCleanup(@() deleteIfPresent(temporaryPath));
fileID = fopen(temporaryPath,'wt');
if fileID == -1
    error("AlongTrackSimulator:PortableRequestOpenFailed", ...
        "Could not open '%s' for writing.",temporaryPath);
end
fileCleanup = onCleanup(@() closeTextFile(fileID));
fprintf(fileID,'%s\n',jsonencode(value,PrettyPrint=true));
fclose(fileID);
clear fileCleanup
[moved,message] = movefile(temporaryPath,path,'f');
if ~moved
    error("AlongTrackSimulator:PortableRequestMoveFailed", ...
        "Could not install '%s': %s",path,message);
end
clear cleanup
end

function closeNetCDF(fileID)
try
    netcdf.close(fileID);
catch exception
    if ~contains(exception.message,"Not a valid ID")
        rethrow(exception)
    end
end
end

function closeTextFile(fileID)
try
    fclose(fileID);
catch exception
    if ~contains(exception.message,"Invalid file identifier")
        rethrow(exception)
    end
end
end

function deleteIfPresent(path)
if isfile(path)
    delete(path);
end
end

function deleteAuthoredStaging(sourcePath,requestPath)
deleteIfPresent(sourcePath);
deleteIfPresent(requestPath);
end

function installAuthoredFile(stagedPath,finalPath,shouldOverwrite)
if shouldOverwrite
    [moved,message] = movefile(stagedPath,finalPath,'f');
else
    [moved,message] = movefile(stagedPath,finalPath);
end
if ~moved
    error("AlongTrackSimulator:PortableAuthoringInstallFailed", ...
        "Could not install '%s': %s",finalPath,message);
end
end

function path = absolutePath(path)
path = string(path);
if ~isAbsolutePath(path)
    path = fullfile(pwd,path);
end
path = string(java.io.File(char(path)).getCanonicalPath());
end

function tf = isAbsolutePath(path)
if ispc
    tf = ~isempty(regexp(path,'^[A-Za-z]:[\\/]|^\\\\','once'));
else
    tf = startsWith(path,"/");
end
end

function path = relativePath(path,folder)
path = string(path);
folder = string(folder);
prefix = folder + filesep;
if startsWith(path,prefix)
    path = extractAfter(path,strlength(prefix));
end
path = replace(path,filesep,"/");
end

function identifier = portableIdentifier(value)
identifier = regexprep(string(value),'[^A-Za-z0-9_.-]','-');
if identifier == ""
    identifier = "unnamed";
end
end

function mustBePortableIdentifier(value,name)
if isempty(regexp(value,'^[A-Za-z0-9_.-]+$','once'))
    error("AlongTrackSimulator:InvalidPortableIdentifier", ...
        "%s must contain only ASCII letters, digits, '_', '-', or '.'.",name);
end
end
