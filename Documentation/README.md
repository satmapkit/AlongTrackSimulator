# Documentation

- `WebsiteDocumentation` contains the canonical hand-authored Markdown and website assets.
- `Verification` contains exact authoring-time acceptance evidence, such as raw benchmark reports. It is not copied into the website or source-only MATLAB package.
- `../tools/build_website_documentation.m` combines the canonical pages, generated class reference, tutorials, and version history in `../docs`.
- Run `buildtool docs:build` after a coherent hand-authored documentation or changelog batch. It refreshes those mirrors while preserving cached class-reference and tutorial outputs. Do not edit generated pages first.
- Run `build_website_documentation` directly for a deliberate class-reference or tutorial refresh, then review the generated API pages and media as one batch.
- Run `buildtool docs:check` to compare the committed canonical mirrors with a clean staged build. CI runs the same check with an isolated MATLAB installation and the pinned documentation generator.
