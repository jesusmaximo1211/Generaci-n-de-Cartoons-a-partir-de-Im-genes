@echo off
set "PATH=D:\mingw64\bin;C:\Program Files\Microsoft MPI\Bin;%PATH%"
echo ========================================================
echo   VERIFICACION CRIPTOGRAFICA BIT A BIT (SHA-256)
echo   Imagen sintetica: 800x800, Filtro: 5x5, K: 9, Umbral: 70
echo ========================================================
echo.
echo Generando salida Secuencial...
bin\cartoon_seq.exe --synthetic test_seq.ppm 5 9 70 800 800 > nul
echo Generando salida OpenMP (4 hilos)...
bin\cartoon_omp.exe --synthetic test_omp.ppm 4 5 9 70 800 800 > nul
echo Generando salida MPI (1 proceso)...
mpiexec -n 1 bin\cartoon_mpi.exe --synthetic test_mpi1.ppm 5 9 70 800 800 > nul
echo Generando salida MPI (2 procesos)...
mpiexec -n 2 bin\cartoon_mpi.exe --synthetic test_mpi2.ppm 5 9 70 800 800 > nul
echo Generando salida MPI (4 procesos)...
mpiexec -n 4 bin\cartoon_mpi.exe --synthetic test_mpi4.ppm 5 9 70 800 800 > nul
echo Generando salida Hibrido (2 proc x 2 hilos)...
mpiexec -n 2 bin\cartoon_hybrid.exe --synthetic test_hyb.ppm 2 5 9 70 800 800 > nul
echo.
echo Calculando Hashes SHA-256:
powershell -NoProfile -Command "Get-FileHash test_*.ppm -Algorithm SHA256 | Format-Table -AutoSize"
del test_*.ppm > nul 2>&1
echo.
echo Todos los hashes deben ser exactamente iguales:
echo DD70A0A2133950635ECD8AE619FB45472D56588E5AA3964DE98F38B90346FBCE
echo ========================================================
