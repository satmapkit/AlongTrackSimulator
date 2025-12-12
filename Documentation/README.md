Documentation
==============

+ The WebsiteDocumentation folder contains markdown files with documentation that is intended to be copied directly to the /docs folder in this repository and hosted with GitHub Pages.
+ The ../tools folder contains a script that will extract documentation from the classes, parse metadata, and then build markdown files which also get hosted with GitHub Pages.
+ This entire process is handled by ../tools/BuildWebsiteDocumentation.m
+ The ../tools/ci_release.m allows this process to be handled by CI on github.
