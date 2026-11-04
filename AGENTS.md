# Repository guidance for agents

## Project context

- This is a legacy Netscape browser source tree with vintage C/C++, Java, JavaScript, make rules, platform-specific code, and localization resources.
- Preserve the existing style and compatibility assumptions of the files you touch. Avoid broad modernization, mechanical reformatting, or unrelated cleanup.
- `README.md` describes the repository layout and historical build context. `README.jwz` records branch-specific changes and is not a general project specification.

## Working in the tree

- Keep changes narrowly scoped to the requested task.
- Check nearby code and platform-specific variants before changing shared interfaces or build rules.
- Do not restore CVS metadata directories. They are not source files and have been removed from this repository.
- Do not claim a build or test passed unless it was actually run in a compatible environment. The historical build may need platform-specific tools and dependencies.
- If a change cannot be verified with the available toolchain, state what was inspected and what remains unverified.

## Documentation

- Use Markdown for new repository-level documentation.
- Keep instructions factual and distinguish verified project behavior from historical context or assumptions.
