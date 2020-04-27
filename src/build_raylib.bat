@echo off
REM For the ! variable notation
setlocal EnableDelayedExpansion
REM For shifting, which the command line argument parsing needs
setlocal EnableExtensions


pushd ".."
set ROOT_PATH="%cd%"
popd

set GAME_NAME=pacman_raylib

set BUILD_DEBUG=1
set BUILD_RAYLIB=
set BUILD_GAME=1

set RAYLIB_PATH=%ROOT_PATH%/libs/raylib
set RAYLIB_SRC_PATH=%RAYLIB_PATH%/raylib-3.0.0/src

set BUILD_PATH=%ROOT_PATH%/run_tree
set SRC_PATH=%ROOT_PATH%/src


:: Warning Options 
:: -Wall to turn all warnings on
:: -Werror to handle warnings as errors
:: -wd4127  conditional expression is constant
:: -wd4505  'function' : unreferenced local function
:: -wd4100  '' : unreferenced formal parameter
:: -wd4701  potentially uninitialized local variable '' used
:: -wd4201  nonstandard extension used: nameless struct/union

::
:: Flags
::
set OUTPUT_FLAG=-Fe: "%GAME_NAME%.exe"
set COMPILATION_FLAGS=-O2 -GL -fp:fast -fp:except- -Gm- -GR- -EHa-
set WARNING_FLAGS=
set SUBSYSTEM_FLAGS=-SUBSYSTEM:WINDOWS -ENTRY:mainCRTStartup
set LINK_FLAGS=/link -incremental:no -opt:ref /LTCG kernel32.lib user32.lib shell32.lib winmm.lib gdi32.lib opengl32.lib
:: Debug changes to flags
IF DEFINED BUILD_DEBUG (
  set OUTPUT_FLAG=-Fe: "%GAME_NAME%_debug.exe"
  set COMPILATION_FLAGS=-Od -MTd -diagnostics:column -diagnostics:caret -fp:fast -fp:except- -Gm- -GR- -EHa- -Z7 -Zo -FC
  set WARNING_FLAGS=-WL -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4701
  set SUBSYSTEM_FLAGS=/DEBUG -DBUILD_SLOW
  set LINK_FLAGS=/link kernel32.lib user32.lib shell32.lib winmm.lib gdi32.lib opengl32.lib
)
IF NOT DEFINED VERBOSE (
  set VERBOSITY_FLAG=/nologo
)

:: Display what we're doing
IF DEFINED BUILD_DEBUG (
  IF NOT DEFINED QUIET echo COMPILE-INFO: Compiling in debug mode, flags: %COMPILATION_FLAGS% %WARNING_FLAGS%
) ELSE (
  IF NOT DEFINED QUIET echo COMPILE-INFO: Compiling in release mode, flags: %COMPILATION_FLAGS% /link /LTCG
)
echo.

::
:: Build raylib
::
IF DEFINED BUILD_RAYLIB (
    pushd %RAYLIB_PATH%
    ctime -begin ./raylib.ctm

    set RAYLIB_DEFINES=-D_DEFAULT_SOURCE -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33
    set RAYLIB_C_FILES=%RAYLIB_SRC_PATH%/core.c %RAYLIB_SRC_PATH%/shapes.c %RAYLIB_SRC_PATH%/textures.c %RAYLIB_SRC_PATH%/text.c %RAYLIB_SRC_PATH%/models.c %RAYLIB_SRC_PATH%/utils.c %RAYLIB_SRC_PATH%/raudio.c %RAYLIB_SRC_PATH%/rglfw.c
    set RAYLIB_INCLUDE_FLAGS=-I%RAYLIB_SRC_PATH% -I%RAYLIB_SRC_PATH%/external/glfw/include

    set LastError=0
    cl !VERBOSITY_FLAG! -w -c !RAYLIB_DEFINES! !RAYLIB_INCLUDE_FLAGS! !COMPILATION_FLAGS! !RAYLIB_C_FILES! || set LastError=1

    ctime -end ./raylib.ctm !LastError!
    popd
    ::echo COMPILE-INFO: Raylib compiled into object files in: %cd%
    echo.
)


::
:: Build game
::
IF DEFINED BUILD_GAME (
    IF NOT EXIST %BUILD_PATH%  mkdir %BUILD_PATH%
    pushd %BUILD_PATH%
    ctime -begin ./raylib_playground.ctm

    del *.obj

    set LastError=0
    cl !VERBOSITY_FLAG! -c !COMPILATION_FLAGS! !WARNING_FLAGS! -I!RAYLIB_SRC_PATH! !SRC_PATH!/platform_web_raylib.cpp || set LastError=1
    cl !VERBOSITY_FLAG! !OUTPUT_FLAG! "!RAYLIB_PATH!/*.obj" *.obj !LINK_FLAGS! !SUBSYSTEM_FLAGS! || set LastError=1

    ctime -end ./raylib_playground.ctm !LastError!
    popd
    ::echo COMPILE-INFO: Game compiled into object files in: %cd%
    echo.
)

