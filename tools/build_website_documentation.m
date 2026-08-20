function build_website_documentation(options)
arguments
    options.rootDir = ".."
    options.rebuildTutorials (1,1) logical = false
    options.buildTutorialDocumentation (1,1) logical = true
    options.buildClassDocumentation (1,1) logical = true
    options.buildFolder (1,1) string = ""
    options.previousBuildFolder (1,1) string = ""
end

validateDocumentationBuilder();
rootDir = char(java.io.File(char(options.rootDir)).getCanonicalPath());
if options.buildFolder == ""
    buildFolder = fullfile(rootDir,"docs");
else
    buildFolder = char(java.io.File(char(options.buildFolder)).getCanonicalPath());
end
if options.previousBuildFolder == ""
    existingBuildFolder = buildFolder;
else
    existingBuildFolder = char(java.io.File(char(options.previousBuildFolder)).getCanonicalPath());
end
sourceFolder = fullfile(rootDir, "Documentation", "WebsiteDocumentation");
tutorialSources = {
    fullfile(rootDir, "Examples", "Tutorials", "Modeling.m")
};
preservedRelativePaths = "figures/figure_0.png";
preservedRelativeDirectories = strings(0,1);
if ~strcmp(existingBuildFolder,buildFolder)
    for relativePath = preservedRelativePaths
        existingPath = fullfile(existingBuildFolder,relativePath);
        if isfile(existingPath)
            targetPath = fullfile(buildFolder,relativePath);
            targetFolder = fileparts(targetPath);
            if ~isfolder(targetFolder)
                mkdir(targetFolder);
            end
            copyfile(existingPath,targetPath);
        end
    end
end

previousTutorialBuildFolder = "";
if options.buildTutorialDocumentation
    if isfolder(fullfile(existingBuildFolder,"tutorials"))
        previousTutorialBuildFolder = tempname();
        mkdir(previousTutorialBuildFolder);
        copyfile(fullfile(existingBuildFolder,"tutorials"),fullfile(previousTutorialBuildFolder,"tutorials"));
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
    preservedRelativeDirectories = unique(string( ...
        {tutorialDocumentation.preservedAssetDirectoryRelativeToBuildFolder}))';
else
    copyPreservedDirectory(existingBuildFolder,buildFolder,"tutorials");
    preservedRelativeDirectories(end+1,1) = "tutorials";
end

if ~options.buildClassDocumentation
    copyPreservedDirectory(existingBuildFolder,buildFolder,"classes");
    preservedRelativeDirectories(end+1,1) = "classes";
end

rebuildWebsiteDocumentationFromSource( ...
    sourceFolder, ...
    buildFolder, ...
    preservedRelativePaths, ...
    preservedRelativeDirectories=unique(preservedRelativeDirectories,"stable"));

changelogPath = fullfile(rootDir, "CHANGELOG.md");
if isfile(changelogPath)
    header = "---" + newline + ...
             "layout: default" + newline + ...
             "title: Version History" + newline + ...
             "nav_order: 100" + newline + ...
             "---" + newline + newline;
    versionHistoryText = header + fileread(changelogPath);
    versionHistoryFilePath = fullfile(buildFolder,"version-history.md");
    fid = fopen(versionHistoryFilePath, "w");
    assert(fid ~= -1, "Could not open CHANGELOG.md for writing");
    fwrite(fid, versionHistoryText);
    fclose(fid);
end

if options.buildTutorialDocumentation
    TutorialDocumentation.writeMarkdownIndex( ...
        tutorialDocumentation, ...
        buildFolder=buildFolder, ...
        websiteFolder="tutorials", ...
        nav_order=4, ...
        description="Follow the tutorials to see how the simulator fits into modeling and analysis workflows.");
    arrayfun(@(a) a.writeToFile(), tutorialDocumentation)
    clear tutorialDocumentation
end
if previousTutorialBuildFolder ~= "" && isfolder(previousTutorialBuildFolder)
    rmdir(previousTutorialBuildFolder, "s");
end

if options.buildClassDocumentation
    rehash

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

end

function copyPreservedDirectory(existingBuildFolder,buildFolder,relativeDirectory)
sourceDirectory = fullfile(existingBuildFolder,relativeDirectory);
if strcmp(existingBuildFolder,buildFolder) || ~isfolder(sourceDirectory)
    return
end
targetDirectory = fullfile(buildFolder,relativeDirectory);
targetParent = fileparts(targetDirectory);
if ~isfolder(targetParent)
    mkdir(targetParent);
end
if isfolder(targetDirectory)
    rmdir(targetDirectory,"s");
end
copyfile(sourceDirectory,targetDirectory);
end
