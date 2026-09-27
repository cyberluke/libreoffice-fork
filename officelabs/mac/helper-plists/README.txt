OfficeLabs CEF Sidebar - macOS Helper .app Info.plist templates
===============================================================

Mirrors the proven cefsimple Helper bundles (CEF 144) for the OfficeLabs
CEF sidebar. Each plist is an Info.plist template intended to be placed at
<Helper.app>/Contents/Info.plist during the Helper-bundle packaging step.

All plists keep the exact key set cefsimple uses:
  CFBundleDevelopmentRegion, CFBundleDisplayName, CFBundleExecutable,
  CFBundleIdentifier, CFBundleInfoDictionaryVersion, CFBundleName,
  CFBundlePackageType (APPL), CFBundleSignature, CFBundleVersion,
  CFBundleShortVersionString, LSEnvironment (MallocNanoZone=0),
  LSFileQuarantineEnabled, LSMinimumSystemVersion (12.0),
  LSUIElement (1, background helper - no dock icon),
  NSSupportsAutomaticGraphicsSwitching.

Only the four identity keys differ per helper type:
  CFBundleName / CFBundleDisplayName / CFBundleExecutable (all equal),
  CFBundleIdentifier (ai.officelabs.helper + suffix).

Filename -> helper type -> required .app bundle name mapping
------------------------------------------------------------

helper.plist            -> (main)     -> OfficeLabs Helper.app
helper-gpu.plist        -> (GPU)      -> OfficeLabs Helper (GPU).app
helper-renderer.plist   -> (Renderer) -> OfficeLabs Helper (Renderer).app
helper-plugin.plist     -> (Plugin)   -> OfficeLabs Helper (Plugin).app
helper-alerts.plist     -> (Alerts)   -> OfficeLabs Helper (Alerts).app

Bundle identifiers
------------------

OfficeLabs Helper.app              -> ai.officelabs.helper
OfficeLabs Helper (GPU).app        -> ai.officelabs.helper.gpu
OfficeLabs Helper (Renderer).app   -> ai.officelabs.helper.renderer
OfficeLabs Helper (Plugin).app     -> ai.officelabs.helper.plugin
OfficeLabs Helper (Alerts).app     -> ai.officelabs.helper.alerts

Executable names (CFBundleExecutable, must match the Mach-O inside Contents/MacOS)
----------------------------------------------------------------------------------

OfficeLabs Helper
OfficeLabs Helper (GPU)
OfficeLabs Helper (Renderer)
OfficeLabs Helper (Plugin)
OfficeLabs Helper (Alerts)

Note: CFBundleVersion and CFBundleShortVersionString are left empty, matching
cefsimple; the build/packaging step should populate them from the OfficeLabs
version constants before signing.