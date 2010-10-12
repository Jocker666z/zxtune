@ECHO OFF

SET VS_PATH=%PROGRAMFILES%\Microsoft Visual Studio 8
ECHO %PATH% | FIND "%VS_PATH%" > NUL && GOTO Quit

call "%VS_PATH%\VC\vcvarsall.bat" x86
SET MSVS_VERSION=vc80

SET self=%0%
call %self:setup_msvs80=setup_build%
call %self:setup_msvs80=setup_boost%
call %self:setup_msvs80=setup_qt%
SET self=
:Quit
