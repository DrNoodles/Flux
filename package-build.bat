@echo off

rem DELETE EXPORT
rd /q /s "Export" 2>nul

rem CREATE EMPTY EXPORT
mkdir %CD%\\Export
set outputdir=%CD%\Export

rem TODO COPY EXE AND SHADERS FROM BIN to EXPORT/BIN


rem TODO DATA DIR TO EXPORT

