function build_website_documentation(options)
arguments
    options.rootDir = ".."
end
buildFolder = fullfile(options.rootDir,"docs");
sourceFolder = fullfile(options.rootDir,"Documentation","WebsiteDocumentation");

copyfile(sourceFolder,buildFolder);

changelogPath = fullfile(options.rootDir, "CHANGELOG.md");
if isfile(changelogPath)
    header = "---" + newline + ...
             "layout: default" + newline + ...
             "title: Version History" + newline + ...
             "nav_order: 100" + newline + ...
             "---" + newline + newline;
    versionHistoryText = header + fileread(changelogPath);
    versionHistoryFilePath = fullfile(options.rootDir,"docs","version-history.md");
    fid = fopen(versionHistoryFilePath, "w");
    assert(fid ~= -1, "Could not open CHANGELOG.md for writing");
    fwrite(fid, versionHistoryText);
    fclose(fid);
end

classFolderName = 'Class documentation';
websiteFolder = 'classes';
excludedSuperclasses = {'handle'};
classes = {'AlongTrackSimulator','WVAlongTrackObservingSystem','WVModelOutputGroupAlongTrack','WVModelOutputGroupAlongTrackRepeatCycle'};
classDocumentation = ClassDocumentation.empty(length(classes),0);
for iName=1:length(classes)
    classDocumentation(iName) = ClassDocumentation(classes{iName},nav_order=iName,buildFolder=buildFolder,excludedSuperclasses=excludedSuperclasses);
end
arrayfun(@(a) a.writeToFile(),classDocumentation)

end