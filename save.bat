@echo off
setlocal
cd /d "%~dp0"

git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 goto no_git

git add -A
if errorlevel 1 goto add_error

git diff --cached --quiet
if errorlevel 1 goto make_commit

echo No hay cambios nuevos. Intentando subir commits pendientes...
goto push_changes

:make_commit
set "MENSAJE=%~1"
if not defined MENSAJE set "MENSAJE=Actualizacion automatica"
git commit -m "%MENSAJE%"
if errorlevel 1 goto commit_error

:push_changes
git push -u origin main
if errorlevel 1 goto push_error

echo.
echo Proyecto actualizado en GitHub.
pause
exit /b 0

:no_git
echo Error: no se encontro un repositorio Git en esta carpeta.
goto failed

:add_error
echo Error al agregar los archivos al commit.
goto failed

:commit_error
echo Error al crear el commit.
goto failed

:push_error
echo No se pudo subir a GitHub. El commit local se conserva; vuelve a ejecutar este archivo cuando quieras reintentar.
goto failed

:failed
pause
exit /b 1
