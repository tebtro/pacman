@echo off

set filetypes=*.h *.c *.cpp *.inl

echo STATICS FOUND:
findstr -s -n -i -l "static" %filetypes%

echo.

echo LOCAL_PERSIST FOUND:
findstr -s -n -i -l "local_persist" %filetypes%

echo.

echo GLOBALS FOUND:
findstr -s -n -i -l "global_variable" %filetypes%
findstr -s -n -i -l "global" %filetypes%