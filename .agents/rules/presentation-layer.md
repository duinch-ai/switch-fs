# Presentation Layer Guardrails (`mvc-borealis`)

1. **Unidirectional Data Flow (UDF)**: Views observe immutable `Sealed UiState` and emit user `Intent` / action callbacks upward.
2. **Dumb Previewable Views**: Pass state slices and event lambdas into UI components (not whole ViewModels/Stores). Split sub-components into `components/` when a screen passes ~200 LOC.
3. **No Framework Leaks in State Holders**: State holders (`ViewModel`, `BLoC`, `Reducer`, `Store`) never hold UI widget, `Context`, `BuildContext`, or DOM references.
