# Refactoring notes

## `libs/csvparser/TabularData.h`

- **Added:** New shared `TabularData` container for table data with explicit header and row separation.
- **Fix:** Replaces ad-hoc row-map structures with one common representation across parsing and export.

## `libs/csvparser/csvParser.*`

- **Changed:** Parser output now uses the shared `TabularData` format instead of a mixed map-based table representation.
- **Added:** Copy-returning getters expose the parsed table without exposing mutable internals.
- **Fix:** Keeps parsing logic consistent with the rest of the conversion pipeline and prevents accidental state mutation.

## `csv2xltabular.h`

- **Added:** `TabularData` is used for both parsed and converted table data.
- **Changed:** LaTeX export now receives explicit header + row data instead of relying on the first map row as a header.
- **Removed:** Legacy `draftTable()` flow and older layout assumptions tied to shared row maps.
- **Added:** `transformTable()`, `formatTable()`, and `buildLaTeXData()` to split conversion and rendering responsibilities.
- **Fix:** Makes the conversion pipeline clearer and avoids conflating user settings with generated layout output.

## `csv2xltabular.cpp`

- **Removed:** `BuildExportTable()` and the old map-based export path; replacement logic now works on rows and headers directly.
- **Changed:** `FilterEmptyRows()` filters data rows rather than an input map containing a header row.
- **Added:** `ResolveLayout()` applies only explicit layout overrides on top of the INI-defined base layout.
- **Added:** `buildLaTeXData()` selects the configured header when enabled, otherwise preserves the first converted row as the fallback header.
- **Changed:** `exportToFileImpl()` now resolves effective layout settings from the conversion state and rejects export before a successful conversion.
- **Changed:** `draftLaTeX()` receives `TabularData`; it prints the configured header and passes only data rows to `tableRender()`.
- **Changed:** `tableRender()` iterates all provided rows and no longer relies on the first row being a header.
- **Fix:** LaTeX output now uses transformed cell values and the intended INI header while preserving all rows. CSV export behavior remains separate and unaffected.
- **Fix:** Default and custom LaTeX exports keep the base INI layout unless a specific override is supplied.

## `LaTeXExporter.*`

- **Added:** Standalone `LaTeXExporter` implementation and declaration.
- **Changed:** LaTeX rendering is separated from the main conversion logic into its own exporter.
- **Fix:** Keeps rendering concerns isolated and reduces coupling between conversion and presentation.

## `main.cpp`

- **Changed:** Updated the custom export example to the new per-field layout override API.
- **Fix:** Keeps sample usage aligned with the refactored configuration interface.

## Supporting library updates

- **Changed:** CMake configuration and utility/sort helpers were adjusted to support the shared `TabularData` format and the standalone exporter.
- **Fix:** Keeps the build, sort, and utility code compatible with the refactored table representation.
