# Guía de Ejecución y Arquitectura: Generación de Cartoons RGB en HPC

Este proyecto implementa el Práctico Final Integrador de **Computación Avanzada**, aplicando técnicas de Computación de Alto Rendimiento (HPC) para la generación de cartoons sobre imágenes RGB (formato PPM P6 binario).

---

## 1. Estructura del Proyecto

```text
TP_Final_CA/
├── include/
│   ├── ppm_utils.h          # Parser/escritor PPM P6 y generador de imágenes sintéticas
│   └── cartoon_core.h       # Filtro de media 3x3/5x5, Sobel, Umbralado, Posterizado y Suma
├── src/
│   ├── cartoon_seq.c        # Versión secuencial en C puro
│   ├── cartoon_omp.c        # Paralelismo de memoria compartida con OpenMP
│   ├── cartoon_mpi.c        # Paralelismo distribuido con MPI (Scatterv/Gatherv + Halo Exchange)
│   └── cartoon_hybrid.c     # Paradigma Híbrido MPI + OpenMP
├── Makefile                 # Compilación modular con gcc y mpicc (-O3 -fopenmp)
├── run_benchmarks.sh        # Script automatizado de pruebas, Speedup, Eficiencia y resultados.md
└── README.md                # Documentación técnica y guía de usuario
```

---

## 2. Algoritmo y Pipeline de Cómputo

El pipeline procesa los 3 canales RGB:
1. **Detección de Bordes en 3 Pasos:**
   - **Paso 1:** Filtro pasa-bajos de media (Box Blur) parametrizable a máscaras de **3x3** (radio $R=1$) o **5x5** ($R=2$) con condición de borde neutro negro `(0, 0, 0)`.
   - **Paso 2:** Resaltado de gradiente con operador de **Sobel** ($3\times 3$) sobre la luminancia calculada de la imagen suavizada:
     $$G_x = \begin{bmatrix}-1 & 0 & +1\\ -2 & 0 & +2\\ -1 & 0 & +1\end{bmatrix}, \quad G_y = \begin{bmatrix}-1 & -2 & -1\\ 0 & 0 & 0\\ +1 & +2 & +1\end{bmatrix}$$
     $$\|\nabla I\| = \sqrt{G_x^2 + G_y^2}$$
   - **Paso 3:** Umbralado binario (Thresholding) para generar máscara de bordes (255 si supera el umbral, 0 si no).
2. **Posterizado por canal RGB:**
   - Mapeo de $[0, 255]$ a $K$ rangos discretos (parametrizable a $K=3$ y $K=9$).
3. **Suma Saturada (Clamp):**
   - Combina la imagen posterizada y la detección de bordes mediante:
     $$\text{dst}(x,y) = \text{clamp}_{[0,255]}(\text{posterizado}(x,y) + \text{borde}(x,y))$$

---

## 3. Descomposición de Dominio e Intercambio de Halos (MPI e Híbrido)

- **Distribución de Celdas:**
  Se divide la imagen por **bloques contiguos de filas** ($N$ filas entre $P$ procesos). Soporta $N$ no divisible por $P$ mediante `MPI_Scatterv` y `MPI_Gatherv`.
- **Celdas Fantasma (Halos):**
  - Para filtro de **3x3**: Se intercambia $1$ fila superior con el vecino $rank - 1$ y $1$ fila inferior con el vecino $rank + 1$ usando `MPI_Sendrecv`.
  - Para filtro de **5x5**: Se intercambian $2$ filas en cada dirección.
  - Los procesos en los extremos ($rank = 0$ y $rank = P - 1$) aplican automáticamente el borde neutro negro en sus halos externos.

---

## 4. Compilación y Ejecución Manual

### Requisitos
- **Windows:** MSYS2 con MinGW 64-bit (`gcc`, `make`), Microsoft MPI (Runtime + SDK) y paquete `mingw-w64-x86_64-msmpi`.
- **Linux / WSL (alternativa):** `build-essential` (`gcc`), `libopenmpi-dev` y `openmpi-bin`.

### Compilar
```bash
make all
```

### Ejecutar Versiones

#### A. Secuencial:
```bash
./bin/cartoon_seq --synthetic salida_seq.ppm 3 9 70 2000 2000
# o con imagen existente:
./bin/cartoon_seq imagen.ppm salida_seq.ppm 3 9 70
```

#### B. OpenMP (Memoria Compartida):
```bash
./bin/cartoon_omp --synthetic salida_omp.ppm 4 3 9 70 2000 2000
# Argumentos: <input> <output> <num_threads> <filtro:3|5> <k:3|9> <umbral> [ancho] [alto]
```

#### C. MPI (Memoria Distribuida):
```bash
# Windows (MS-MPI):
mpiexec -n 4 ./bin/cartoon_mpi --synthetic salida_mpi.ppm 3 9 70 2000 2000
# Linux (OpenMPI):
# mpirun -np 4 ./bin/cartoon_mpi --synthetic salida_mpi.ppm 3 9 70 2000 2000
# Argumentos: <input> <output> <filtro:3|5> <k:3|9> <umbral> [ancho] [alto]
```

#### D. Híbrido (MPI + OpenMP):
```bash
# Windows (MS-MPI):
mpiexec -n 2 ./bin/cartoon_hybrid --synthetic salida_hyb.ppm 2 3 9 70 2000 2000
# Linux (OpenMPI):
# mpirun -np 2 ./bin/cartoon_hybrid --synthetic salida_hyb.ppm 2 3 9 70 2000 2000
# Argumentos: <input> <output> <threads_por_proceso> <filtro:3|5> <k:3|9> <umbral> [ancho] [alto]
```

---

## 5. Pruebas de Rendimiento Automatizadas

Para compilar y ejecutar la suite completa midiendo **Tiempo**, **Speedup** y **Eficiencia** sobre imágenes de 800x800, 2000x2000 y 5000x5000:

```bash
chmod +x run_benchmarks.sh
./run_benchmarks.sh
```
El script generará el reporte consolidado en `resultados.md`.
