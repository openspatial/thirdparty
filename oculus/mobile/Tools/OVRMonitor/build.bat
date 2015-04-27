echo off

echo Loading VS 2013 Environment Variables...
CALL "%VS120COMNTOOLS%\VsDevCmd.bat"

echo Building...

msbuild build\windows\ovrmonitor.vcxproj /nologo /verbosity:m /p:Configuration=Debug;Platform=x64
msbuild build\windows\ovrmonitor.vcxproj /nologo /verbosity:m /p:Configuration=Release;Platform=x64

pause
