# Data & Infrastructure Layer Guardrails

1. **Model Isolation**: Keep `*Dto` (serialization annotations) in `remote/` and `*Entity` (ORM/SQL annotations) in `local/`. Never leak them outside `data/`.
2. **Stateless Mappers**: Implement pure transformation extensions/functions (`toDomain()`, `toEntity()`) tested with unit tests.
3. **Error Translation**: Wrap external I/O calls in a boundary translator that maps HTTP/DB failures into typed `DomainError` (always rethrowing coroutine/async cancellation).
