# Writer 2027 — Type System + Typography Browser Remediation Specification

**Audience:** junior C++ / LibreOffice developer  
**Repository:** `cyberluke/libreoffice-fork`  
**Branch:** `lukas_dev`  
**Baseline commit:** `f7817ebf4baff492fde15550ed5bb9047070e51e`  
**Primary scope:** Writer 2027 Type System picker + custom font / Typography Browser popup  
**Target:** production-quality implementation, deterministic UI behavior, crash-safe diagnostics, automated semantic + visual regression coverage  
**Status:** implementation specification, not a claim that the fixes have already been built or verified

---

## 0. Executive decision

Do **not** continue patching the current Type System popup by adding more coordinate arithmetic or more custom row painting.

The correct remediation is:

1. **Replace the current Type System toolbar-opening path with LibreOffice's native popup-controller architecture**, i.e. `svt::PopupWindowController` + `WeldToolbarPopup`, following a working in-tree pattern such as `TextUnderlinePopup`.
2. **Bind the controller to the actual frame that owns the toolbar button.** Do not use `SwModule::GetFirstView()`.
3. **Keep the existing Type System catalog and Writer style-application backend.** Those parts are already substantially implemented and should not be rewritten unless tests expose a functional defect.
4. **Rebuild the Type System UI as a native, measured, non-overlapping control hierarchy.** Do not custom-paint several font specimens inside every `TreeView` row.
5. **Fix the Typography Browser custom renderer by measuring real glyph bounds, clipping every row, and returning the actual content width instead of a hard-coded `200` px.**
6. Add three complementary test layers:
   - C++/CppUnit for Type System data + style application.
   - LibreOffice `UITest` Python tests for real UI behavior, roughly the native-LO equivalent of Jest/Playwright interaction tests.
   - LibreOffice screenshot capture + deterministic golden-image comparison for proof that the UI actually renders correctly.
7. Add structured exception logging at all UI/action boundaries and preserve hard-crash capture. Normal C++/UNO exceptions should be contained; access violations must be logged and then allowed to terminate rather than trying to continue a corrupted process.

The acceptance gate is **not** "the code looks correct". The gate is:

- automated UI interaction passes,
- geometry invariants pass,
- Type System actually applies the correct styles,
- visual goldens pass at the defined DPI matrix,
- no crash occurs in repeated open/close and multi-document scenarios,
- failure artifacts make regressions indisputable.

---

# 1. What is broken today

There are two separate visible failures plus one architecture problem.

## 1.1 Type System opens in the top-left / sidebar-like location

Current controller code obtains the rectangle of the toolbar item:

```cpp
ToolBox* pToolBox = nullptr;
ToolBoxItemId nId;
if (getToolboxId(nId, &pToolBox))
    aAnchorRect = pToolBox->GetItemRect(nId);
```

That rectangle is in the **ToolBox coordinate system**.

The code then calls:

```cpp
pView->OpenWriter2027TypeSystemPopup(aAnchorRect);
```

Inside `SwView::OpenWriter2027TypeSystemPopup`, the rectangle's original owner is discarded and the code substitutes the whole Writer frame window:

```cpp
vcl::Window* pAnchor = &GetViewFrame().GetWindow();
m_xWriter2027TypeSystemPopup->Open(
    pFontList,
    aCurrentPresetId,
    *pAnchor,
    rAnchorRect);
```

Finally, `Writer2027TypeSystemPopup::Open()` treats that rectangle as though it were expressed in the coordinate space of the Writer frame:

```cpp
weld::Window* pParent = weld::GetPopupParent(rAnchorWin, rAnchorRect);
m_xPopup->popup_at_rect(pParent, rAnchorRect, weld::Placement::Under);
```

This is a coordinate-space ownership bug.

A rectangle is meaningless without the window whose coordinate system it belongs to.

If the Type System item is located at `x=0` or near `x=0` inside a notebookbar ToolBox, that same `x=0` is later interpreted as `x=0` of the entire Writer frame. The visible result is exactly what has been reported: the picker appears on the far left instead of under the button.

### Mandatory conclusion

Do not "fix" this by adding offsets based on the notebookbar, document window, title bar, monitor origin, or DPI scale. That only creates another fragile coordinate conversion path.

The popup framework already knows the toolbar item and should own positioning.

---

## 1.2 Type System popup height is computed before the Type System model exists

Current `Writer2027TypeSystemPopup::Open()` calculates the content height from `maRowIds`:

```cpp
tools::Long nContentH = 0;
for (const auto& rId : maRowIds)
    nContentH += nTextH * 2;
```

but calls:

```cpp
RebuildRows();
```

**after** the geometry calculation and `set_size_request()`.

On first open, `maRowIds` is empty.

Therefore:

- `nContentH == 0`,
- height falls back to the minimum,
- rows are inserted only afterwards,
- the popup opens too small,
- scrolling / clipping appears immediately,
- the result looks like an unfinished tiny menu.

This is not a DPI bug. It is an order-of-operations bug.

The current comment claiming geometry must be calculated before `RebuildRows()` conflicts with the implementation because geometry itself depends on the data populated by `RebuildRows()`.

### Mandatory conclusion

Separate:

1. **model construction**,
2. **geometry calculation**,
3. **widget population**.

Never make layout depend on a vector that has not been populated yet.

---

## 1.3 The Type System backend exists, but the current picker UI no longer represents it

The Type System is not merely a "font pair".

The current catalog already has semantic roles:

- Heading
- Body
- Mono
- Display

It also carries:

- typography scale,
- title / heading weight,
- body size,
- H1/H2/H3 sizes,
- title/subtitle sizes,
- quote/caption/mono sizes,
- line spacing,
- vertical rhythm / paragraph spacing,
- optional color roles,
- preferred fonts and fallback chains.

The Writer backend already applies those values to semantic Writer styles including:

- Default Paragraph Style,
- Heading base,
- Heading 1,
- Heading 2,
- Heading 3,
- Title,
- Subtitle,
- Block Quote,
- Caption / Label,
- Preformatted Text,
- Source Text / inline code.

That is the actual product feature.

The current popup, however, has been reduced to plain rows similar to:

```text
Modern Product — Inter + 72
Executive — Inter + 72
Editorial — Instrument Serif + Inter
```

That throws away most of the information that defines the Type System.

It also leaves old custom-renderer functions in the source, even though the current popup no longer connects them. This creates misleading dead code and invites a future developer to accidentally reactivate the broken renderer.

### Mandatory conclusion

Preserve the backend and rebuild the picker so it clearly exposes the semantic roles.

Do not reduce the Type System to a font dropdown with a longer label.

---

## 1.4 `SwModule::GetFirstView()` is incorrect in a multi-document application

Current toolbar controller:

```cpp
SwView* pView = SwModule::GetFirstView();
```

This is unsafe.

The toolbar controller belongs to a concrete LibreOffice frame. If two Writer documents are open, "first Writer view" is not guaranteed to be the view that owns the clicked button.

Possible failure:

1. Document A is opened first.
2. Document B is active.
3. User clicks Type System in B.
4. Controller resolves A via `GetFirstView()`.
5. Popup state and/or preset application comes from the wrong document.

This is a correctness bug even if it is not visible in the current screenshot.

### Mandatory conclusion

All Type System actions must resolve the `SwView` associated with the controller's `XFrame`.

No global "first view" lookup is allowed.

---

## 1.5 Typography Browser font rows have an unsafe custom renderer

The font browser currently uses:

```cpp
m_xRows->set_column_custom_renderer(0, true);
m_xRows->connect_custom_get_size(...);
m_xRows->connect_custom_render(...);
```

That is acceptable in principle because a font browser benefits from real font specimens.

The implementation is not safe enough.

### Problem A: hard-coded row width

`RowGetSizeHdl()` currently returns widths such as:

```cpp
return Size(200, nBaseH * 5);
```

for font rows.

This conflicts with a popup intended to be approximately 560-760 logical px wide.

The row-measure contract and the popup geometry are therefore describing different layouts.

### Problem B: specimen font size is inferred from row height, not measured

Current code approximately does:

```cpp
const size_t nSpecimenH =
    static_cast<size_t>(rRect.GetHeight() * 0.46);

vcl::Font aPreviewFont(aMetric);
aPreviewFont.SetFontSize(Size(0, nSpecimenH));
```

Then it draws `"Aa"`.

A requested font height is **not a guarantee** that the rendered glyph bounding box will fit inside the intended specimen rectangle.

Different typefaces have different:

- ascenders,
- descenders,
- internal leading,
- external leading,
- glyph bounds,
- font metrics.

At HiDPI, this becomes even more visible.

### Problem C: no hard row clip around the specimen

The text band is clipped, but the entire row/specimen is not robustly contained before drawing.

A too-large `Aa` can therefore paint into neighboring rows.

This matches the observed screenshot where the giant specimen overlaps section labels and adjacent font entries.

### Mandatory conclusion

Every custom-rendered row must obey all three:

1. real bounds measurement,
2. scale-down-to-fit,
3. hard clipping to its own row/specimen rectangle.

---

# 2. Architecture to implement

## 2.1 Type System must use native toolbar popup ownership

Use the LibreOffice popup-controller pattern:

```text
notebookbar toolbar item
        │
        ▼
svt::PopupWindowController
        │
        ▼
WeldToolbarPopup
        │
        ├── native popup anchoring
        ├── correct toolbar ownership
        ├── correct frame ownership
        ├── focus lifecycle
        └── close lifecycle
```

Reference pattern to study in the branch / upstream source:

- `svt::PopupWindowController`
- `WeldToolbarPopup`
- a working controller such as `TextUnderlinePopup`
- its `weldPopupWindow()` implementation
- its toolbar item setup in `initialize()`

Do not copy APIs from memory. Open the exact implementation in the checked-out branch and follow its constructor / registration conventions.

The source branch matters because LibreOffice internal UI APIs can change.

---

## 2.2 Keep the following existing Type System layers

Keep these concepts:

```text
Writer2027TypeSystemCatalog
    │
    ├── TypeSystemPreset
    ├── TypeSystemScale
    ├── font-role fallback chains
    └── ResolveTypeSystem()
            │
            ▼
sw::writer2027typesystem::ApplyTypeSystem()
            │
            ▼
semantic Writer styles
```

These are product-domain logic, not popup-layout logic.

Do not move style-application logic into a toolbar controller.

Do not make the popup directly mutate random style widgets.

---

## 2.3 Recommended Type System UI structure

The robust version should avoid multi-font custom painting in every row.

Recommended popup structure:

```text
┌────────────────────────────────────────────────────────────────┐
│ Type System                                                    │
│ A coordinated typography system for this document              │
├────────────────────────────────────────────────────────────────┤
│ ○ Modern Product    H Inter · B 72 · M 72 Mono · D Inter      │
│ ○ Executive         H Inter · B 72 · M JetBrains · D Inter    │
│ ○ Editorial         H Instr. Serif · B Inter · M IBM Plex ... │
│ ○ Tech              H IBM Plex Sans · B Inter · M JetBrains   │
│ ○ Research          H 72 · B 72 · M 72 Mono · D 72            │
│ ○ Accessible        H Atkinson · B Atkinson · M JetBrains     │
│ ○ Creative Agency   H Instr. Serif · B Inter · M JetBrains    │
├────────────────────────────────────────────────────────────────┤
│ Selected: Editorial                                            │
│                                                                │
│ Heading   Instrument Serif       H1 26 pt                      │
│ Body      Inter                  Body 12 pt · 130%             │
│ Display   Instrument Serif       Title 36 pt                   │
│ Code      IBM Plex Mono          10.5 pt                       │
│                                                                │
│ Fallbacks: 2 preferred fonts unavailable                       │
│ Using: Instrument Serif · Inter · IBM Plex Mono                │
└────────────────────────────────────────────────────────────────┘
```

Important: the detail pane can be text-first.

A Type System is a semantic configuration. It does not need to prove itself by custom-painting four different fonts into every row.

If a real specimen is desired, add **one isolated preview area for the currently highlighted preset**, not seven independently custom-painted list rows.

That single preview area can be measured and clipped reliably.

---

## 2.4 Type System list implementation rule

Preferred:

- native `weld::TreeView` / list behavior,
- one line per preset,
- native renderer,
- native selection,
- native keyboard navigation.

Do not use a custom renderer for the preset list unless there is a concrete requirement that cannot be met with native cells.

Remove stale functions if they are no longer used:

- `RowGetSizeHdl`
- `RowRenderHdl`
- `RowRender`
- `lcl_TypeSystemCardHeight`
- any associated custom-render state

Dead renderer code is not documentation. It is a trap.

---

# 3. Type System controller implementation plan

## 3.1 Replace the current custom `svt::ToolboxController` path

Current file:

```text
sw/source/uibase/uiview/writer2027toolboxctrl.cxx
```

Current implementation manually overrides `execute()` and calls into `SwView`.

Replace or substantially rewrite this controller so the Type System command is represented by a native `PopupWindowController`.

Target structure, names may be adjusted to match project conventions:

```cpp
class Writer2027TypeSystemPopupController final
    : public svt::PopupWindowController
{
public:
    explicit Writer2027TypeSystemPopupController(
        const css::uno::Reference<css::uno::XComponentContext>& xContext);

    void SAL_CALL initialize(
        const css::uno::Sequence<css::uno::Any>& rArguments) override;

    std::unique_ptr<WeldToolbarPopup> weldPopupWindow() override;

    OUString SAL_CALL getImplementationName() override;
    css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;
};
```

Do not blindly paste this skeleton. Match the exact signatures used by the branch.

---

## 3.2 Popup content class

Recommended:

```cpp
class Writer2027TypeSystemPopup final : public WeldToolbarPopup
{
public:
    Writer2027TypeSystemPopup(
        Writer2027TypeSystemPopupController* pController,
        weld::Widget* pParent,
        const css::uno::Reference<css::frame::XFrame>& xFrame);

    void GrabFocus() override;

private:
    // model
    // native list
    // detail fields
    // click / key handlers
};
```

Again, use the exact branch's working `WeldToolbarPopup` constructor pattern.

---

## 3.3 Frame ownership

The popup/controller must retain the frame supplied by the toolbar framework.

The selected preset must be applied to the Writer document belonging to that frame.

Forbidden:

```cpp
SwModule::GetFirstView()
SfxViewShell::Current()  // if used without proving it corresponds to m_xFrame
```

Required conceptually:

```text
controller.m_xFrame
    │
    ▼
resolve SfxViewFrame / SwView for THIS frame
    │
    ▼
read current document
    │
    ▼
apply preset to THIS document
```

Implement a small helper with one responsibility:

```cpp
SwView* ResolveWriterViewForFrame(
    const css::uno::Reference<css::frame::XFrame>& xFrame);
```

The helper must compare against frame/controller identity using an existing LibreOffice mapping API from the current source tree.

### Multi-document acceptance test

Two Writer windows must be open.

Applying "Editorial" in window B must:

- modify B,
- not modify A,
- not change A's current Type System detection,
- keep the popup anchored in B.

This test is mandatory.

---

## 3.4 Remove manual anchor rectangle plumbing

After migration, delete the need for:

```cpp
OpenWriter2027TypeSystemPopup(const tools::Rectangle& rAnchorRect)
```

or at minimum remove anchor geometry from the public SwView API.

No Writer document-view method should need to know where a toolbar button sits.

That is framework/UI-controller responsibility.

Also remove fallback code that turns an empty button rect into the entire document-window rectangle.

That fallback hides bugs and recreates the sidebar symptom.

---

## 3.5 What to do with `.uno:Writer2027TypeSystem`

The handoff states that this command is missing from the generated Sfx slot pool and normal `queryDispatch()` returns null.

Do not revive the previous crashing generic dispatch path.

The toolbar command can remain mapped to its dedicated popup controller through `Controller.xcu`.

Keep the generic null-dispatch guard because a generic controller should not dereference a null dispatch target.

However:

- the Type System feature should not depend on the generic dispatch path to open,
- controller registration must be the single supported toolbar-opening path,
- command registration must be tested.

If the developer wants to repair the svidl omission separately, do that in a separate change with its own tests. It is not required to fix the visible popup behavior.

---

# 4. Type System model/UI split

The current `RebuildRows()` mixes:

- catalog resolution,
- `maRowIds`,
- `maResolved`,
- tree mutation.

Split this.

## 4.1 Model build

Create a small popup model:

```cpp
struct TypeSystemPickerRow
{
    OUString maPresetId;
    OUString maDisplayName;

    OUString maHeadingFamily;
    OUString maBodyFamily;
    OUString maMonoFamily;
    OUString maDisplayFamily;

    int mnMissingCount = 0;
    bool mbCurrent = false;
};
```

Build it before calculating popup content.

Example method:

```cpp
void Writer2027TypeSystemPopup::BuildModel();
```

Rules:

- one row per preset,
- no tree operations,
- no rendering,
- no geometry,
- all resolved roles stored explicitly,
- current preset marked explicitly,
- custom/unmatched document state represented separately, not as a fake preset.

---

## 4.2 Geometry

Geometry is computed only after `BuildModel()`.

Inputs:

- model row count,
- real text metrics,
- popup framework work area if necessary,
- density token,
- detail-panel minimum size.

No geometry formula may depend on a stale widget state.

---

## 4.3 Widget population

Only then:

```cpp
PopulatePresetList();
UpdateDetailPanel();
```

Use `freeze()` / `thaw()` where appropriate.

Guard selection-change handlers during rebuilding.

Pattern:

```cpp
mbInternalMove = true;
m_xRows->freeze();

try
{
    // clear + repopulate
}
catch (...)
{
    m_xRows->thaw();
    mbInternalMove = false;
    throw;
}

m_xRows->thaw();
mbInternalMove = false;
```

Prefer RAII for this state so an exception cannot leave the tree permanently frozen or the internal-move flag stuck.

---

# 5. Type System interaction behavior

## 5.1 Open

On open:

1. resolve the document from the owning frame,
2. obtain `FontList`,
3. detect current Type System,
4. build model,
5. select current preset if matched,
6. otherwise show "Custom document typography" state,
7. update detail pane,
8. focus selected row or first preset.

Do not apply anything merely because the popup opened.

---

## 5.2 Hover / selection

Changing keyboard selection or list highlight should only update preview/detail content.

It must not mutate the document.

---

## 5.3 Apply

Single click or Enter on a preset:

1. validate preset ID,
2. resolve fonts,
3. apply Type System to the active document,
4. verify the call returned successfully,
5. close popup,
6. invalidate relevant Writer UI bindings if required so visible font/style controls refresh.

One explicit user action must correspond to one grouped undo action.

---

## 5.4 Escape

Escape closes the popup without applying.

---

## 5.5 Re-opening

Repeated:

```text
open
close
open
close
open
close
```

must not:

- hang,
- wedge weld,
- retain stale pointers,
- retain stale model data,
- duplicate event handlers,
- crash.

Run at least 100 cycles in an automated stress test.

---

# 6. Type System backend validation

Do not assume the existing backend is correct merely because it exists.

Add tests for the following.

## 6.1 Catalog

Assert expected preset IDs exist:

```text
modern-product
executive
editorial
tech
research
accessible
creative-agency
```

Assert every preset has usable roles.

At minimum:

- Body must resolve or have a fallback.
- Heading must resolve or have a fallback.
- Mono must resolve or have a fallback.
- Display must resolve or have a fallback.

---

## 6.2 Aliases

Test known alias behavior:

```text
SAP 72      -> installed family "72"
SAP 72 Mono -> installed family "72 Mono"
```

when the canonical marketing name is not exposed by the installed font system.

Alias resolution must **not** count as missing if it is the same typeface.

---

## 6.3 Fallback

Test:

```text
preferred installed -> preferred
preferred missing + fallback1 installed -> fallback1 + mbFallbackUsed
fallback1 missing + fallback2 installed -> fallback2 + mbFallbackUsed
all unavailable -> mbUnresolved
```

---

## 6.4 Apply semantics

For one known preset, assert exact style mutations.

Example categories:

| Semantic role | Writer target |
|---|---|
| Body | Default Paragraph Style |
| Heading family/weight | Heading base |
| H1 | Heading 1 |
| H2 | Heading 2 |
| H3 | Heading 3 |
| Display | Title |
| Subtitle | Subtitle |
| Quote | Block Quote |
| Caption | Caption/Label |
| Mono block | Preformatted Text |
| Mono inline | Source Text |

Verify:

- family,
- size,
- weight,
- line spacing,
- upper/lower spacing,
- color roles where applicable.

---

## 6.5 Dark-document color behavior

Existing behavior intends semantic colors only for the Writer 2027 digital-dark document.

Test both:

```text
dark Writer 2027 document
light/imported document
```

Light/imported documents must not unexpectedly receive dark-theme text colors.

---

## 6.6 Detect/apply round trip

Test:

```text
ApplyTypeSystem(Editoral)
DetectCurrentTypeSystem()
```

Expected:

```text
"editorial"
```

Then manually alter one required style attribute.

Expected:

```text
DetectCurrentTypeSystem() == empty/custom
```

---

## 6.7 Undo / redo

After applying one preset:

- one Undo should restore the previous complete Type System-related style state,
- one Redo should restore the selected preset.

Do not accept a state where only some semantic styles are reverted.

---

## 6.8 Idempotence

Applying the same preset twice must not create different style values.

Preferably, the second apply should not create a meaningful empty undo entry.

If the current grouped undo implementation creates empty undo groups, inspect and correct it if practical.

---

# 7. Typography Browser remediation

Files:

```text
svx/source/tbxctrls/writer2027fontpopup.cxx
svx/inc/svx/writer2027fontpopup.hxx
svx/uiconfig/ui/writer2027fontpopup.ui
```

The feature may keep a custom renderer because the font specimen is valuable.

But the renderer must be treated like graphics code: measured, clipped, bounded.

---

# 8. Font row sizing rules

## 8.1 Remove `Size(200, ...)`

Never return a hard-coded 200 px row width for a 560-760 px browser.

Add a member representing the actual content width:

```cpp
tools::Long mnPopupContentWidth = 0;
```

Compute it before the tree is laid out.

`RowGetSizeHdl()` must return that width or a safe width derived from the current render target.

Example concept:

```cpp
const tools::Long nWidth =
    std::max<tools::Long>(mnPopupContentWidth, nMinimumRowWidth);
```

The exact unit must match what the weld tree expects on the active platform.

Do not mix logical and device pixels.

---

## 8.2 One coordinate/unit convention per calculation

Document the units of every geometry value.

Example:

```cpp
// device px
tools::Long mnPopupContentWidthPx;

// logical px
tools::Long nPreferredWidthLogical;
```

Do not name everything `nWidth` and rely on comments elsewhere.

At HiDPI, unit ambiguity becomes a rendering bug factory.

---

# 9. Font specimen fit algorithm

The current approach assumes requested font size equals usable glyph height.

Replace it with real bounds measurement.

## 9.1 Define a specimen box

Within the row:

```text
row rect
┌──────────────────────────────────────────────────┐
│ specimen box │ name                              │
│              │ metadata                          │
└──────────────────────────────────────────────────┘
```

The specimen box must have explicit padding.

Example:

```cpp
tools::Rectangle aSpecimenRect(...);
aSpecimenRect.AdjustLeft(4);
aSpecimenRect.AdjustRight(-4);
aSpecimenRect.AdjustTop(4);
aSpecimenRect.AdjustBottom(-4);
```

Use project geometry helpers instead of these exact calls if APIs differ.

---

## 9.2 Measure actual text bounds

For `"Aa"`:

1. set candidate font,
2. call the current VCL text-bound measurement API,
3. inspect width and height,
4. shrink font if either exceeds specimen box,
5. repeat until it fits or minimum size reached.

Use a bounded iteration count.

Example algorithm:

```text
candidate = target size
repeat max 8 times:
    set font(candidate)
    bounds = real text bounds("Aa")

    if bounds fits specimen rectangle:
        break

    scale = min(
        availableWidth / boundsWidth,
        availableHeight / boundsHeight)

    candidate = floor(candidate * scale * 0.98)
```

The extra `0.98` safety factor prevents edge clipping due to rounding.

Never loop indefinitely.

---

## 9.3 Hard clip

Before drawing anything for a row:

```cpp
rCtx.Push(vcl::PushFlags::CLIPREGION);
rCtx.IntersectClipRegion(aRowRect);
```

For the specimen, optionally intersect again with `aSpecimenRect`.

Draw.

Then:

```cpp
rCtx.Pop();
```

Even if metrics are wrong, one row must never paint into another row.

This is a non-negotiable invariant.

---

# 10. Font name + metadata positioning

Do not calculate a line top using assumptions that can place the first line above the row.

Calculate the combined text block.

Conceptually:

```text
line 1 height = actual current UI font height
line 2 height = actual current UI font height
gap           = spacing token

blockHeight = line1 + gap + line2
startY = rowTop + max(0, (rowHeight - blockHeight) / 2)
```

Then:

```text
line1Y = startY
line2Y = startY + line1Height + gap
```

Clip the text band.

Long values must ellipsize or be clipped within the row; never overflow horizontally.

---

# 11. Section headers / category rows

Rows such as:

```text
Recommended
Collections
Modern
Editorial
Accessible
Technical / Mono
Legacy
```

must use native UI font metrics.

Do not allow the specimen renderer to run for them.

Do not reuse font-row height assumptions for headers.

---

# 12. Typography Browser popup geometry

Current target geometry is approximately:

```text
min       560 logical px
preferred 640 logical px
max       760 logical px
```

That is reasonable for a "Typography Browser", not a classic 200 px combo dropdown.

Rules:

- width clamped to work area,
- height content-driven,
- max height around 65-70% of work area,
- vertical scrollbar only when content exceeds cap,
- no horizontal scrollbar,
- search field remains visible while list scrolls.

Do not set mutually contradictory widths in:

- `.ui` file,
- `set_size_request`,
- custom row measure callback.

There must be one source of truth.

---

# 13. Type System popup `.ui` redesign

Current file:

```text
svx/uiconfig/ui/writer2027typesystempopup.ui
```

Current root includes a `GtkPopover` and a single `GtkTreeView`.

When moving to `WeldToolbarPopup`, follow the exact root/container structure used by an existing `WeldToolbarPopup` in the same branch.

Recommended logical content:

```text
root container
├── title
├── subtitle
├── scrolled preset list
└── detail container
    ├── selected preset name
    ├── heading role
    ├── body role
    ├── display role
    ├── mono role
    ├── scale/rhythm
    └── fallback status
```

Every important widget must have a stable buildable ID for UI automation.

Suggested IDs:

```text
typesystem_root
typesystem_title
typesystem_subtitle
preset_list
detail_name
role_heading
role_body
role_display
role_mono
role_scale
fallback_status
```

Stable IDs are part of the test API.

Do not rename them casually after tests are added.

---

# 14. Accessibility

The popup must be navigable without a mouse.

Required:

- accessible popup name: `Type System`,
- preset rows expose preset names,
- current preset state visible to assistive technology,
- detail fields have labels,
- Up/Down moves preset selection,
- Enter applies,
- Escape closes,
- focus starts inside the popup,
- no focus trap after close,
- Tab traversal is deterministic.

The semantic Type System detail should remain available as text even if a future visual specimen fails to render.

---

# 15. Exception handling and crash diagnostics

## 15.1 Principle

There are two classes of failures.

### Recoverable C++ / UNO exceptions

Examples:

- `css::uno::Exception`,
- `std::exception`,
- expected model/configuration errors.

These should be caught at meaningful UI/action boundaries, logged, and prevented from taking down Writer.

### Hard process faults

Examples:

- access violation,
- invalid memory access,
- stack corruption.

These are not normal exceptions under Windows `/EHsc`.

Do **not** pretend that `catch (...)` makes access violations recoverable.

The vectored crash handler should capture diagnostics, then allow the normal fatal path to continue.

Continuing after arbitrary memory corruption is more dangerous than crashing.

---

## 15.2 Boundaries that must catch normal exceptions

Add guarded boundaries around:

```text
popup creation
model build
font resolution
preset selection handler
ApplyTypeSystem call
font-popup model rebuild
custom row renderer callback
search callback that rebuilds the list
```

Recommended pattern:

```cpp
try
{
    ...
}
catch (const css::uno::Exception& rEx)
{
    Writer2027LogException("operation-name", rEx);
}
catch (const std::exception& rEx)
{
    Writer2027LogException("operation-name", rEx);
}
catch (...)
{
    Writer2027LogUnknownException("operation-name");
}
```

Do not wrap every three lines in try/catch.

Catch at component boundaries so logs explain what operation failed.

---

## 15.3 Never leave UI state half-mutated after an exception

Use RAII for:

- `freeze()/thaw()`,
- internal-selection guards,
- temporary render-context push/pop,
- temporary cursor changes,
- undo grouping if possible.

Example concept:

```cpp
class ScopedInternalMove
{
public:
    explicit ScopedInternalMove(bool& rFlag)
        : mrFlag(rFlag)
    {
        mrFlag = true;
    }

    ~ScopedInternalMove()
    {
        mrFlag = false;
    }

private:
    bool& mrFlag;
};
```

Use existing LibreOffice helpers if equivalent utilities already exist.

---

## 15.4 Structured log content

Keep:

```text
%TEMP%\writer2027.log
%TEMP%\writer2027_crash.log
```

but improve the log events.

Each Type System open should log:

```text
event=typesystem.open.begin
frame=<stable pointer/id>
view=<pointer>
dpiScale=...
presetCurrent=...
presetCount=7
```

After popup creation:

```text
event=typesystem.open.ready
popupWidth=...
popupHeight=...
selectedIndex=...
```

On apply:

```text
event=typesystem.apply.begin
preset=editorial
doc=<pointer>
```

Success:

```text
event=typesystem.apply.ok
preset=editorial
```

Failure:

```text
event=typesystem.apply.error
preset=editorial
exceptionType=...
message=...
```

Font popup render failures:

```text
event=fontpicker.row.render.error
rowId=...
family=...
```

### Do not log

- document text,
- clipboard text,
- passwords/tokens,
- full user content,
- unnecessary full document paths.

---

## 15.5 Crash log metadata

For hard faults, capture at least:

```text
timestamp
process id
thread id
exception code
fault address
module
stack/backtrace when available
current Writer 2027 operation
build/commit identifier if practical
```

If a crash occurs while visual tests run, CI must preserve this file as an artifact.

---

# 16. Testing strategy: the "Jest/Playwright equivalent" for LibreOffice

## Short answer

Do **not** introduce Jest or TypeScript to test this native C++/VCL/weld UI.

Jest operates naturally on JavaScript/DOM applications. Writer is native LibreOffice UI.

The closest native equivalents already exist in this repository:

### Semantic UI interaction

```text
LibreOffice UITest
Python
uitest.framework.UITestCase
```

This is the equivalent of:

```text
Jest + Testing Library / Playwright interaction assertions
```

for Writer widgets.

The branch already contains many tests under:

```text
sw/qa/uitest/
```

and `sw/Module_sw.mk` registers multiple `UITest_*` targets.

### Native screenshot capture

LibreOffice already has:

```cpp
test/screenshot_test.hxx
ScreenshotTest
```

and the Writer module already registers screenshot targets such as:

```make
CppunitTest_sw_dialogs_test
CppunitTest_sw_dialogs_test_2
```

The screenshot framework can render weld windows and save PNGs.

### What is missing

The generic screenshot mechanism primarily **captures** screenshots.

For this feature, add a deterministic **golden comparison** step so visual regressions fail automatically.

That gives us the equivalent of Playwright:

```ts
expect(page).toHaveScreenshot(...)
```

but for the real native Writer UI.

---

# 17. Required test layers

No single test type is enough.

Use all of these:

```text
Layer 1: C++ CppUnit domain tests
Layer 2: LibreOffice UITest interaction tests
Layer 3: geometry invariant tests
Layer 4: native screenshot/golden visual tests
Layer 5: crash / stress tests
```

---

# 18. Layer 1 — C++ Type System tests

Recommended location:

```text
sw/qa/uibase/uiview/
```

or another existing Type System-adjacent CppUnit target with the necessary dependencies.

If adding a new dedicated target is cleaner:

```text
CppunitTest_sw_writer2027_typesystem.mk
sw/qa/unit/writer2027-typesystem-test.cxx
```

Avoid creating a new target if the existing `sw_uibase_uiview` test target already provides everything needed.

## Required tests

```text
testCatalogContainsExpectedPresets
testResolvePreferredFamily
testResolveFallback1
testResolveFallback2
testResolveUnresolved
testSap72Alias
testApplyEditorialPreset
testApplyModernProductPreset
testDetectCurrentPresetAfterApply
testDetectCustomAfterManualMutation
testUndoRedoTypeSystem
testApplyAffectsOnlyExpectedDocument
testDarkDocumentColorRoles
testLightDocumentDoesNotReceiveDarkColorRoles
testReapplyIsIdempotent
```

---

# 19. Layer 2 — LibreOffice UI automation tests

Create:

```text
sw/qa/uitest/writer2027/
    typeSystemPopup.py
    typographyBrowser.py
```

Register them in a dedicated or existing UITest make target.

The repository already demonstrates the pattern:

```python
from uitest.framework import UITestCase
from uitest.uihelper.common import get_state_as_dict
```

and access to floating popup windows using:

```python
xFloatWindow = self.xUITest.getFloatWindow()
```

## 19.1 Type System open test

Pseudo-test:

```python
class Writer2027TypeSystemPopup(UITestCase):

    def test_popup_opens_from_toolbar(self):
        with self.ui_test.create_doc_in_start_center("writer"):
            # switch/ensure Writer 2027 mode
            # locate Type System toolbar control
            # click
            xPopup = self.xUITest.getFloatWindow()

            self.assertIsNotNone(xPopup)

            xList = xPopup.getChild("preset_list")
            self.assertEqual(7, len(xList.getChildren()))
```

Do not use sleep.

Use existing wait helpers:

```text
wait_until_child_is_available
wait_until_property_is_updated
```

---

## 19.2 Type System content test

Assert semantic text is present.

For selected Editorial preset:

```text
Heading contains Instrument Serif or resolved fallback
Body contains Inter or resolved fallback
Mono contains IBM Plex Mono / resolved fallback
Display contains Instrument Serif / resolved fallback
```

The exact expectation should derive from the deterministic test font environment.

---

## 19.3 Type System keyboard test

Automate:

```text
open
Down
Down
Enter
```

Assert:

- popup closes,
- selected Type System was applied,
- document style state changed as expected.

---

## 19.4 Escape test

Automate:

```text
open
Escape
```

Assert:

- popup closes,
- document style state unchanged.

---

## 19.5 Multi-document UI test

Open A and B.

Apply preset from B.

Assert:

```text
B == selected preset
A == previous state
```

This test exists specifically to prevent reintroduction of `GetFirstView()` behavior.

---

## 19.6 Repeat open/close test

At least:

```python
for _ in range(20):
    open_popup()
    close_popup()
```

A lower count is acceptable for normal `uicheck`.

Add a separate stress variant with 100-500 cycles for local / nightly CI.

Assert Writer remains alive and responsive.

---

# 20. Layer 3 — geometry invariant tests

Visual screenshots are essential, but geometry can be tested more precisely than pixels.

Expose or factor pure helpers where possible.

Examples:

```text
ComputeFontRowHeight(...)
ComputePopupWidth(...)
FitSpecimenFont(...)
```

Test them independently.

## Type System runtime geometry assertions

At UI level, where the accessibility state exposes position/size:

```text
popup.left >= toolbarButton.left - tolerance
popup.top >= toolbarButton.bottom - tolerance
popup.width >= requiredMinimum
popup.right <= workArea.right
popup.bottom <= workArea.bottom
```

Most importantly:

```text
popup is not at frame x=0 unless the invoking button itself is actually there
```

Do not hard-code exact screen coordinates across DPI modes.

Test relationships.

---

# 21. Layer 4 — visual golden tests

This layer resolves the recurring argument:

> "The code says it is fixed."

versus:

> "The screenshot is visibly broken."

The image is the contract.

## 21.1 Use LibreOffice's native screenshot facility

The current tree already has:

```cpp
#include <test/screenshot_test.hxx>
```

and:

```cpp
ScreenshotTest::saveScreenshot(weld::Window&)
```

which renders through the native VCL/weld stack and writes PNGs.

Add a dedicated Writer 2027 screenshot target rather than piggy-backing on unrelated dialog batches.

Suggested:

```text
CppunitTest_sw_writer2027_visual.mk
sw/qa/unit/writer2027-visual-test.cxx
```

Register under:

```make
gb_Module_add_screenshot_targets
```

in `sw/Module_sw.mk`.

---

## 21.2 Capture these visual states

Mandatory goldens:

```text
typesystem-default.png
typesystem-editorial-selected.png
typesystem-missing-fonts.png

fontpicker-root.png
fontpicker-recommended.png
fontpicker-modern.png
fontpicker-editorial.png
fontpicker-technical-mono.png
fontpicker-legacy.png
fontpicker-search.png
```

Also capture:

```text
fontpicker-long-family-name.png
fontpicker-long-metadata.png
```

---

## 21.3 Automatic comparison

Add a small comparator script.

Recommended implementation language: Python, because the LO test stack already uses Python and this does not justify a Node/TypeScript toolchain.

Pinned dependencies if external comparison is needed:

```text
Pillow
```

Optionally:

```text
scikit-image
```

only if SSIM is required and dependency weight is acceptable.

Prefer a small in-repo comparator using Pillow:

1. verify dimensions,
2. compute absolute per-channel difference,
3. ignore tiny antialiasing noise below a threshold,
4. calculate changed-pixel percentage,
5. output a highlighted diff image,
6. fail when threshold exceeded.

Failure artifacts:

```text
expected.png
actual.png
diff.png
metrics.json
writer2027.log
writer2027_crash.log
```

---

## 21.4 Golden policy

Goldens are not automatically rewritten.

Updating them requires explicit:

```text
UPDATE_WRITER2027_GOLDENS=1
```

or equivalent documented command.

A developer must inspect the newly generated images before committing them.

Never make CI silently accept a new screenshot after code changes.

---

## 21.5 Stable visual environment

Font rendering is sensitive to environment.

The visual CI runner must pin:

```text
OS image
Windows version/build
font set
theme
UI scale / DPI
LibreOffice locale
GPU/backend settings where relevant
```

Do not compare goldens generated on random developer machines.

Use a designated self-hosted Windows visual runner or a fixed VM image.

---

# 22. DPI visual matrix

The original failure is DPI-sensitive.

Run at least:

| Scenario | Resolution | Scale |
|---|---:|---:|
| Standard | 1920×1080 | 100% |
| HiDPI | 2560×1440 or equivalent | 150% |
| 4K | 3840×2160 | 200% |

If CI cost is too high:

- 100% on every PR,
- 200% on every PR for Writer 2027 UI changes,
- 150% nightly.

Never ship the custom font renderer based solely on a 100% screenshot.

---

# 23. Visual assertions specific to the font browser

The golden comparator detects overall change, but also add geometry assertions.

For each font row:

```text
specimenBounds ⊆ specimenRect
specimenRect ⊆ rowRect
textBounds ⊆ textBand
row[i].bottom <= row[i+1].top
```

If practical, add debug-only assertion code in the renderer for development builds:

```cpp
assert(aMeasuredBounds.GetHeight() <= aSpecimenRect.GetHeight());
```

Do not ship user-visible failures for harmless rounding; use tolerance of a few device pixels.

---

# 24. Layer 5 — crash/stress tests

## 24.1 Type System

Stress:

```text
100 open/close cycles
100 selection-move cycles
50 apply/undo/redo cycles
20 two-document alternating applies
```

Pass conditions:

- process alive,
- no `writer2027_crash.log` fatal entry created during run,
- no hang,
- no stale popup,
- no wrong-document mutation.

---

## 24.2 Font Browser

Stress:

```text
open/close 100x
type/clear search 100x
enter/leave categories 100x
scroll full list top-to-bottom repeatedly
```

Run with:

```text
100%
200%
```

DPI profiles.

---

# 25. Testing the real rendering, not only widget existence

A UITest that says:

```python
self.assertEqual(7, len(xList.getChildren()))
```

does **not** prove those seven rows are visible or non-overlapping.

A screenshot test alone does **not** prove a click applies the right Writer styles.

Therefore:

```text
UITest     = behavior
CppUnit    = domain correctness
geometry   = layout invariants
screenshot = visual proof
stress     = lifecycle/crash proof
```

All are required.

This is the native Writer equivalent of combining:

```text
Jest unit tests
Testing Library
Playwright
Playwright screenshot snapshots
```

in a web product.

---

# 26. Recommended visual-comparison script interface

Create:

```text
solenv/bin/writer2027_visual_compare.py
```

Example CLI:

```bash
python solenv/bin/writer2027_visual_compare.py \
  --expected v271/goldens/writer2027/200/typesystem-default.png \
  --actual workdir/screenshots/.../typesystem-default.png \
  --diff workdir/writer2027-diffs/typesystem-default.png \
  --max-changed-pixels 0.005 \
  --channel-threshold 8
```

Example `metrics.json`:

```json
{
  "expectedWidth": 640,
  "expectedHeight": 620,
  "actualWidth": 640,
  "actualHeight": 620,
  "changedPixelsRatio": 0.00031,
  "maxChannelDelta": 5,
  "pass": true
}
```

Do not set the tolerance so high that a giant overlapping `"Aa"` can pass.

---

# 27. Test-only deterministic font set

The Type System uses fonts that may not exist on every machine.

Visual CI must use a known font set.

At minimum ensure availability of the fonts expected in the baseline or intentionally exercise fallbacks.

Create two profiles:

## Full-font profile

Expected preferred fonts installed where licensing allows in the controlled environment.

## Fallback profile

Intentionally omit selected preferred fonts.

Verify UI displays:

```text
N fonts missing
Using: <resolved families>
```

and the document receives the resolved fallbacks.

Do not bundle proprietary fonts into the repository merely to satisfy tests.

---

# 28. Build and test commands

The exact command syntax depends on the existing LibreOffice build environment, but targets should be runnable individually.

Expected patterns:

```bash
make CppunitTest_sw_uibase_uiview
```

or the new dedicated target:

```bash
make CppunitTest_sw_writer2027_typesystem
```

UI tests:

```bash
make UITest_sw_writer2027
```

Screenshot target:

```bash
make CppunitTest_sw_writer2027_visual
```

Existing repository evidence shows Writer already registers:

```text
UITest_sw_sidebar
UITest_writer_tests*
CppunitTest_sw_dialogs_test
CppunitTest_sw_dialogs_test_2
```

Use the same gbuild conventions.

### Important

Do not claim a test passed unless the command was actually run and its exit code/log was captured.

The implementation report must list:

```text
command
exit code
duration
artifact/log location
```

---

# 29. File-by-file implementation plan

## `sw/source/uibase/uiview/writer2027toolboxctrl.cxx`

### Change

- replace `svt::ToolboxController` direct-open behavior,
- derive from native popup controller,
- remove `SwModule::GetFirstView()`,
- remove manual anchor rect handling,
- create `WeldToolbarPopup`,
- retain proper UNO component/service registration.

### Delete

Any fallback similar to:

```cpp
if (aAnchorRect.IsEmpty())
    aAnchorRect = wholeDocumentWindow;
```

---

## `officecfg/registry/data/org/openoffice/Office/UI/Controller.xcu`

### Change

Keep/update the Type System command mapping to the new popup-controller implementation.

Verify:

```text
Command = .uno:Writer2027TypeSystem
Module  = Writer
Controller = new implementation name
```

Do not accidentally register it globally for Calc/Impress.

---

## `sw/util/sw.component`

### Change

Register the new implementation name/exported constructor as required by the exact component conventions.

Remove obsolete registration if the old controller class is deleted.

---

## `svx/source/tbxctrls/writer2027typesystempopup.cxx`

### Change

If popup remains here:

- convert content implementation to `WeldToolbarPopup` compatible form,
- separate `BuildModel`, geometry, populate,
- remove manual `popup_at_rect`,
- remove stale per-row custom renderer,
- add detail panel logic,
- preserve catalog resolution.

If ownership becomes cleaner in `sw`, move only the Writer-specific popup controller/view integration. Do not duplicate the Type System catalog.

---

## `svx/inc/svx/writer2027typesystempopup.hxx`

### Change

- remove anchor-window/rectangle API,
- expose only popup lifecycle required by `WeldToolbarPopup`,
- add explicit model row structure if kept local,
- remove unused custom-render declarations.

---

## `svx/uiconfig/ui/writer2027typesystempopup.ui`

### Change

- migrate to the correct root expected by `WeldToolbarPopup`,
- add stable widget IDs,
- add semantic detail panel,
- remove fixed dimensions that contradict runtime geometry.

---

## `sw/source/uibase/uiview/view0.cxx`

### Change

Remove Type System popup anchoring responsibility.

Keep or move only:

- current document lookup,
- apply handler if architecture still requires it.

Prefer applying through a helper bound to the controller's frame.

Do not leave two separate supported paths for the same toolbar action.

---

## `svx/source/tbxctrls/writer2027fontpopup.cxx`

### Change

- fix row width,
- add real specimen measurement,
- scale-to-fit,
- hard row clip,
- deterministic vertical layout,
- preserve freeze/thaw protections,
- preserve search/category behavior,
- log render exceptions.

---

## `svx/uiconfig/ui/writer2027fontpopup.ui`

### Change

Ensure runtime width and `.ui` width do not conflict.

Keep:

- search field,
- scroll container,
- tree.

Remove arbitrary width request if the runtime geometry policy is authoritative, or make the values identical and documented.

---

# 30. Do not modify unrelated crash paths in this change

The handoff identified background exceptions from:

```text
pyuno.pyd / python313.dll
Windows crypto / NGC / BitLocker-related path
```

Those are not the Type System geometry bug.

Do not turn this remediation into a Python UNO / crypto investigation.

Preserve crash diagnostics, but keep the Type System PR/change focused.

If those faults still reproduce independently, file a separate defect with a separate reproduction.

---

# 31. Required logging for current rendering bugs

Before deleting old code, add one temporary diagnostic build if needed.

Type System open:

```text
controllerFrame
activeView
popupModelRows
currentPreset
dpiScale
```

Font row render:

```text
rowId
rowRect
specimenRect
requestedFontHeight
actualGlyphBounds
finalFontHeight
```

This makes it possible to prove whether a remaining 200% issue is:

- wrong row height,
- wrong font metrics,
- wrong clipping,
- wrong DPI conversion.

Remove extremely noisy per-pixel/per-frame logging once fixed.

Keep only useful operation-level diagnostics in production.

---

# 32. Acceptance criteria — Type System

The feature is complete only when all are true.

## Positioning

- opens under the actual Type System button,
- does not open at the Writer frame's left edge,
- remains correct after window move,
- remains correct on a second monitor,
- remains correct at 100/150/200% scaling.

## Content

- all expected presets visible,
- current preset indicated,
- Heading shown,
- Body shown,
- Mono shown,
- Display shown,
- scale/rhythm summary shown,
- missing/fallback information shown when relevant.

## Interaction

- keyboard navigation works,
- click works,
- Enter applies,
- Escape closes,
- no mutation on open/hover,
- repeated open/close does not hang.

## Document behavior

- preset changes semantic Writer styles,
- one undo restores previous state,
- redo restores preset,
- correct active document only,
- read-only document not mutated.

## Reliability

- no normal exception escapes UI boundary,
- no crash in stress test,
- crash diagnostics preserved for hard faults.

---

# 33. Acceptance criteria — Typography Browser

## Layout

- popup width is in intended browser range,
- search field fits,
- no horizontal overflow,
- rows do not overlap,
- specimen never paints outside row,
- section headers remain readable,
- metadata remains inside text band.

## HiDPI

At 200%:

- `"Aa"` is fully inside specimen box,
- `Recommended` not obscured,
- Atkinson Hyperlegible row not obscured,
- category labels not obscured,
- scrollbar behaves correctly.

## Interaction

- search works,
- category navigation works,
- Legacy navigation works,
- single click applies font,
- Enter applies,
- Escape closes/clears according to current contract.

---

# 34. Visual review matrix

Before declaring complete, save actual screenshots for:

```text
01_writer2027_home.png
02_typesystem_default_100.png
03_typesystem_default_200.png
04_typesystem_editorial_200.png
05_fontpicker_root_100.png
06_fontpicker_root_200.png
07_fontpicker_modern_200.png
08_fontpicker_legacy_200.png
09_two_documents_popup_on_B.png
```

These should be artifacts from the automated test environment, not manually cropped images.

---

# 35. Required implementation report from the developer

At completion, provide:

```markdown
# Writer 2027 Type System / Typography remediation report

## Commit
<sha>

## Files changed
...

## Type System architecture
...

## Removed legacy paths
...

## Font renderer fix
...

## Exception/logging changes
...

## Tests added
...

## Commands executed

| Command | Result | Duration |
|---|---|---|
| ... | PASS | ... |

## Visual artifacts
<paths>

## Remaining known issues
...
```

Do not write:

```text
"Should be fixed."
"Looks correct from code."
"Probably a DPI issue."
```

The report must contain evidence.

---

# 36. Stop conditions for the junior developer

Stop and ask for review if any of these occur:

1. Implementing `PopupWindowController` appears to require changes to generic framework code.
2. The exact `XFrame -> SwView` mapping cannot be found in current LibreOffice APIs.
3. A fix requires reintroducing document-window coordinate arithmetic.
4. A visual test requires bundling a non-redistributable font.
5. `WeldToolbarPopup` cannot host the required content without a significant framework patch.
6. A hard access violation still occurs after the old popup path is removed.
7. The same Type System apply modifies two Writer documents.
8. A test is flaky twice in the locked CI environment.

Do not improvise around these with hidden fallbacks.

---

# 37. Explicit anti-patterns

Do **not** do any of the following.

## Positioning

```cpp
popupX = frameX + toolbarX + magicOffset;
popupY = titlebarHeight + notebookbarHeight + ...
```

No.

## Active document

```cpp
SwModule::GetFirstView()
```

No.

## Renderer

```cpp
return Size(200, ...);
```

No.

## Specimen

```cpp
fontHeight = rowHeight * 0.46;
DrawText(...); // without measuring
```

No.

## Crash handling

```cpp
catch (...) {
    // ignore
}
```

No.

## UI verification

```text
I inspected the source and it should render.
```

No.

---

# 38. Suggested implementation sequence

Use this order.

## Phase A — lock tests around backend

1. Add catalog/fallback CppUnit tests.
2. Add apply/detect/undo tests.
3. Confirm backend semantics before UI rewrite.

## Phase B — Type System popup architecture

4. Implement native popup controller.
5. Bind to real frame.
6. Remove manual anchor plumbing.
7. Implement native preset list + detail pane.
8. Add UITest open/apply/escape/multi-document coverage.

## Phase C — Type System visual contract

9. Add screenshot target.
10. Capture 100% and 200% goldens.
11. Add automatic comparison.

## Phase D — font browser renderer

12. Remove hard-coded 200 width.
13. Implement glyph bounds fit.
14. Add hard clipping.
15. Add long-name/long-meta tests.
16. Add 100/200% goldens.

## Phase E — robustness

17. Run stress loops.
18. Verify crash logs remain empty.
19. Verify two-document correctness.
20. Produce completion report + artifacts.

This order isolates regressions. Do not change Type System architecture and font renderer in one giant unverified edit.

---

# 39. Primary expected root causes after this remediation

When implementation is complete, the following defects should be structurally impossible:

| Current defect | Structural fix |
|---|---|
| Popup at top-left | native toolbar popup owns anchor |
| ToolBox rect interpreted in frame coordinates | no manual rect transfer |
| Tiny first-open popup | model built before geometry |
| Wrong Writer document | controller uses owning `XFrame` |
| Type System reduced to two fonts | semantic role detail pane |
| Giant `Aa` overlaps rows | real bounds fit + clipping |
| 200 px row inside 640 px browser | row width follows content width |
| "fixed in code" but broken visually | golden screenshot CI |

---

# 40. Evidence from the current branch

The diagnosis in this spec is based on the current `lukas_dev` implementation around baseline commit:

```text
f7817ebf4baff492fde15550ed5bb9047070e51e
```

Relevant current files:

```text
sw/source/uibase/uiview/writer2027toolboxctrl.cxx
sw/source/uibase/uiview/view0.cxx
svx/source/tbxctrls/writer2027typesystempopup.cxx
svx/source/tbxctrls/writer2027typesystem.cxx
sw/source/core/doc/writer2027typesystem.cxx
svx/source/tbxctrls/writer2027fontpopup.cxx
svx/uiconfig/ui/writer2027typesystempopup.ui
svx/uiconfig/ui/writer2027fontpopup.ui
officecfg/registry/data/org/openoffice/Office/UI/Controller.xcu
sw/util/sw.component
sw/Module_sw.mk
test/source/screenshot_test.cxx
sw/qa/uitest/
```

Important existing infrastructure confirmed in-tree:

```text
uitest.framework.UITestCase
self.xUITest.getFloatWindow()
ScreenshotTest
ScreenshotTest::saveScreenshot(weld::Window&)
gb_Module_add_screenshot_targets
gb_Module_add_uicheck_targets
```

Use those systems rather than adding a parallel web-testing stack.

---

# 41. Definition of done

This work is **DONE** only if a reviewer can pull the branch and obtain the following without subjective interpretation:

```text
1. successful build
2. successful Type System CppUnit tests
3. successful Writer 2027 UITest tests
4. successful screenshot/golden comparison
5. successful 200% DPI visual test
6. successful multi-document test
7. successful stress test
8. no writer2027 crash log from the test run
9. PNG artifacts showing correct popup placement and font rows
10. exact commit SHA and reproducible commands
```

The UI screenshot is part of the product contract.

If the screenshot is visibly broken, the feature is broken even if every source file compiles.

If the screenshot looks correct but applying a preset changes the wrong document, the feature is broken.

If both are correct but repeated opening crashes, the feature is broken.

All three dimensions must be green:

```text
VISUAL
FUNCTIONAL
RELIABILITY
```

Only then merge.

---

# 42. Final engineering intent

Writer 2027 should treat typography as a product system, not as a collection of patched legacy dropdowns.

The Type System picker is the semantic layer:

```text
Heading + Body + Display + Mono + Scale + Rhythm
```

The Typography Browser is the individual-font layer:

```text
browse + inspect + choose one family
```

They must remain separate.

The Type System should use native, framework-owned popup lifecycle and native list layout.

The font browser may use custom painting where it adds real value, but custom painting must obey the same invariants as any graphics engine:

```text
measure
fit
clip
render
verify
```

The test suite then acts as the referee, not developer confidence.

