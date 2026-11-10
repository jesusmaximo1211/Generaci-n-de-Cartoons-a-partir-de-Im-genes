# Informe de Desempeño: Generación de Cartoons RGB (HPC)
**Computación Avanzada - Práctico Final Integrador**

## 1. Entorno de Ejecución
- **Fecha:** 2026-11-10
- **Sistema Operativo:** Windows 10 Pro (MINGW64_NT-10.0-19045 x86_64)
- **Procesador / Cores:** Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz (6 núcleos físicos / 12 hilos lógicos)
- **Compilador:** gcc.exe (Rev6, Built by MSYS2 project) 16.2.0 (`-O3 -fopenmp`)
- **MPI:** Microsoft MPI v10.1 (MS-MPI)

## 2. Metodología de Medición
Las pruebas se realizan con imágenes sintéticas autogeneradas para garantizar repetibilidad estricta.
- **Resoluciones:** 800x800, 2000x2000, 5000x5000 píxeles
- **Máscaras de Media (Filtro):** 3x3 (radio 1), 5x5 (radio 2) con borde neutro negro (0,0,0)
- **Posterización:** K = 3 y K = 9 niveles por canal RGB
- **Criterio de Cronometraje (End-to-End):** 
  - En todos los paradigmas se excluye la lectura y escritura de archivos en disco.
  - En **MPI** e **Híbrido**, el tiempo medido $T_{par}$ es el tiempo **End-to-End**, abarcando desde el inicio de `MPI_Scatterv`, pasando por los dos intercambios de halos (`MPI_Sendrecv`) y el cómputo local, hasta la finalización de `MPI_Gatherv`, sincronizado mediante `MPI_Barrier`.
- **Métricas:** 
  - $T_{seq}$: Tiempo secuencial promedio
  - $T_{par}$: Tiempo paralelo promedio (End-to-End)
  - $\text{Speedup } (S) = \frac{T_{seq}}{T_{par}}$
  - $\text{Eficiencia } (E) = \frac{S}{P} \times 100\%$

---

## 3. Tablas de Resultados y Métricas

### Imagen: 800x800 píxeles

| Filtro | K Niveles | Paradigma | Configuración | Tiempo Promedio (s) | Speedup (S) | Eficiencia (E) |
| :---: | :---: | :--- | :--- | :---: | :---: | :---: |
| 3x3 | 3 | Secuencial | 1 Core | 0.022475 | 1.00x | 100.0% |
| 3x3 | 3 | OpenMP | 2 Hilos | 0.016000 | 1.40x | 70.0% |
| 3x3 | 3 | OpenMP | 4 Hilos | 0.010000 | 2.25x | 56.2% |
| 3x3 | 3 | OpenMP | 8 Hilos | 0.007500 | 3.00x | 37.5% |
| 3x3 | 3 | OpenMP | 12 Hilos | 0.006333 | 3.55x | 29.6% |
| 3x3 | 3 | MPI | 2 Procesos | 0.014210 | 1.58x | 79.1% |
| 3x3 | 3 | MPI | 4 Procesos | 0.010850 | 2.07x | 51.8% |
| 3x3 | 3 | MPI | 8 Procesos | 0.008420 | 2.67x | 33.4% |
| 3x3 | 3 | MPI | 12 Procesos | 0.007310 | 3.07x | 25.6% |
| 3x3 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.011240 | 2.00x | 50.0% |
| 3x3 | 9 | Secuencial | 1 Core | 0.022748 | 1.00x | 100.0% |
| 3x3 | 9 | OpenMP | 2 Hilos | 0.015667 | 1.45x | 72.5% |
| 3x3 | 9 | OpenMP | 4 Hilos | 0.010000 | 2.27x | 56.8% |
| 3x3 | 9 | OpenMP | 8 Hilos | 0.007667 | 2.97x | 37.1% |
| 3x3 | 9 | OpenMP | 12 Hilos | 0.006500 | 3.50x | 29.2% |
| 3x3 | 9 | MPI | 2 Procesos | 0.014520 | 1.57x | 78.3% |
| 3x3 | 9 | MPI | 4 Procesos | 0.011120 | 2.05x | 51.1% |
| 3x3 | 9 | MPI | 8 Procesos | 0.008650 | 2.63x | 32.9% |
| 3x3 | 9 | MPI | 12 Procesos | 0.007490 | 3.04x | 25.3% |
| 3x3 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.011480 | 1.98x | 49.5% |
| 5x5 | 3 | Secuencial | 1 Core | 0.031273 | 1.00x | 100.0% |
| 5x5 | 3 | OpenMP | 2 Hilos | 0.020333 | 1.54x | 76.9% |
| 5x5 | 3 | OpenMP | 4 Hilos | 0.012000 | 2.61x | 65.2% |
| 5x5 | 3 | OpenMP | 8 Hilos | 0.009000 | 3.47x | 43.4% |
| 5x5 | 3 | OpenMP | 12 Hilos | 0.007667 | 4.08x | 34.0% |
| 5x5 | 3 | MPI | 2 Procesos | 0.017518 | 1.79x | 89.3% |
| 5x5 | 3 | MPI | 4 Procesos | 0.014207 | 2.20x | 55.0% |
| 5x5 | 3 | MPI | 8 Procesos | 0.009740 | 3.21x | 40.1% |
| 5x5 | 3 | MPI | 12 Procesos | 0.008120 | 3.85x | 32.1% |
| 5x5 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.013890 | 2.25x | 56.3% |
| 5x5 | 9 | Secuencial | 1 Core | 0.031403 | 1.00x | 100.0% |
| 5x5 | 9 | OpenMP | 2 Hilos | 0.020667 | 1.52x | 76.0% |
| 5x5 | 9 | OpenMP | 4 Hilos | 0.011000 | 2.85x | 71.4% |
| 5x5 | 9 | OpenMP | 8 Hilos | 0.008667 | 3.62x | 45.3% |
| 5x5 | 9 | OpenMP | 12 Hilos | 0.007333 | 4.28x | 35.7% |
| 5x5 | 9 | MPI | 2 Procesos | 0.017701 | 1.77x | 88.7% |
| 5x5 | 9 | MPI | 4 Procesos | 0.015368 | 2.04x | 51.1% |
| 5x5 | 9 | MPI | 8 Procesos | 0.009862 | 3.18x | 39.8% |
| 5x5 | 9 | MPI | 12 Procesos | 0.008055 | 3.90x | 32.5% |
| 5x5 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.014120 | 2.22x | 55.6% |

### Imagen: 2000x2000 píxeles

| Filtro | K Niveles | Paradigma | Configuración | Tiempo Promedio (s) | Speedup (S) | Eficiencia (E) |
| :---: | :---: | :--- | :--- | :---: | :---: | :---: |
| 3x3 | 3 | Secuencial | 1 Core | 0.138912 | 1.00x | 100.0% |
| 3x3 | 3 | OpenMP | 2 Hilos | 0.093667 | 1.48x | 74.2% |
| 3x3 | 3 | OpenMP | 4 Hilos | 0.059000 | 2.35x | 58.9% |
| 3x3 | 3 | OpenMP | 8 Hilos | 0.043333 | 3.21x | 40.1% |
| 3x3 | 3 | OpenMP | 12 Hilos | 0.036667 | 3.79x | 31.6% |
| 3x3 | 3 | MPI | 2 Procesos | 0.079410 | 1.75x | 87.5% |
| 3x3 | 3 | MPI | 4 Procesos | 0.051240 | 2.71x | 67.8% |
| 3x3 | 3 | MPI | 8 Procesos | 0.041890 | 3.32x | 41.5% |
| 3x3 | 3 | MPI | 12 Procesos | 0.038450 | 3.61x | 30.1% |
| 3x3 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.053120 | 2.62x | 65.4% |
| 3x3 | 9 | Secuencial | 1 Core | 0.144823 | 1.00x | 100.0% |
| 3x3 | 9 | OpenMP | 2 Hilos | 0.095333 | 1.52x | 76.0% |
| 3x3 | 9 | OpenMP | 4 Hilos | 0.054667 | 2.65x | 66.2% |
| 3x3 | 9 | OpenMP | 8 Hilos | 0.042000 | 3.45x | 43.1% |
| 3x3 | 9 | OpenMP | 12 Hilos | 0.035333 | 4.10x | 34.2% |
| 3x3 | 9 | MPI | 2 Procesos | 0.081150 | 1.78x | 89.2% |
| 3x3 | 9 | MPI | 4 Procesos | 0.052310 | 2.77x | 69.2% |
| 3x3 | 9 | MPI | 8 Procesos | 0.042610 | 3.40x | 42.5% |
| 3x3 | 9 | MPI | 12 Procesos | 0.039120 | 3.70x | 30.9% |
| 3x3 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.054280 | 2.67x | 66.7% |
| 5x5 | 3 | Secuencial | 1 Core | 0.195083 | 1.00x | 100.0% |
| 5x5 | 3 | OpenMP | 2 Hilos | 0.125000 | 1.56x | 78.0% |
| 5x5 | 3 | OpenMP | 4 Hilos | 0.081000 | 2.41x | 60.2% |
| 5x5 | 3 | OpenMP | 8 Hilos | 0.054000 | 3.61x | 45.2% |
| 5x5 | 3 | OpenMP | 12 Hilos | 0.045000 | 4.34x | 36.1% |
| 5x5 | 3 | MPI | 2 Procesos | 0.108977 | 1.79x | 89.5% |
| 5x5 | 3 | MPI | 4 Procesos | 0.068239 | 2.86x | 71.5% |
| 5x5 | 3 | MPI | 8 Procesos | 0.051410 | 3.79x | 47.4% |
| 5x5 | 3 | MPI | 12 Procesos | 0.047890 | 4.07x | 33.9% |
| 5x5 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.075339 | 2.59x | 64.7% |
| 5x5 | 9 | Secuencial | 1 Core | 0.198249 | 1.00x | 100.0% |
| 5x5 | 9 | OpenMP | 2 Hilos | 0.132333 | 1.50x | 75.0% |
| 5x5 | 9 | OpenMP | 4 Hilos | 0.072667 | 2.73x | 68.2% |
| 5x5 | 9 | OpenMP | 8 Hilos | 0.053333 | 3.72x | 46.5% |
| 5x5 | 9 | OpenMP | 12 Hilos | 0.044333 | 4.47x | 37.3% |
| 5x5 | 9 | MPI | 2 Procesos | 0.106540 | 1.86x | 93.0% |
| 5x5 | 9 | MPI | 4 Procesos | 0.063985 | 3.10x | 77.5% |
| 5x5 | 9 | MPI | 8 Procesos | 0.051820 | 3.83x | 47.8% |
| 5x5 | 9 | MPI | 12 Procesos | 0.048924 | 4.05x | 33.8% |
| 5x5 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.074810 | 2.65x | 66.3% |

### Imagen: 5000x5000 píxeles

| Filtro | K Niveles | Paradigma | Configuración | Tiempo Promedio (s) | Speedup (S) | Eficiencia (E) |
| :---: | :---: | :--- | :--- | :---: | :---: | :---: |
| 3x3 | 3 | Secuencial | 1 Core | 0.878782 | 1.00x | 100.0% |
| 3x3 | 3 | OpenMP | 2 Hilos | 0.619333 | 1.42x | 71.0% |
| 3x3 | 3 | OpenMP | 4 Hilos | 0.343000 | 2.56x | 64.0% |
| 3x3 | 3 | OpenMP | 8 Hilos | 0.251000 | 3.50x | 43.8% |
| 3x3 | 3 | OpenMP | 12 Hilos | 0.212000 | 4.15x | 34.5% |
| 3x3 | 3 | MPI | 2 Procesos | 0.504120 | 1.74x | 87.2% |
| 3x3 | 3 | MPI | 4 Procesos | 0.321450 | 2.73x | 68.3% |
| 3x3 | 3 | MPI | 8 Procesos | 0.268900 | 3.27x | 40.9% |
| 3x3 | 3 | MPI | 12 Procesos | 0.219400 | 4.01x | 33.4% |
| 3x3 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.364200 | 2.41x | 60.3% |
| 3x3 | 9 | Secuencial | 1 Core | 0.884634 | 1.00x | 100.0% |
| 3x3 | 9 | OpenMP | 2 Hilos | 0.604667 | 1.46x | 73.0% |
| 3x3 | 9 | OpenMP | 4 Hilos | 0.346333 | 2.55x | 63.7% |
| 3x3 | 9 | OpenMP | 8 Hilos | 0.248000 | 3.57x | 44.6% |
| 3x3 | 9 | OpenMP | 12 Hilos | 0.209000 | 4.23x | 35.3% |
| 3x3 | 9 | MPI | 2 Procesos | 0.501230 | 1.76x | 88.2% |
| 3x3 | 9 | MPI | 4 Procesos | 0.319840 | 2.77x | 69.1% |
| 3x3 | 9 | MPI | 8 Procesos | 0.265410 | 3.33x | 41.7% |
| 3x3 | 9 | MPI | 12 Procesos | 0.217800 | 4.06x | 33.8% |
| 3x3 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.361800 | 2.45x | 61.1% |
| 5x5 | 3 | Secuencial | 1 Core | 1.246977 | 1.00x | 100.0% |
| 5x5 | 3 | OpenMP | 2 Hilos | 0.812000 | 1.54x | 76.8% |
| 5x5 | 3 | OpenMP | 4 Hilos | 0.455000 | 2.74x | 68.5% |
| 5x5 | 3 | OpenMP | 8 Hilos | 0.311000 | 4.01x | 50.1% |
| 5x5 | 3 | OpenMP | 12 Hilos | 0.258000 | 4.83x | 40.3% |
| 5x5 | 3 | MPI | 2 Procesos | 0.671240 | 1.86x | 92.9% |
| 5x5 | 3 | MPI | 4 Procesos | 0.419820 | 2.97x | 74.3% |
| 5x5 | 3 | MPI | 8 Procesos | 0.358410 | 3.48x | 43.5% |
| 5x5 | 3 | MPI | 12 Procesos | 0.261890 | 4.76x | 39.7% |
| 5x5 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.539120 | 2.31x | 57.8% |
| 5x5 | 9 | Secuencial | 1 Core | 1.254905 | 1.00x | 100.0% |
| 5x5 | 9 | OpenMP | 2 Hilos | 0.802000 | 1.56x | 78.2% |
| 5x5 | 9 | OpenMP | 4 Hilos | 0.435000 | 2.88x | 72.1% |
| 5x5 | 9 | OpenMP | 8 Hilos | 0.307000 | 4.09x | 51.1% |
| 5x5 | 9 | OpenMP | 12 Hilos | 0.254000 | 4.94x | 41.2% |
| 5x5 | 9 | MPI | 2 Procesos | 0.674190 | 1.86x | 93.1% |
| 5x5 | 9 | MPI | 4 Procesos | 0.429736 | 2.92x | 73.0% |
| 5x5 | 9 | MPI | 8 Procesos | 0.366257 | 3.43x | 42.8% |
| 5x5 | 9 | MPI | 12 Procesos | 0.265062 | 4.73x | 39.4% |
| 5x5 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.548343 | 2.29x | 57.2% |
| 5x5 | 9 | Híbrido | 2 Proc x 4 OMP (8 cores) | 0.342009 | 3.67x | 45.9% |
| 5x5 | 9 | Híbrido | 2 Proc x 6 OMP (12 cores) | 0.329129 | 3.81x | 31.8% |

---

## 4. Análisis y Conclusiones Técnicas
1. **Medición End-to-End Justa:**
   - Al incluir `MPI_Scatterv` y `MPI_Gatherv` dentro del cronómetro de MPI e Híbrido, todas las métricas de eficiencia se ubican dentro de los límites físicos reales ($<100\%$), cumpliendo estrictamente con la Ley de Amdahl.
2. **Escalabilidad en 4, 8 y 12 Procesos/Hilos:**
   - En el procesador Intel Core i7-8700 (6 núcleos físicos / 12 hilos lógicos), pasar de 1 a 4 procesos logra eficiencias altas (**73.0% en MPI** y **72.1% en OpenMP**).
   - Al escalar a **8 y 12 procesos**, el tiempo absoluto continúa bajando (llegando a **0.265 s** en MPI y **0.254 s** en OpenMP, con Speedups de **4.73x** y **4.94x**), pero la eficiencia disminuye al ~40% debido al uso de núcleos lógicos (Hyper-Threading) y a la saturación del ancho de banda de memoria RAM.
3. **Intensidad Aritmética (3x3 vs 5x5):**
   - El filtro 5x5 realiza 25 operaciones por píxel frente a las 9 del 3x3, amortiguando mejor el costo de comunicación y logrando mayores factores de aceleración en todas las configuraciones.
