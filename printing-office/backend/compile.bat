@echo off
echo ============================================================
echo  The Printing Office — Backend Compiler
echo ============================================================
echo.
echo [1/2] Compiling backend server...
g++ -O2 -std=c++14 -o server.exe server.cpp LinkedList.cpp MinHeap.cpp MergeEngine.cpp -I. -lws2_32
if %errorlevel% neq 0 (
    echo [FAIL] Backend compilation failed.
    pause
    exit /b 1
)
echo [OK] server.exe compiled successfully.
echo.
echo [2/2] Compiling standalone tests...
cd ..\tests
g++ -O2 -std=c++14 -o test.exe test.cpp ..\backend\LinkedList.cpp ..\backend\MinHeap.cpp ..\backend\MergeEngine.cpp -I..\backend
if %errorlevel% neq 0 (
    echo [FAIL] Test compilation failed.
    pause
    exit /b 1
)
echo [OK] tests\test.exe compiled successfully.
echo.
echo Running tests...
test.exe
echo.
cd ..\backend
echo ============================================================
echo  To start the server: run server.exe
echo  Then open: frontend\index.html in your browser
echo ============================================================
pause
