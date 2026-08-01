# SKSE-Shared

A collection of reusable C++ utilities and abstractions for SKSE plugin development.

## Purpose

This library provides common functionality to use across multiple SKSE projects, including:

- Utility functions and helper classes
- Common abstractions for SKSE-specific operations
- Reusable patterns and interfaces

## Integration

This repository is managed as a **Git subtree** and is embedded directly into consuming projects. It is not intended to be built independently.

### Adding to a Project

To integrate SKSE-Shared into your project:

```bash
git subtree add --prefix=src/shared https://github.com/KrisV-777/SKSE-Shared.git master
```

This adds the repository to your `src/shared` directory as part of your codebase.

### Updating from Upstream

To pull the latest changes from the upstream repository:

```bash
git subtree pull --prefix=src/shared https://github.com/KrisV-777/SKSE-Shared.git master --squash
```

## Contributing

Pull Requests are welcome. Changes made to this directory will be tracked and can be contributed back to the upstream repository.
