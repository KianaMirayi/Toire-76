; ============================================================================
;  TOIRE-76 - Windows VST3 installer (Inno Setup 6)
;
;  Build with:
;    powershell -ExecutionPolicy Bypass -File installer\build-installer.ps1
;
;  Structure mirrors the TOIRE-2A installer (same five lessons):
;  1. VST3 is a BUNDLE (folder) - the whole Contents/ must be installed,
;     otherwise WebView2Loader.dll / webui go missing and the UI stays blank.
;  2. ArchitecturesInstallIn64BitMode makes {commoncf} resolve to the 64-bit
;     Common Files; without it the plugin lands in the x86 folder.
;  3. WebView2 is a hard dependency (the UI is a web page). The bootstrapper is
;     bundled and only runs when the runtime is actually missing.
;  4. {app} is the bundle directory itself, so [Files] install into {app}
;     and the uninstaller lives inside the bundle.
;  5. TOIRE-76's UI lives in <bundle>\Contents\webui\index.html; the editor
;     searches webui/index.html and Contents/webui/index.html, so the payload
;     must be the complete bundle with webui already under Contents.
; ============================================================================

#define AppName      "TOiRE-76"
#define AppVersion   "0.1.0"
#define AppPublisher "Kiana Mirayi"
#define BundleName   "TOiRE-76.vst3"

[Setup]
AppId={{8F3C1A62-7E4B-4C2D-9A71-2B5E8D4F6C10}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/KianaMirayi/Toire-76
AppSupportURL=https://github.com/KianaMirayi/Toire-76
AppUpdatesURL=https://github.com/KianaMirayi/Toire-76/releases
VersionInfoVersion={#AppVersion}
VersionInfoDescription={#AppName} VST3 installer
VersionInfoCompany={#AppPublisher}

DefaultDirName={commoncf}\VST3\{#BundleName}
DisableDirPage=yes
DisableProgramGroupPage=yes

PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763

Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

CloseApplications=yes
RestartApplications=no

OutputDir=..\dist
OutputBaseFilename=TOIRE-76-{#AppVersion}-win64
UninstallDisplayName={#AppName} {#AppVersion} (VST3)
UninstallDisplayIcon={app}\Contents\x86_64-win\{#BundleName}
AppComments=VST3 64-bit audio plugin for Windows. UI rendered with Microsoft Edge WebView2.

InfoBeforeFile=BEFORE-INSTALL.txt

[Languages]
; Inno only accepts .isl files that carry a UTF-8 BOM
#define DepsDir AddBackslash(SourcePath) + "deps"
#if FileExists(DepsDir + "\ChineseSimplified.isl")
Name: "chinesesimplified"; MessagesFile: "{#DepsDir}\ChineseSimplified.isl"
#else
Name: "english"; MessagesFile: "compiler:Default.isl"
#endif

[Files]
Source: "payload\{#BundleName}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "deps\MicrosoftEdgeWebview2Setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall dontcopy

[Run]
Filename: "{tmp}\MicrosoftEdgeWebview2Setup.exe"; Parameters: "/silent /install"; StatusMsg: "Installing Microsoft WebView2 runtime (required by the plugin UI)..."; Flags: waituntilterminated; Check: WebView2Missing

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function KeyHasWebView2(const RootKey: Integer; const SubKey: String): Boolean;
var
  Version: String;
begin
  Result := RegQueryStringValue(RootKey, SubKey, 'pv', Version) and (Version <> '');
end;

function WebView2Missing: Boolean;
begin
  Result := not (KeyHasWebView2(HKLM64, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}')
              or KeyHasWebView2(HKLM64, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}')
              or KeyHasWebView2(HKLM32, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}')
              or KeyHasWebView2(HKLM32, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}')
              or KeyHasWebView2(HKCU,   'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}')
              or KeyHasWebView2(HKCU,   'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'));
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  if WebView2Missing then
    ExtractTemporaryFile('MicrosoftEdgeWebview2Setup.exe');

  Result := '';
end;
