# Domain Layer Guardrails (`modular-clean`)

1. **Zero Framework Leakage**: Domain files MUST NOT import `switch.h`, `borealis.hpp`, `curl/curl.h`.
2. **Ownership**: Domain owns business Entities, Value Objects, narrow Repository interfaces (ISP), UseCases/Policies, and `DomainError`.
3. **Pure Unit Tests**: Every Domain class must be testable in milliseconds using pure in-memory Fakes.
