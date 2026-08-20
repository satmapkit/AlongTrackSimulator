function plan = buildfile
import matlab.buildtool.Task
import matlab.buildtool.TaskGroup

documentationTasks = [
    Task(Actions=@documentationBuildTask,Description="Build committed website documentation.",DisableIncremental=true)
    Task(Actions=@documentationCheckTask,Description="Verify committed documentation against a clean generated site.",DisableIncremental=true)
    ];

plan = buildplan;
plan("docs") = TaskGroup(documentationTasks,TaskNames=["build"; "check"],Description="Build or verify website documentation.");
plan.DefaultTasks = "docs:check";
end

function documentationBuildTask(~)
repositoryRoot = fileparts(mfilename("fullpath"));
cacheCleanup = onCleanup(@()removeBuildtoolCache(repositoryRoot));
withToolsPath(@()build_website_documentation( ...
    rootDir=repositoryRoot, ...
    buildTutorialDocumentation=false, ...
    buildClassDocumentation=false));
clear cacheCleanup
end

function documentationCheckTask(~)
repositoryRoot = fileparts(mfilename("fullpath"));
cacheCleanup = onCleanup(@()removeBuildtoolCache(repositoryRoot));
withToolsPath(@()check_website_documentation(rootDir=repositoryRoot));
clear cacheCleanup
end

function withToolsPath(action)
repositoryRoot = fileparts(mfilename("fullpath"));
originalPath = path;
pathCleanup = onCleanup(@()path(originalPath));
addpath(fullfile(repositoryRoot,"tools"));
action();
clear pathCleanup
end

function removeBuildtoolCache(repositoryRoot)
cacheRoot = fullfile(repositoryRoot,".buildtool");
if isfolder(cacheRoot)
    rmdir(cacheRoot,"s");
end
end
