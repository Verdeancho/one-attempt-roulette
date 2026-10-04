@echo off
REM Compila el mod y lo instala automaticamente en Geometry Dash.
REM Uso: build.bat          (compilar)
REM      build.bat clean    (borrar la carpeta build y recompilar desde cero)

setlocal
cd /d "%~dp0"

if "%GEODE_SDK%"=="" set "GEODE_SDK=%USERPROFILE%\Documents\GeodeSDK"

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || (echo No se encontro Visual Studio & exit /b 1)

if /i "%1"=="clean" rmdir /s /q build

REM Se usa clang-cl (LLVM): MSVC da un error interno con la libreria async de Geode v5
set "CLANG=C:\Program Files\LLVM\bin\clang-cl.exe"
if not exist build\build.ninja (
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_C_COMPILER=%CLANG%" "-DCMAKE_CXX_COMPILER=%CLANG%" || exit /b 1
)
cmake --build build || exit /b 1

echo.
echo Mod compilado e instalado. Abre (o reinicia) Geometry Dash para probarlo.
