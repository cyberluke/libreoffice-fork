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

helper.plist            -> (main)     -> NAI Office Helper.app
helper-gpu.plist        -> (GPU)      -> NAI Office Helper (GPU).app
helper-renderer.plist   -> (Renderer) -> NAI Office Helper (Renderer).app
helper-plugin.plist     -> (Plugin)   -> NAI Office Helper (Plugin).app
helper-alerts.plist     -> (Alerts)   -> NAI Office Helper (Alerts).app

Bundle identifiers
------------------

NAI Office Helper.app              -> ai.officelabs.helper
NAI Office Helper (GPU).app        -> ai.officelabs.helper.gpu
NAI Office Helper (Renderer).app   -> ai.officelabs.helper.renderer
NAI Office Helper (Plugin).app     -> ai.officelabs.helper.plugin
NAI Office Helper (Alerts).app     -> ai.officelabs.helper.alerts

Executable names (CFBundleExecutable, must match the Mach-O inside Contents/MacOS)
----------------------------------------------------------------------------------

NAI Office Helper
NAI Office Helper (GPU)
NAI Office Helper (Renderer)
NAI Office Helper (Plugin)
NAI Office Helper (Alerts)

Note: CFBundleVersion and CFBundleShortVersionString are left empty, matching
cefsimple; the build/packaging step should populate them from the OfficeLabs
version constants before signing.