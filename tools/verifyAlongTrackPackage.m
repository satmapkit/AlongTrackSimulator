function report = verifyAlongTrackPackage(packageRoot)
%VERIFYALONGTRACKPACKAGE Install and exercise an isolated source-only package.
arguments
    packageRoot (1,1) string {mustBeFolder}
end

preferencesRoot = string(getenv("MATLAB_PREFDIR"));
if preferencesRoot == "" || ~isfolder(preferencesRoot)
    error("AlongTrackSimulator:PackageVerificationNotIsolated","Package verification requires an existing isolated MATLAB_PREFDIR.");
end
preferencesRoot = canonicalPath(preferencesRoot);
require(canonicalPath(string(prefdir)) == preferencesRoot, ...
    "MATLAB is not using the requested isolated preferences root.");
addOnRoot = string(getenv("ATS_MATLAB_ADDON_ROOT"));
if addOnRoot == "" || ~isfolder(addOnRoot)
    error("AlongTrackSimulator:PackageVerificationNotIsolated", ...
        "Package verification requires an existing disposable ATS_MATLAB_ADDON_ROOT.");
end
addOnRoot = canonicalPath(addOnRoot);
addOnSettings = settings;
configuredAddOnRoot = canonicalPath(string( ...
    addOnSettings.matlab.addons.InstallationFolder.ActiveValue));
require(configuredAddOnRoot == addOnRoot, ...
    "MATLAB is not using the requested disposable add-on root.");

packageRoot = canonicalPath(packageRoot);
expectedName = "AlongTrackSimulator";
expectedID = "7be8b3a1-bf4d-4362-8f78-f3e14f69cf3d";
expectedVersion = "1.1.0";
sourceDefinition = readPackageDefinition(packageRoot);
require(string(sourceDefinition.name) == expectedName && ...
    string(sourceDefinition.id) == expectedID && ...
    string(sourceDefinition.version) == expectedVersion, ...
    "The exported package definition does not have the expected name, ID, and version.");

sourceTreeManifest = treeManifest(packageRoot);
package = matlab.mpm.Package(packageRoot);
requirePackageIdentity(package,expectedName,expectedID,expectedVersion, ...
    "exported package");
require(string(package.ReleaseCompatibility) == ">=R2024b", ...
    "The package compatibility floor changed unexpectedly.");
sourceDigest = string(digest(package));
require(~isempty(regexp(sourceDigest,"^[0-9a-f]{64}$","once")), ...
    "The exported package did not produce a SHA-256 package digest.");

requiredPaths = [
    "CMakeLists.txt"
    "authorAlongTrackPortableRunBundle.m"
    "UnitTests/TestAlongTrackPortableRunBundle.m"
    "cpp/include/alongtrack/portable_core.hpp"
    "cpp/src/portable_core.cpp"
    "cpp/extensions/wavevortex/include/alongtrack/wavevortex_extension.hpp"
    "cpp/extensions/wavevortex/src/wavevortex_extension.cpp"
    "cpp/extensions/wavevortex/app/AlongTrackWaveVortexRunMain.cpp"
    "cpp/extensions/wavevortex/tests/check_runner_composition.cmake"
    "cpp/extensions/wavevortex/tests/test_source_linked_runner.cpp"
    "cpp/tests/data/matlab_reference.csv"
    "cpp/tests/data/wavevortex_matlab_reference.csv"
    "cpp/tests/data/ats4/matlab-authored-repeating.nc"
    "cpp/tests/data/ats4/matlab-authored-repeating-request.json"
    "cpp/tests/data/ats4/matlab-authored-geodetic.nc"
    "cpp/tests/data/ats4/matlab-authored-geodetic-request.json"
    "cpp/tests/data/ats4/README.md"
    "cpp/tests/data/ats4/benchmark/README.md"
    "cpp/tests/data/ats4/benchmark/matlab-authored-builtin-output.nc"
    "cpp/tests/data/ats4/benchmark/matlab-authored-builtin-output-request.json"
    "resources/mpackage.json"
    ];
for relativePath = requiredPaths'
    require(isfile(fullfile(packageRoot,relativePath)),"The source-only package is missing " + relativePath + ".");
end

require(~isfolder(fullfile(packageRoot,"cpp","tests","data","ats4", ...
    "benchmark","raw")), ...
    "Raw benchmark reports with host paths must remain authoring-only.");

for excludedFolder = [".git" ".github" ".buildtool" "Documentation" "docs" "tools" "dist"]
    excludedPath = fullfile(packageRoot,excludedFolder);
    require(~isfolder(excludedPath) && ~isfile(excludedPath),"The source-only package contains authoring path " + excludedFolder + ".");
end

installationParent = string(tempname);
mkdir(installationParent);
installationCleanup = onCleanup(@()rmdir(installationParent,"s"));
installationRoot = fullfile(installationParent,"AlongTrackSimulator-package-under-test");
copyfile(packageRoot,installationRoot);
installationRoot = canonicalPath(installationRoot);
require(isequal(sourceTreeManifest,treeManifest(installationRoot)), ...
    "The disposable pre-install copy does not match the audited export.");
stagedPackage = matlab.mpm.Package(installationRoot);
requirePackageIdentity(stagedPackage,expectedName,expectedID,expectedVersion, ...
    "pre-install package copy");
require(string(digest(stagedPackage)) == sourceDigest, ...
    "The disposable pre-install copy does not match the exported package digest.");

packagesBefore = mpmlist;
require(isempty(packagesBefore), ...
    "The isolated MATLAB session already contains MPM packages; a sibling or user package could satisfy the verification.");
for symbol = ["AlongTrackSimulator" "authorAlongTrackPortableRunBundle"]
    require(isempty(resolvedSymbolPaths(symbol)), ...
        symbol + " was visible before installing the package under test.");
end

installedByCall = mpminstall(installationRoot,Prompt=false,Temporary=true, ...
    PathPosition="beginning",Verbosity="detailed");
require(isscalar(installedByCall), ...
    "MPM did not return exactly one requested package installation.");
requirePackageIdentity(installedByCall,expectedName,expectedID,expectedVersion, ...
    "package returned by mpminstall");
require(installedByCall.Installed && ~installedByCall.Editable && ...
    ~installedByCall.InstalledAsDependency, ...
    "MPM did not return a managed, noneditable, explicitly requested package.");
installedRoot = canonicalPath(string(installedByCall.PackageRoot));
require(isfolder(installedRoot),"The installed MPM PackageRoot does not exist.");
require(installedRoot ~= packageRoot && installedRoot ~= installationRoot, ...
    "MPM did not materialize a distinct managed package installation.");
require(isPathWithin(installedRoot,addOnRoot), ...
    "MPM installed AlongTrackSimulator outside the disposable add-on root.");

packages = mpmlist;
packageNames = reshape(string([packages.Name]),[],1);
packageIDs = reshape(string([packages.ID]),[],1);
installedPackageRoots = strings(numel(packages),1);
for iPackage = 1:numel(packages)
    installedPackageRoots(iPackage) = canonicalPath(string(packages(iPackage).PackageRoot));
    require(isPathWithin(installedPackageRoots(iPackage),addOnRoot), ...
        string(packages(iPackage).Name) + ...
        " resolved outside the disposable add-on root.");
end
matches = find(packageNames == expectedName | packageIDs == expectedID);
require(isscalar(matches), ...
    "The isolated installation did not contain exactly one package with the expected AlongTrack name or ID.");
installed = packages(matches);
requirePackageIdentity(installed,expectedName,expectedID,expectedVersion, ...
    "package listed by MPM");
require(canonicalPath(string(installed.PackageRoot)) == installedRoot, ...
    "The package returned by mpminstall and the package listed by MPM have different roots.");

installedDigest = string(digest(installed));
require(installedDigest == sourceDigest, ...
    "The installed package digest does not match the audited export.");
require(isequal(sourceTreeManifest,treeManifest(installedRoot)), ...
    "The installed package content does not match the audited export file-for-file.");

simulatorPath = string(which("AlongTrackSimulator"));
authoringPath = string(which("authorAlongTrackPortableRunBundle"));
require(simulatorPath ~= "","AlongTrackSimulator was not visible after installation.");
require(authoringPath ~= "","The portable-bundle author was not visible after installation.");
requireResolvedOnlyFrom("AlongTrackSimulator",installedRoot);
requireResolvedOnlyFrom("authorAlongTrackPortableRunBundle",installedRoot);

simulator = AlongTrackSimulator();
parameters = simulator.missionParameters("j3");
require(isfinite(parameters.semi_major_axis) && parameters.semi_major_axis > 0,"The installed simulator did not resolve the canonical mission catalog.");
require(isequal(sourceTreeManifest,treeManifest(packageRoot)), ...
    "Package installation mutated the audited source-only export.");
require(isequal(sourceTreeManifest,treeManifest(installationRoot)), ...
    "Package installation mutated the disposable pre-install copy.");

report = struct("packageName",expectedName,"packageID",expectedID, ...
    "version",expectedVersion, ...
    "releaseCompatibility",string(package.ReleaseCompatibility), ...
    "packageRoot",packageRoot,"stagingRoot",installationRoot, ...
    "installedPackageRoot",installedRoot,"sourceDigest",sourceDigest, ...
    "installedDigest",installedDigest, ...
    "preferencesRoot",preferencesRoot,"addOnRoot",addOnRoot, ...
    "installedPackageRoots",installedPackageRoots, ...
    "missionSemiMajorAxis",parameters.semi_major_axis);
fprintf("Verified isolated AlongTrackSimulator %s source package at %s.\n",string(package.Version),packageRoot);
clear installationCleanup
end

function definition = readPackageDefinition(packageRoot)
manifestPath = fullfile(packageRoot,"resources","mpackage.json");
if ~isfile(manifestPath)
    error("AlongTrackSimulator:PackageVerificationFailed", ...
        "Package manifest not found at %s.",manifestPath);
end
definition = jsondecode(fileread(manifestPath));
end

function requirePackageIdentity(package,expectedName,expectedID,expectedVersion,description)
require(string(package.Name) == expectedName && ...
    string(package.ID) == expectedID && ...
    string(package.Version) == expectedVersion, ...
    "The " + description + " does not have the expected name, ID, and version.");
end

function requireResolvedOnlyFrom(symbol,installedRoot)
resolvedPaths = resolvedSymbolPaths(symbol);
require(isscalar(resolvedPaths), ...
    symbol + " did not resolve uniquely from the isolated installation.");
require(isPathWithin(canonicalPath(resolvedPaths),installedRoot), ...
    symbol + " resolved from a sibling or user package instead of the installed MPM PackageRoot.");
end

function paths = resolvedSymbolPaths(symbol)
paths = string(which(symbol,"-all"));
paths = reshape(paths(paths ~= ""),[],1);
end

function tf = isPathWithin(path,root)
tf = path == root || startsWith(path,root + filesep);
end

function require(condition,message)
if ~condition
    error("AlongTrackSimulator:PackageVerificationFailed","%s",message);
end
end

function path = canonicalPath(path)
path = string(java.io.File(char(path)).getCanonicalPath());
end

function manifest = treeManifest(root)
listing = dir(fullfile(root,"**","*"));
if isempty(listing)
    manifest = strings(0,1);
    return
end
absolutePaths = string(fullfile({listing.folder},{listing.name}));
relativePaths = erase(reshape(absolutePaths,[],1),string(root) + filesep);
[relativePaths,order] = sort(relativePaths);
listing = listing(order);
manifest = strings(numel(listing),1);
for iEntry = 1:numel(listing)
    if listing(iEntry).isdir
        manifest(iEntry) = "directory " + relativePaths(iEntry);
    else
        manifest(iEntry) = "file " + relativePaths(iEntry) + " " + fileHash(fullfile(root,relativePaths(iEntry)));
    end
end
end

function hash = fileHash(path)
fileID = fopen(path,"r");
if fileID == -1
    error("AlongTrackSimulator:PackageVerificationReadFailed","Could not read %s.",path);
end
fileCleanup = onCleanup(@()fclose(fileID));
bytes = fread(fileID,Inf,"*uint8");
digest = java.security.MessageDigest.getInstance("SHA-256");
digest.update(typecast(bytes,"int8"));
hashBytes = typecast(digest.digest(),"uint8");
hash = lower(string(reshape(dec2hex(hashBytes,2).',1,[])));
clear fileCleanup
end
