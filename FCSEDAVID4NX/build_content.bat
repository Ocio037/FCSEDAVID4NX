@echo off
setlocal
echo ===============================================================
echo    FCSEDAVID4NX - Preparar Contenido
echo ===============================================================
echo Este script prepara recursos y shaders; no compila proyectos.

set "VULKAN_GLSLC=C:\VulkanSDK\1.4.350.0\Bin\glslc.exe"
set "ARC_PACKER=C:\Project_MX\Uadeo\videojuegos\Herramientas\ArcPacker\bin\Release\net10.0\ArcPacker.exe"
set "EVENT_EDITOR=C:\Project_MX\Uadeo\videojuegos\Herramientas\x64\Release\EventEditor.exe"
set "CONTENT_DIR=C:\Project_MX\Uadeo\videojuegos\FCSEDAVID4NX\Content"
set "BUILD_DIR=C:\Project_MX\Uadeo\videojuegos\FCSEDAVID4NX\Build"

echo.
if not exist "%ARC_PACKER%" (
    echo ERROR: No se encontro ArcPacker.exe: "%ARC_PACKER%"
    echo Compila ArcPacker por separado; este script no compila proyectos.
    pause
    exit /b 1
)
if not exist "%EVENT_EDITOR%" (
    echo ERROR: No se encontro EventEditor.exe: "%EVENT_EDITOR%"
    echo Compila EventEditor en Release por separado; este script no compila proyectos.
    pause
    exit /b 1
)
if not exist "%VULKAN_GLSLC%" (
    echo ERROR: No se encontro glslc.exe: "%VULKAN_GLSLC%"
    pause
    exit /b 1
)

echo [1/11] Compilando vertex shader...
"%VULKAN_GLSLC%" "%CONTENT_DIR%\data\foca.vert" -o "%CONTENT_DIR%\data\foca_vert.spv"
if errorlevel 1 (
    echo ERROR: Fallo al compilar vertex shader
    pause
    exit /b 1
)

echo [2/11] Compilando fragment shader...
"%VULKAN_GLSLC%" "%CONTENT_DIR%\data\foca.frag" -o "%CONTENT_DIR%\data\foca_frag.spv"
if errorlevel 1 (
    echo ERROR: Fallo al compilar fragment shader
    pause
    exit /b 1
)

echo [3/11] Compilando vertex shader de antialiasing...
"%VULKAN_GLSLC%" "%CONTENT_DIR%\data\foca_aa.vert" -o "%CONTENT_DIR%\data\foca_aa_vert.spv"
if errorlevel 1 (
    echo ERROR: Fallo al compilar el vertex shader de antialiasing
    pause
    exit /b 1
)

echo [4/11] Compilando fragment shader de copia del buffer de escena al fotograma de video...
"%VULKAN_GLSLC%" "%CONTENT_DIR%\data\foca_aa.frag" -o "%CONTENT_DIR%\data\foca_aa_frag.spv"
if errorlevel 1 (
    echo ERROR: Fallo al compilar el fragment shader de antialiasing
    pause
    exit /b 1
)

echo [5/11] Compilando fragment shader para presentar el fotograma de video...
"%VULKAN_GLSLC%" "%CONTENT_DIR%\data\foca_aa_present.frag" -o "%CONTENT_DIR%\data\foca_aa_present_frag.spv"
if errorlevel 1 (
	echo ERROR: Fallo al compilar el fragment shader de presentacion
	pause
	exit /b 1
)

echo [6/11] Compilando vertex shader de fuente...
"%VULKAN_GLSLC%" "%CONTENT_DIR%\data\font.vert" -o "%CONTENT_DIR%\data\font_vert.spv"
if errorlevel 1 (
    echo ERROR: Fallo al compilar el vertex shader de fuente
    pause
    exit /b 1
)

echo [7/11] Compilando fragment shader de fuente...
"%VULKAN_GLSLC%" "%CONTENT_DIR%\data\font.frag" -o "%CONTENT_DIR%\data\font_frag.spv"
if errorlevel 1 (
    echo ERROR: Fallo al compilar el fragment shader de fuente
    pause
    exit /b 1
)

echo [8/11] Empaquetando data.dat (shaders y fuente)...
if not exist "%BUILD_DIR%\data" mkdir "%BUILD_DIR%\data"
"%ARC_PACKER%" "%CONTENT_DIR%\data\data.csv" "%CONTENT_DIR%\data" "%BUILD_DIR%\data\data.dat"
if errorlevel 1 (
    echo ERROR: Fallo al empaquetar data.dat
    pause
    exit /b 1
)

echo [9/11] Empaquetando map.dat (modelos y texturas)...
if not exist "%BUILD_DIR%\map" mkdir "%BUILD_DIR%\map"
"%ARC_PACKER%" "%CONTENT_DIR%\map\map.csv" "%CONTENT_DIR%\map" "%BUILD_DIR%\map\map.dat"
if errorlevel 1 (
    echo ERROR: Fallo al empaquetar map.dat
    pause
    exit /b 1
)

if not exist "%CONTENT_DIR%\mapinfo\mapinfo.csv" (
    echo ERROR: No se encontro mapinfo.csv en "%CONTENT_DIR%\mapinfo"
    pause
    exit /b 1
)

echo [10/11] Compilando los eventos y creando un .dat por mapa...
if not exist "%BUILD_DIR%\mapinfo" mkdir "%BUILD_DIR%\mapinfo"
for %%C in ("%CONTENT_DIR%\mapinfo\*.csv") do (
    if exist "%%~fC" if /I not "%%~nxC"=="mapinfo.csv" (
        call :BuildMapInfo "%%~fC" "%%~nC"
        if errorlevel 1 (
            pause
            exit /b 1
        )
    )
)

echo [11/11] Empaquetando mapinfo.csv al final...
"%ARC_PACKER%" "%CONTENT_DIR%\mapinfo\mapinfo.csv" "%BUILD_DIR%\mapinfo" "%BUILD_DIR%\map\mapinfo.dat"
if errorlevel 1 (
    echo ERROR: Fallo al empaquetar mapinfo.dat
    pause
    exit /b 1
)

echo.
echo ===============================================================
echo    Build completado exitosamente!
echo ===============================================================
pause
exit /b 0

:BuildMapInfo
set "MAP_CSV=%~1"
set "MAP_NAME=%~2"
set "MAP_DIR=%CONTENT_DIR%\mapinfo\%MAP_NAME%"
if not exist "%MAP_DIR%\" (
    echo ERROR: No existe la carpeta de datos del mapa "%MAP_DIR%"
    exit /b 1
)

echo.
echo   Compilando archivos .events.json del mapa %MAP_NAME%...
for /r "%MAP_DIR%" %%J in (*.events.json) do (
	call :CompileEvent "%%~fJ"
    if errorlevel 1 (
        exit /b 1
    )
)

echo   Empaquetando %MAP_NAME%.dat...
"%ARC_PACKER%" "%MAP_CSV%" "%MAP_DIR%" "%BUILD_DIR%\mapinfo\%MAP_NAME%.dat"
if errorlevel 1 (
    echo ERROR: Fallo al empaquetar "%MAP_NAME%.dat"
    exit /b 1
)
exit /b 0

:CompileEvent
set "EVENT_INPUT=%~1"
set "EVENT_BASE=%~n1"
if /I "%EVENT_BASE:~-7%"==".events" set "EVENT_BASE=%EVENT_BASE:~0,-7%"
set "EVENT_OUTPUT=%~dp1%EVENT_BASE%.fest"
"%EVENT_EDITOR%" "%EVENT_INPUT%" "%EVENT_OUTPUT%"
if errorlevel 1 (
	echo ERROR: Fallo al compilar "%EVENT_INPUT%"
	exit /b 1
)
exit /b 0
