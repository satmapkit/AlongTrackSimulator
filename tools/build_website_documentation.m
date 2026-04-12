function build_website_documentation(options)
arguments
    options.rootDir = ".."
    options.rebuildTutorials (1,1) logical = false
end

rootDir = char(java.io.File(char(options.rootDir)).getCanonicalPath());
buildFolder = fullfile(rootDir, "docs");
sourceFolder = fullfile(rootDir, "Documentation", "WebsiteDocumentation");
tutorialSources = {
    fullfile(rootDir, "Examples", "Tutorials", "Modeling.m")
};
preservedRelativePaths = "figures/figure_0.png";

previousTutorialBuildFolder = "";
if isfolder(fullfile(buildFolder, "tutorials"))
    previousTutorialBuildFolder = tempname();
    mkdir(previousTutorialBuildFolder);
    copyfile(fullfile(buildFolder, "tutorials"), fullfile(previousTutorialBuildFolder, "tutorials"));
end

tutorialDocumentation = TutorialDocumentation.documentationFromSourceFiles( ...
    tutorialSources, ...
    buildFolder=buildFolder, ...
    websiteRootURL="AlongTrackSimulator/", ...
    websiteFolder="tutorials", ...
    sourceRoot=rootDir, ...
    previousBuildFolder=previousTutorialBuildFolder, ...
    executionPaths=string(rootDir), ...
    rebuildTutorials=options.rebuildTutorials);
preservedTutorialDirectories = unique(string({tutorialDocumentation.preservedAssetDirectoryRelativeToBuildFolder}))';

rebuildWebsiteDocumentationFromSource( ...
    sourceFolder, ...
    buildFolder, ...
    preservedRelativePaths, ...
    preservedRelativeDirectories=preservedTutorialDirectories);

changelogPath = fullfile(rootDir, "CHANGELOG.md");
if isfile(changelogPath)
    header = "---" + newline + ...
             "layout: default" + newline + ...
             "title: Version History" + newline + ...
             "nav_order: 100" + newline + ...
             "---" + newline + newline;
    versionHistoryText = header + fileread(changelogPath);
    versionHistoryFilePath = fullfile(rootDir, "docs", "version-history.md");
    fid = fopen(versionHistoryFilePath, "w");
    assert(fid ~= -1, "Could not open CHANGELOG.md for writing");
    fwrite(fid, versionHistoryText);
    fclose(fid);
end

TutorialDocumentation.writeMarkdownIndex( ...
    tutorialDocumentation, ...
    buildFolder=buildFolder, ...
    websiteFolder="tutorials", ...
    nav_order=4, ...
    description="Follow the tutorials to see how the simulator fits into modeling and analysis workflows.");
arrayfun(@(a) a.writeToFile(), tutorialDocumentation)
clear tutorialDocumentation
if previousTutorialBuildFolder ~= "" && isfolder(previousTutorialBuildFolder)
    rmdir(previousTutorialBuildFolder, "s");
end

evalin('base', 'clear classes');
evalin('base', 'rehash');

websiteRootURL = "AlongTrackSimulator/";
classFolderName = 'Class documentation';
websiteFolder = 'classes';
excludedSuperclasses = {'handle', 'WVObservingSystem', 'WVModelOutputGroup', 'matlab.mixin.Heterogeneous', 'CAAnnotatedClass'};
classes = {'AlongTrackSimulator', 'WVAlongTrackObservingSystem', 'WVModelOutputGroupAlongTrack'};
classDocumentation = ClassDocumentation.empty(length(classes), 0);
for iName = 1:length(classes)
    classDocumentation(iName) = ClassDocumentation(classes{iName}, nav_order=iName, websiteRootURL=websiteRootURL, buildFolder=buildFolder, websiteFolder=websiteFolder, parent=classFolderName, excludedSuperclasses=excludedSuperclasses);
end
arrayfun(@(a) a.writeToFile(), classDocumentation)

end
