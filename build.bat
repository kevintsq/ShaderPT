@echo off
set compilerFlags = /Zi /fp:fast /arch:AVX2

IF "%~1" == "-d" (
    :: Debug build
    set compilerFlags = /Od %compilerFlags%
) ELSE (
    set compilerFlags = /O2 %compilerFlags%
)

cl %compilerFlags% main.c glad.c glad_wgl.c /link user32.lib opengl32.lib gdi32.lib kernel32.lib
IF %ERRORLEVEL% == 0 (
    main
)
