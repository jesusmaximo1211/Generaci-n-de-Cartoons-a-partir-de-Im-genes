# Informe de Desempeño: Generación de Cartoons RGB (HPC)
**Computación Avanzada - Práctico Final Integrador**

## 1. Entorno de Ejecución
- **Fecha:** 2026-11-04 19:17:52
- **Sistema Operativo:** MINGW64_NT-10.0-19045 3.6.12-50c44b7c.x86_64 x86_64
- **Procesador / Cores:** 12 cores detectados
- **Compilador:** gcc.exe (Rev6, Built by MSYS2 project) 16.2.0
- **MPI:** Microsoft MPI (MS-MPI)

## 2. Metodología
Las pruebas se realizan con imágenes sintéticas autogeneradas para garantizar repetibilidad:
- **Resoluciones:** 800x800, 2000x2000, 5000x5000
- **Máscaras de Media (Filtro):** 3x3 (radio 1), 5x5 (radio 2) con borde neutro negro (0,0,0)
- **Posterización:** K = 3 y K = 9 niveles por canal RGB
- **Métricas:** 
  - $T_{seq}$: Tiempo secuencial promedio
  - $T_{par}$: Tiempo paralelo promedio
  - $\text{Speedup } (S) = \frac{T_{seq}}{T_{par}}$
  - $\text{Eficiencia } (E) = \frac{S}{P} = \frac{T_{seq}}{P \times T_{par}}$

---

## 3. Tablas de Resultados y Métricas

### Imagen: 800x800 píxeles

| Filtro | K Niveles | Paradigma | Configuración | Tiempo Promedio (s) | Speedup (S) | Eficiencia (E) |
| :---: | :---: | :--- | :--- | :---: | :---: | :---: |
| 3x3 | 3 | Secuencial | 1 Core | 0.022475 | 1.00x | 100.0% |
| 3x3 | 3 | OpenMP | 2 Hilos | 0.016000 | 1.40x | 70.0% |
| 3x3 | 3 | OpenMP | 4 Hilos | 0.010000 | 2.25x | 56.2% |
| 3x3 | 3 | MPI | 2 Procesos | 0.012488 | 1.80x | 90.0% |
| 3x3 | 3 | MPI | 4 Procesos | 0.006577 | 3.42x | 85.5% |
| 3x3 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.006522 | 3.45x | 86.2% |
| 3x3 | 9 | Secuencial | 1 Core | 0.022748 | 1.00x | 100.0% |
| 3x3 | 9 | OpenMP | 2 Hilos | 0.015667 | 1.45x | 72.5% |
| 3x3 | 9 | OpenMP | 4 Hilos | 0.010000 | 2.27x | 56.8% |
| 3x3 | 9 | MPI | 2 Procesos | 0.013012 | 1.75x | 87.5% |
| 3x3 | 9 | MPI | 4 Procesos | 0.007942 | 2.86x | 71.5% |
| 3x3 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.006062 | 3.75x | 93.8% |
| 5x5 | 3 | Secuencial | 1 Core | 0.031070 | 1.00x | 100.0% |
| 5x5 | 3 | OpenMP | 2 Hilos | 0.020333 | 1.53x | 76.5% |
| 5x5 | 3 | OpenMP | 4 Hilos | 0.012667 | 2.45x | 61.3% |
| 5x5 | 3 | MPI | 2 Procesos | 0.012393 | 2.51x | 125.5% |
| 5x5 | 3 | MPI | 4 Procesos | 0.008521 | 3.65x | 91.2% |
| 5x5 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.005765 | 5.39x | 134.8% |
| 5x5 | 9 | Secuencial | 1 Core | 0.031068 | 1.00x | 100.0% |
| 5x5 | 9 | OpenMP | 2 Hilos | 0.020667 | 1.50x | 75.0% |
| 5x5 | 9 | OpenMP | 4 Hilos | 0.013000 | 2.39x | 59.8% |
| 5x5 | 9 | MPI | 2 Procesos | 0.012385 | 2.51x | 125.5% |
| 5x5 | 9 | MPI | 4 Procesos | 0.010090 | 3.08x | 77.0% |
| 5x5 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.006193 | 5.02x | 125.5% |

### Imagen: 2000x2000 píxeles

| Filtro | K Niveles | Paradigma | Configuración | Tiempo Promedio (s) | Speedup (S) | Eficiencia (E) |
| :---: | :---: | :--- | :--- | :---: | :---: | :---: |
| 3x3 | 3 | Secuencial | 1 Core | 0.138912 | 1.00x | 100.0% |
| 3x3 | 3 | OpenMP | 2 Hilos | 0.093667 | 1.48x | 74.0% |
| 3x3 | 3 | OpenMP | 4 Hilos | 0.059000 | 2.35x | 58.8% |
| 3x3 | 3 | MPI | 2 Procesos | 0.029787 | 4.66x | 233.0% |
| 3x3 | 3 | MPI | 4 Procesos | 0.018621 | 7.46x | 186.5% |
| 3x3 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.016274 | 8.54x | 213.5% |
| 3x3 | 9 | Secuencial | 1 Core | 0.144823 | 1.00x | 100.0% |
| 3x3 | 9 | OpenMP | 2 Hilos | 0.095333 | 1.52x | 76.0% |
| 3x3 | 9 | OpenMP | 4 Hilos | 0.054667 | 2.65x | 66.2% |
| 3x3 | 9 | MPI | 2 Procesos | 0.030296 | 4.78x | 239.0% |
| 3x3 | 9 | MPI | 4 Procesos | 0.019184 | 7.55x | 188.8% |
| 3x3 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.017967 | 8.06x | 201.5% |
| 5x5 | 3 | Secuencial | 1 Core | 0.193865 | 1.00x | 100.0% |
| 5x5 | 3 | OpenMP | 2 Hilos | 0.125000 | 1.55x | 77.5% |
| 5x5 | 3 | OpenMP | 4 Hilos | 0.077333 | 2.51x | 62.7% |
| 5x5 | 3 | MPI | 2 Procesos | 0.029996 | 6.46x | 323.0% |
| 5x5 | 3 | MPI | 4 Procesos | 0.017764 | 10.91x | 272.8% |
| 5x5 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.012521 | 15.48x | 387.0% |
| 5x5 | 9 | Secuencial | 1 Core | 0.198249 | 1.00x | 100.0% |
| 5x5 | 9 | OpenMP | 2 Hilos | 0.132333 | 1.50x | 75.0% |
| 5x5 | 9 | OpenMP | 4 Hilos | 0.072667 | 2.73x | 68.2% |
| 5x5 | 9 | MPI | 2 Procesos | 0.029973 | 6.61x | 330.5% |
| 5x5 | 9 | MPI | 4 Procesos | 0.018867 | 10.51x | 262.8% |
| 5x5 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.013069 | 15.17x | 379.2% |

### Imagen: 5000x5000 píxeles

| Filtro | K Niveles | Paradigma | Configuración | Tiempo Promedio (s) | Speedup (S) | Eficiencia (E) |
| :---: | :---: | :--- | :--- | :---: | :---: | :---: |
| 3x3 | 3 | Secuencial | 1 Core | 0.878782 | 1.00x | 100.0% |
| 3x3 | 3 | OpenMP | 2 Hilos | 0.619333 | 1.42x | 71.0% |
| 3x3 | 3 | OpenMP | 4 Hilos | 0.343000 | 2.56x | 64.0% |
| 3x3 | 3 | MPI | 2 Procesos | 0.079215 | 11.09x | 554.5% |
| 3x3 | 3 | MPI | 4 Procesos | 0.048449 | 18.14x | 453.5% |
| 3x3 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.036640 | 23.98x | 599.5% |
| 3x3 | 9 | Secuencial | 1 Core | 0.884634 | 1.00x | 100.0% |
| 3x3 | 9 | OpenMP | 2 Hilos | 0.604667 | 1.46x | 73.0% |
| 3x3 | 9 | OpenMP | 4 Hilos | 0.346333 | 2.55x | 63.7% |
| 3x3 | 9 | MPI | 2 Procesos | 0.075896 | 11.66x | 583.0% |
| 3x3 | 9 | MPI | 4 Procesos | 0.041687 | 21.22x | 530.5% |
| 3x3 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.033564 | 26.36x | 659.0% |
| 5x5 | 3 | Secuencial | 1 Core | 1.192367 | 1.00x | 100.0% |
| 5x5 | 3 | OpenMP | 2 Hilos | 0.787333 | 1.51x | 75.5% |
| 5x5 | 3 | OpenMP | 4 Hilos | 0.434333 | 2.75x | 68.8% |
| 5x5 | 3 | MPI | 2 Procesos | 0.073745 | 16.17x | 808.5% |
| 5x5 | 3 | MPI | 4 Procesos | 0.043294 | 27.54x | 688.5% |
| 5x5 | 3 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.029206 | 40.83x | 1020.8% |
| 5x5 | 9 | Secuencial | 1 Core | 1.209819 | 1.00x | 100.0% |
| 5x5 | 9 | OpenMP | 2 Hilos | 0.768333 | 1.57x | 78.5% |
| 5x5 | 9 | OpenMP | 4 Hilos | 0.426667 | 2.84x | 71.0% |
| 5x5 | 9 | MPI | 2 Procesos | 0.074165 | 16.31x | 815.5% |
| 5x5 | 9 | MPI | 4 Procesos | 0.047672 | 25.38x | 634.5% |
| 5x5 | 9 | Híbrido | 2 Proc x 2 OMP (4 cores) | 0.030498 | 39.67x | 991.8% |


## 4. Análisis y Conclusiones Técnicas
1. **Escalabilidad y Cuello de Botella de Memoria vs Cómputo:**
   - El filtro de media 5x5 realiza 25 accesos y acumulaciones por píxel frente a los 9 accesos de 3x3, lo que incrementa la intensidad aritmética y amortigua mejor el overhead de sincronización tanto en OpenMP como en MPI.
2. **Impacto de la Comunicación de Halos en MPI:**
   - Para resoluciones pequeñas (800x800), el costo de `MPI_Sendrecv` y `MPI_Scatterv`/`MPI_Gatherv` representa una porción apreciable del tiempo total.
   - En resoluciones grandes (2000x2000 y 5000x5000), el cómputo domina notablemente sobre la comunicación, logrando factores de aceleración lineales.
3. **Comportamiento Híbrido:**
   - La combinación híbrida (MPI multinodo/multiproceso + OpenMP local) reduce el volumen total de mensajes y transfiere la carga fina de los bucles a hilos ligeros que comparten caché de nivel L2/L3.
