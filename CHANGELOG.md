# Changelog

## [0.4.1] - 2026-10-25
### Added
- Tab close buttons (X) with custom icon
- Full keyboard shortcuts dialog translations (PT/ES/EN)
- App icons installed to hicolor theme (16-256px + SVG)

### Fixed
- Linux packaging: icon installation to `/usr/share/icons/hicolor/`

## [0.4.0] - 2026-10-24
### Added
- Fluent Design interface: design tokens (`resources/themes/fluent.json`) plus stylesheet (`fluent.qss`)
- Custom Fluent title bar with Mica integration on Windows
- Language selector in Preferences → Appearance (System default / English / Português / Español)
- "System default" entry in View → Language so the app can follow the OS locale again

### Fixed
- Menu bar items were clipped on Linux when the stylesheet forced a fixed menu bar height
- Dark theme overrides never applied (the `theme` property was set on QApplication, not on a widget)
- Combo box popups opened showing a single item plus scroll arrows
- Low-contrast selected item text in dark theme combo popups

## [0.3.0] - 2026-10-24
### Added
- Advanced print preview with configurable headers/footers
- Java/JDK integration: detection, compile and run helpers
- Real virtual-space cursor with join/split lines
- Full TextPad feature parity pass (sort lines shortcut, error links, tab drag-and-drop, split view sync)
- Real diff hunks with change navigation and syntax highlighting
- Large-file mode and "mark clean" support
- Brazilian Portuguese and Spanish README translations

### Fixed
- Diff hunk boundaries, print pagination and virtual-cursor behavior
- Custom macro shortcuts

## [0.2.0] - 2026-10-23
### Added
- LSP core integration with completion and hover widgets
- Git integration: diff viewer and inline blame
- Keyboard Shortcuts dialog (`Ctrl+Alt+K`)

### Fixed
- Block selection context menu now preserves the selection
- Editor gutter rendering and blame gutter width
- Classic open/save icons on all platforms
- `replaceAll` respects normal and block selections

## [0.1.0] - 2026-10-23
### Added
- Initial public release
- Syntax highlighting for C++, Python, JS, HTML, CSS, Java, JSON, Markdown, Text
- Multi-line strings/comments support
- Auto-indent and auto-close brackets
- Macro recording/replay with 9 slots
- Clip library with categories/tags
- Spell checking (Hunspell: pt_BR, en_US, es_ES)
- Dark/Light themes
- i18n: Portuguese, English, Spanish
- File explorer, workspaces, external tools
- Find in files, block selection, folding
- Printing, session restore
