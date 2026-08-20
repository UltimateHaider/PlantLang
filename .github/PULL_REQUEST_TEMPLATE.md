## Description

<!-- A clear and concise description of what this pull request does and why. -->

Fixes #<!-- issue number -->

## Type of Change

<!-- Tick the boxes that apply (keep the line as `- [x] ...`). -->

- [ ] Bug fix (non-breaking change that fixes an issue)
- [ ] New feature (non-breaking change that adds functionality)
- [ ] Language/compiler change (lexer, parser, codegen)
- [ ] Runtime change (`runtime/c/`)
- [ ] Documentation only (CHANGELOG, Language Tour, GAP_ANALYSIS, etc.)
- [ ] Infrastructure (build system, CI, community files)
- [ ] Breaking change (fix or feature that changes existing behavior)

## Testing Checklist

<!-- Describe the verification performed. The repo has four suites: -->

- [ ] `make all` — self-hosted compiler builds cleanly
- [ ] `make self` — self-hosting convergence check passes
- [ ] `make test` — all suites pass
      (native 20/20, generics 7/7, closures 6/6, regression 157/157)
- [ ] Added or updated regression tests (`tests/regression/*.plant` +
      `.expected`) if behavior changed

## Checklist

- [ ] My code follows the project's conventions (two-space indent, no
      comments unless needed, v1-bootstrap-safe constructs in `.plant`)
- [ ] I have read the [Contributing Guide](docs/CONTRIBUTING.md)
- [ ] I updated the CHANGELOG (and Language Tour / GAP_ANALYSIS when
      user-facing behavior changed)
- [ ] No secrets or credentials are committed
- [ ] My changes are limited to the scope described above