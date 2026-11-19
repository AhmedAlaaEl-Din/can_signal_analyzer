# Qt: license notice

CAN Analyzer uses the **Qt framework** (version @QT_VERSION@), © The Qt Company
Ltd. and other contributors, under the **GNU Lesser General Public License
version 3 (LGPLv3)**. The license texts are in this folder:

- `LGPL-3.0.txt`: GNU Lesser General Public License, version 3
- `GPL-3.0.txt`: GNU General Public License, version 3 (the LGPLv3 is a set
  of additional permissions on top of it)

## Qt modules in this package

Qt Core, Qt Gui, Qt Qml, Qt Quick, Qt Quick Controls 2 (incl. Templates),
Qt Quick Dialogs, Qt Quick Layouts, Qt Quick Shapes, Qt Quick Effects, Qt Svg,
and the platform, image-format, icon-engine and style plugins that
`windeployqt` selected for them. The Qt libraries were **not modified**.

Qt itself contains third-party components (for example zlib, libpng,
libjpeg, FreeType, HarfBuzz, PCRE2, double-conversion). Their licenses and
attributions are listed in the Qt documentation under "Licenses Used in Qt"
for this Qt version.

## Source code of Qt

The complete source code of this Qt version is available from the Qt Project:
<https://download.qt.io/official_releases/qt/> (folder for version
@QT_VERSION@), and from <https://code.qt.io>. The libraries in this package
were built by The Qt Company from that unmodified source.

## Replacing or relinking Qt (your rights under the LGPLv3)

CAN Analyzer links to Qt **dynamically**: every Qt library is a separate DLL
in this folder tree (`Qt6Core.dll`, `Qt6Quick.dll`, ..., the `plugins` folders
and the `qml` folder). You may replace them with your own build of Qt:

1. Build or install Qt @QT_VERSION_MAJOR_MINOR@.x for Windows x64 with the
   **MSVC 2022 (or newer, ABI-compatible)** compiler, release configuration.
   A modified Qt must keep binary compatibility with the version listed above
   (same major/minor version; Qt keeps binary compatibility across patch
   releases of a minor version).
2. Copy your `Qt6*.dll` files over the ones next to `can_analyzer.exe`, and
   your plugin and QML module folders over `platforms\`, `imageformats\`,
   `iconengines\`, `styles\`, `qml\`, etc. (or run your Qt's `windeployqt`
   on `can_analyzer.exe`).
3. Start `can_analyzer.exe`. The About dialog shows the Qt version that was
   actually loaded.

The application's own source code is available under the MIT license (see
`LICENSE.txt`), so you can also rebuild it entirely against another Qt
version with CMake (see `README.md`).
