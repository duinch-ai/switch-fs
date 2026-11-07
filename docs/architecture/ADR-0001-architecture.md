# ADR-0001: Project Base Architecture, Security Gates & Multi-Agent Standards

- **Date**: 2026-11-07 | **Status**: Accepted | **Project**: switch-fs | **Stack**: C++ / Nintendo Switch Homebrew (CMake + libnx + Borealis)

## Decision
- **Architecture Style**: `modular-clean` | **Flow Pattern**: `mvc-borealis`
- **Reference Architecture Anchor**: [`dragonflylee/switchfin`](https://github.com/dragonflylee/switchfin)
- **Boundary Enforcement**: CMake target boundary isolation (domain pure C++ library, presentation Borealis app) + clang-tidy check
- **File Size Policy**: `1 File = 1 Primary Responsibility`, warn at `300 LOC`, hard block at `450 LOC`.
- **Security & Git Gates**: Staged secret scanning, OWASP heuristics, Conventional Commits, and pre-push test/CVE verification via `.githooks/`.

### Selected Base Capabilities
- `solid-dry`
- `typed-errors`
- `di-core`
- `network-storage`
- `ui-nav`
- `arch-lint`
- `testing-ci`
