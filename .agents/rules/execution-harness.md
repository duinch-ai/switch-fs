# Task Execution Harness (Mandatory for Every Coding Request)

1. **Gate 1 — Grounding & Assumptions**: Read `.agents/CODEBASE_MAP.md`, `.agents/memory/lessons.md`, and `referenceRepo` conventions. Surface assumptions upfront; track > 3-file work in `.agents/memory/active-plan.md`.
2. **Gate 2 — Surgical File Split**: Touch only required files/lines (`karpathy-skills`). Keep 1 primary responsibility per file (`> 300 LOC` = WARN, `> 450 LOC` = FAIL).
3. **Gate 3 — Implementation & 3-Strike Debugging**:
   - **Feature**: `Domain` (entities, narrow ports, `DomainError`) -> `Data` (DTO/Entity, Mappers, RepoImpl) -> `State Holder` (Sealed `UiState`, `Intent`) -> `Stateless UI` -> `DI/Nav`.
   - **Bugfix**: Trace root cause; test 1 hypothesis at a time; **STOP after 3 failed attempts** (`obra/superpowers`) to reassess architecture. Ban blind `try/catch`, arbitrary delays, and unsafe casts.
   - **No Leftovers**: Ban `// TODO`, fake production returns, unused imports, or swallowed `CancellationException`/`CancelledError`. 
4. **Gate 4 — Edge-Case Unit Tests**: Cover happy path, empty state, offline/error mapping, and invalid inputs using fast in-memory Fakes.
5. **Gate 5 — Paranoid Check & Self-Healing**: Audit diff for race conditions, N+1 queries, and unbounded lists. Run `ctest --test-dir build -R <test_name>` and `clang-format --dry-run -Werror $(find app -name '*.cpp' -o -name '*.hpp') && clang-tidy -p build app/src/*.cpp` until green; run the stack security audit before delivery.
6. **Gate 6 — Lessons & Terse Output**: Append new project gotchas to `.agents/memory/lessons.md`; deliver concise high-signal summary (`caveman`).
