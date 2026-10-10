# AGENTS.md — LibreOffice fork (cyberluke) working rules

Rules that apply to every agent working in this repository.

## No scaffolding

- **No scaffolding is allowed.** Do not create placeholder, stub, mock,
  skeleton, or "scaffold" files, classes, functions, tests, harnesses, or
  UI resources that are not a real, working implementation.
- A "scaffold" is anything created to look like progress without delivering
  the actual behavior: empty test modules, fake/dummy controllers, stubbed
  drawing surfaces, panning placeholder cards, "TODO will implement later"
  handlers, synthetic visual mockups, or golden-image stand-ins that are not
  real captures.
- Every file added or every implementation step must be a concrete, functional,
  buildable piece that is part of the finished feature — and must be verified
  to work, not merely to compile.
- If a piece cannot be delivered as a real working implementation and verified
  here, do not fabricate a scaffold for it; report it as not implemented with
  the exact reason.
- This applies to the Writer 2027 Type System / Style Gallery work and to all
  other work in the repository.

## Other working rules for this repository

- Keep Writer 2027 document mutation in the `sw` layer; keep reusable Type
  System data structures and custom drawing widgets in the `svx` layer. Do not
  introduce a sw<->svx circular dependency.
- Do not use `SfxObjectShell::Current()` / `SwModule::GetFirstView()` /
  document-global current-shell shortcuts in Writer 2027 code; resolve the
  owning frame explicitly.
- Do not log user document text; log operations/ids/counts only.
- Do not leave unbalanced `StartUndo`/`EndUndo` on exceptions; never report a
  clean preset when the apply failed.
- If the external build environment blocks `make build`/CppUnitTest/UITest,
  fix the blocker rather than working around it, and report exact status
  (COMPILED / RUNTIME-WIRED / FUNCTIONALLY-VERIFIED / VISUALLY-VERIFIED)
  instead of claiming PASS.