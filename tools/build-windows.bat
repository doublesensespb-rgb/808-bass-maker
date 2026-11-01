@echo off
rem 808 BASS MAKER - local Windows build (no GitHub needed).
rem Needs once: Visual Studio 2022 (Community or Build Tools, "Desktop development with C++") and CMake.
rem Double-click this file. The result appears in the "out" folder next to CMakeLists.txt.

setlocal
cd /d "%~dp0.."

where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake not found. Install it from https://cmake.org/download/ and tick "Add CMake to PATH".
  pause & exit /b 1
)

if not exist "JUCE\CMakeLists.txt" (
  echo JUCE folder not found, downloading JUCE 8.0.10...
  git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git JUCE
  if errorlevel 1 (
    echo Could not download JUCE. Put the JUCE 8.0.10 folder next to CMakeLists.txt and run again.
    pause & exit /b 1
  )
)

cmake -B build -A x64 || goto :fail
cmake --build build --config Release --parallel --target BassMaker_VST3 BassMaker_Standalone || goto :fail

if exist out rmdir /s /q out
mkdir out
xcopy /e /i /q "build\BassMaker_artefacts\Release\VST3\808 BASS MAKER.vst3" "out\808 BASS MAKER.vst3" >nul
copy /y "build\BassMaker_artefacts\Release\Standalone\808 BASS MAKER.exe" out\ >nul

echo.
echo Done. Plugin: out\808 BASS MAKER.vst3
echo Copy it to C:\Program Files\Common Files\VST3 and rescan plugins in FL Studio.
pause
exit /b 0

:fail
echo.
echo Build failed. Scroll up to see the error.
pause
exit /b 1
