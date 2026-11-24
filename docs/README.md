# PlantLang Documentation

> Version: v0.63.3 · Last reviewed: 2026-11-24
>
> Start with the root [README](../README.md) for installation and a project
> overview. This page is the complete index of documents under `docs/`.

## Start here

| Document | What it covers |
|----------|----------------|
| [ROADMAP.md](ROADMAP.md) | Completed milestones, upcoming work, and backlog |
| [CHANGELOG.md](CHANGELOG.md) | Full version history through v0.63.3 |
| [RELEASES.md](RELEASES.md) | Release timeline and artifact sizes |

## Language reference

| Document | What it covers |
|----------|----------------|
| [Language Tour.md](Language%20Tour.md) | Complete ground-up syntax and mechanics guide |
| [DATA_TYPES.md](DATA_TYPES.md) | Type system reference |
| [COERCION_COVERAGE.md](COERCION_COVERAGE.md) | Numeric coercion rules and coverage |
| [GLOBALS_RECIPE.md](GLOBALS_RECIPE.md) | Global variables and the type registry |
| [FFI_GUIDE.md](FFI_GUIDE.md) | Calling C from PlantLang |

## Build, test, and release

| Document | What it covers |
|----------|----------------|
| [BUILD.md](BUILD.md) | Build system, bootstrap chain, Windows/MSYS2, packaging |
| [TESTING_STRATEGY.md](TESTING_STRATEGY.md) | Test pyramid, suites, and release checks |
| [PRODUCTION_GUIDE.md](PRODUCTION_GUIDE.md) | Production deployment guidance |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution workflow and PR checklist |
| [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | Community standards |
| [SECURITY.md](SECURITY.md) | Vulnerability reporting policy |

## Architecture and internals

| Document | What it covers |
|----------|----------------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Compiler architecture and pipeline |
| [TECHNICAL.md](TECHNICAL.md) | Deep technical reference: codegen and memory model |
| [EVAPORATE.md](EVAPORATE.md) | Memory management and ownership |
| [TECH_DEBT.md](TECH_DEBT.md) | Technical debt register |
| [DEVELOPMENT_PRINCIPLES.md](DEVELOPMENT_PRINCIPLES.md) | Engineering principles |
| [GAP_ANALYSIS.md](GAP_ANALYSIS.md) | Legacy JS vs native comparison — **archival** |

## Numerics, tensors, and neural networks

| Document | What it covers |
|----------|----------------|
| [TENSOR.md](TENSOR.md) | PlantTensor type, operations, and roadmap (authoritative) |
| [TENSOR_UTILS_RECIPE.md](TENSOR_UTILS_RECIPE.md) | Tensor utility recipes |
| [ACTIVATIONS_RECIPE.md](ACTIVATIONS_RECIPE.md) | Activation functions |
| [CONV2D_RECIPE.md](CONV2D_RECIPE.md) | Convolution layers |
| [NORM_RECIPE.md](NORM_RECIPE.md) | Normalization layers |
| [DROPOUT_RECIPE.md](DROPOUT_RECIPE.md) | Dropout |
| [DENSE_TRAINING_RECIPE.md](DENSE_TRAINING_RECIPE.md) | Dense-layer training |
| [NN_EXAMPLE.md](NN_EXAMPLE.md) | Full neural-network example |
| [NN_LIMITATIONS.md](NN_LIMITATIONS.md) | Neural-network limitations |
| [ADV2_LIMITATIONS.md](ADV2_LIMITATIONS.md) | Higher-order AD limitations |
| [PDE_LIMITATIONS.md](PDE_LIMITATIONS.md) | PDE solver limitations |

## Authoritative sources

| Topic | Source of truth |
|-------|-----------------|
| Current version | `Makefile` (`VERSION`) and `CHANGELOG.md` |
| Test counts | `TESTING_STRATEGY.md`, verified per release |
| Build and bootstrap rules | `BUILD.md` |
| Roadmap | `ROADMAP.md`; subsystem plans live in subsystem docs |
| Tensor API and roadmap | `TENSOR.md` |
| Technical debt | `TECH_DEBT.md` |
| Security policy | `SECURITY.md` |

## Documentation rules

- Keep one roadmap: `ROADMAP.md`. Subsystem plans belong in the subsystem
  document (for example, tensor work belongs in `TENSOR.md`).
- Update `CHANGELOG.md` and all version references in the same commit as the
  release.
- Mark historical documents as **archival**; do not present them as current.
- Do not commit build artifacts or generated reports.
- Recipes should link back to the authoritative subsystem document.
