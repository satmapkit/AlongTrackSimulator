function dependency = validateDocumentationBuilder()
%VALIDATEDOCUMENTATIONBUILDER Require the reproducible documentation package.
requiredVersion = "1.3.2";
classPath = string(which("ClassDocumentation"));
if classPath == ""
    error("AlongTrackSimulator:DocumentationBuilderMissing","Documentation generation requires ClassDocumentation %s.",requiredVersion);
end

packageRoot = string(fileparts(classPath));
manifestPath = fullfile(packageRoot,"resources","mpackage.json");
if ~isfile(manifestPath)
    error("AlongTrackSimulator:DocumentationBuilderInvalid","ClassDocumentation resolved from %s, which has no package manifest.",packageRoot);
end
manifest = jsondecode(fileread(manifestPath));
actualVersion = string(manifest.version);
if actualVersion ~= requiredVersion
    error("AlongTrackSimulator:DocumentationBuilderVersionMismatch","Documentation generation requires ClassDocumentation %s, but %s resolved from %s.",requiredVersion,actualVersion,packageRoot);
end

rebuildHelper = string(which("rebuildWebsiteDocumentationFromSource"));
if rebuildHelper == "" || ~startsWith(rebuildHelper,packageRoot + filesep)
    error("AlongTrackSimulator:DocumentationBuilderInvalid","The documentation rebuild helper did not resolve from ClassDocumentation %s.",requiredVersion);
end
dependency = struct("Name","ClassDocumentation","Version",actualVersion,"Root",packageRoot);
end
