@echo off

rem set the directory to the same value as tht used to strt the bat file
setlocal enabledelayedexpansion
@setlocal enableextensions
@cd /d "%~dp0"
set NODE_MODULES_PATH=%~dp0%node_modules

set NULL_VAL=null
set NODE_VER=%NULL_VAL%
set NODE_EXEC=v22.3.0/node-v22.3.0-x86.msi

node -v >.tmp_nodever
set /p NODE_VER=<.tmp_nodever
del .tmp_nodever

set GULP_PATH=%NULL_VAL%
where gulp > .tempGulpPath
set /p GULP_PATH=<.tempGulpPath
del .tempGulpPath

IF "%NODE_VER%"=="%NULL_VAL%" (
    NET SESSION >nul 2>&1
    IF %ERRORLEVEL% NEQ 0 (
        echo This setup needs admin permissions. Please run this file as admin.
        pause
        exit
    )

	echo.
	echo Node.js is not installed! Please press a key to download and install it from the website that will open.
	PAUSE
	start "" http://nodejs.org/dist/%NODE_EXEC%
	echo.
	echo.
	echo After you have installed Node.js, press a key to shut down this process. Please restart it again afterwards.
    call npm install
	PAUSE
	EXIT
) ELSE (
	echo A version of Node.js ^(%NODE_VER%^) is installed. Proceeding...
)

set InstallGulp=0

if "%GULP_PATH%"=="%NULL_VAL%" (
    set InstallGulp=1
    echo no gulp path
    )

if NOT exist %NODE_MODULES_PATH% (
    set InstallGulp=1
    echo no modules dir
    )

rem echo NODE_MODULES_PATH = %NODE_MODULES_PATH%
rem echo GULP_PATH = %GULP_PATH%
rem echo InstallGulp = !InstallGulp!
rem pause

IF !InstallGulp! == 1 (
    NET SESSION >nul 2>&1
    IF %ERRORLEVEL% NEQ 0 (
        echo This setup needs admin permissions. Please run this file as admin.
        pause
        exit
    )
	echo.
	echo Install gulp

    call npm install
    call npm install --global gulp-cli
    call npm install --global
    call npm install --save-dev gulp
    call npm audit fix
) ELSE (
    echo gulp is installed. Proceeding...
)

echo execute gulp
gulp
pause
