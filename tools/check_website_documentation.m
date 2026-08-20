function comparison = check_website_documentation(options)
%CHECK_WEBSITE_DOCUMENTATION Compare committed documentation with a clean build.
arguments
    options.rootDir (1,1) string = ""
end

if options.rootDir == ""
    options.rootDir = string(fileparts(fileparts(mfilename("fullpath"))));
end
repositoryRoot = canonicalPath(options.rootDir);
validateDocumentationBuilder();
committedFolder = fullfile(repositoryRoot,"docs");
if ~isfolder(committedFolder)
    error("AlongTrackSimulator:DocumentationMissing","The committed documentation folder is missing: %s",committedFolder);
end

stagingRoot = string(tempname);
mkdir(stagingRoot);
stagingCleanup = onCleanup(@()rmdir(stagingRoot,"s"));
generatedFolder = fullfile(stagingRoot,"docs");
build_website_documentation( ...
    rootDir=repositoryRoot, ...
    buildFolder=generatedFolder, ...
    previousBuildFolder=committedFolder, ...
    buildTutorialDocumentation=false, ...
    buildClassDocumentation=false);
comparison = compareTrees(committedFolder,generatedFolder);
fprintf("Documentation comparison: added=%d, removed=%d, modified=%d\n",numel(comparison.Added),numel(comparison.Removed),numel(comparison.Modified));
printPaths("Added",comparison.Added);
printPaths("Removed",comparison.Removed);
printPaths("Modified",comparison.Modified);
if ~comparison.IsEqual
    error("AlongTrackSimulator:DocumentationOutOfDate","Committed documentation does not match a clean generated build.");
end
clear stagingCleanup
end

function comparison = compareTrees(expectedRoot,actualRoot)
expectedPaths = relativeFilePaths(expectedRoot);
actualPaths = relativeFilePaths(actualRoot);
added = setdiff(actualPaths,expectedPaths);
removed = setdiff(expectedPaths,actualPaths);
common = intersect(expectedPaths,actualPaths);
modified = strings(0,1);
for iPath = 1:numel(common)
    expectedBytes = readBytes(fullfile(expectedRoot,common(iPath)));
    actualBytes = readBytes(fullfile(actualRoot,common(iPath)));
    if ~isequal(expectedBytes,actualBytes)
        modified(end+1,1) = common(iPath); %#ok<AGROW>
    end
end
comparison = struct("IsEqual",isempty(added) && isempty(removed) && isempty(modified),"Added",added,"Removed",removed,"Modified",modified);
end

function paths = relativeFilePaths(root)
listing = dir(fullfile(root,"**","*"));
listing = listing(~[listing.isdir]);
if isempty(listing)
    paths = strings(0,1);
    return
end
absolutePaths = string(fullfile({listing.folder},{listing.name}));
paths = sort(erase(reshape(absolutePaths,[],1),string(root) + filesep));
end

function bytes = readBytes(path)
fileID = fopen(path,"r");
if fileID == -1
    error("AlongTrackSimulator:DocumentationReadFailed","Could not read %s.",path);
end
fileCleanup = onCleanup(@()fclose(fileID));
bytes = fread(fileID,Inf,"*uint8");
clear fileCleanup
end

function printPaths(label,paths)
if isempty(paths)
    return
end
fprintf("%s:\n  %s\n",label,strjoin(paths,newline + "  "));
end

function path = canonicalPath(path)
path = string(java.io.File(char(path)).getCanonicalPath());
end
