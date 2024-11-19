Unicode True
; Define your application name
!define APPNAME "Flip Clock OBS Plugin"
!define APPVERSION "1.0.0.1"
!define APPNAMEANDVERSION "Flip Clock OBS Plugin ${APPVERSION}"

; Main Install settings
Name "${APPNAMEANDVERSION}"
InstallDirRegKey HKLM "Software\${APPNAME}" ""
InstallDir "$PROGRAMFILES\obs-studio"
OutFile "FlipClock-Plugin-Install-v${APPVERSION}.exe"

Var INSTALL_BASE_DIR
Var OBS_INSTALL_DIR

; Use compression
SetCompressor /FINAL /SOLID lzma
SetDatablockOptimize on

; Modern interface settings
!include "MUI.nsh"

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Set languages (first is default language)
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_RESERVEFILE_LANGDLL

Section "Flip Clock OBS Plugin" Section1
	StrCpy $INSTALL_BASE_DIR "$PROGRAMFILES\obs-studio"

	ReadRegStr $OBS_INSTALL_DIR HKLM "SOFTWARE\OBS Studio" ""

	!if "$OBS_INSTALL_DIR" != ""
		StrCpy $INSTALL_BASE_DIR "$OBS_INSTALL_DIR"
	!endif


	StrCpy $InstDir "$INSTALL_BASE_DIR"

	IfFileExists "$INSTDIR\*.*" +3
		MessageBox MB_OK|MB_ICONSTOP "OBS Directory doesn't exist!"
		Abort

	; Set Section properties
	SetOverwrite on
	AllowSkipFiles off

	SetOutPath "$INSTDIR\bin\64bit\"
	File ".\flipclock.ttf"
	
	SetOutPath "$INSTDIR\obs-plugins\64bit\"
	File ".\x64\Release\flipclock.dll"
	File ".\x64\Release\flipclock.pdb"
	
	SetOutPath "$INSTDIR\data\obs-plugins\flipclock\locale\"
	File ".\data\locale\en-US.ini"
	File ".\data\locale\zh-CN.ini"
	File ".\data\locale\zh-TW.ini"
	
	CreateDirectory "$SMPROGRAMS\Flip Clock OBS Plugin"
	CreateShortCut "$SMPROGRAMS\Flip Clock OBS Plugin\Uninstall Flip Clock OBS Plugin.lnk" "$INSTDIR\obs-plugins\uninstall-flipclock-plugin.exe"

SectionEnd

Section -FinishSection

	WriteRegStr HKLM "Software\${APPNAME}" "InstallDir" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$INSTDIR\uninstall-flipclock-plugin.exe"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "Biry"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "HelpLink" "https://github.com/PeterCang/obs-flip-clock"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${APPVERSION}"
	WriteUninstaller "$INSTDIR\uninstall-flipclock-plugin.exe"

SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Section1} "Install the Flip Clock OBS Plugin to your installed OBS Studio version"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

UninstallText "This will uninstall Flip Clock OBS Studio plugin from your system"

;Uninstall section
Section Uninstall
	SectionIn RO
	AllowSkipFiles off
	;Remove from registry...
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
	DeleteRegKey HKLM "SOFTWARE\${APPNAME}"

	; Delete self
	Delete "$INSTDIR\uninstall-flipclock-plugin.exe"

	; Delete Shortcuts
	Delete "$SMPROGRAMS\Flip Clock OBS Plugin\Uninstall Flip Clock OBS Plugin.lnk"

	Delete "$INSTDIR\bin\64bit\flipclock.ttf"
	Delete "$INSTDIR\obs-plugins\64bit\flipclock.dll"
	Delete "$INSTDIR\obs-plugins\64bit\flipclock.pdb"
	Delete "$INSTDIR\data\obs-plugins\flipclock\locale\en-US.ini"
	Delete "$INSTDIR\data\obs-plugins\flipclock\locale\zh-CN.ini"
	Delete "$INSTDIR\data\obs-plugins\flipclock\locale\zh-TW.ini"

	; Remove remaining directories
	RMDir "$SMPROGRAMS\Flip Clock OBS Plugin"
	RMDir "$INSTDIR\data\obs-plugins\flipclock\locale\"
	RMDir "$INSTDIR\\data\obs-plugins\flipclock\"

SectionEnd
; eof