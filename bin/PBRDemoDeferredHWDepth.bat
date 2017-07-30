@echo off
setlocal
if %0 == "%~0" (set "dirname=%cd%\") else (set "dirname=%~dp0")
if exist "%dirname%Urho3DPlayer.exe" (set "DEBUG=") else (set "DEBUG=_d")
"%dirname%Urho3DPlayer%DEBUG%" Scripts/42_PBRMaterials.as -renderpath CoreData/RenderPaths/PBRDeferredHWDepth.xml %*
