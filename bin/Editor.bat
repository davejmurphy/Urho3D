@echo off
setlocal
if %0 == "%~0" (set "dirname=%cd%\") else (set "dirname=%~dp0")
if exist "%dirname%Urho3DPlayer.exe" (set "DEBUG=") else (set "DEBUG=_d")
if [%1] == [] (set "OPT1=-w -s") else (set "OPT1=")
start "" "%dirname%Urho3DPlayer%DEBUG%" Scripts/Editor.as %OPT1% %*
