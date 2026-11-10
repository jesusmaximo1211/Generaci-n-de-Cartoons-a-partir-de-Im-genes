# Informe Tecnico y Analisis Experimental: Pipeline de Generacion de Cartoons RGB en HPC

Catedra: Computacion Avanzada (4to Ano)
Autor: Jesus Maximo Serra
Fecha: Noviembre 2026
Repositorio: https://github.com/jesusmaximo1211/Generaci-n-de-Cartoons-a-partir-de-Im-genes


1. Resumen Ejecutivo

Este trabajo documenta el diseno, implementacion, optimizacion y analisis experimental del Practico Final Integrador de Computacion Avanzada. El objetivo central consiste en transformar imagenes digitales a color (formato binario PPM P6) hacia una estetica visual tipo comic o cartoon, explotando diferentes paradigmas de Computacion de Alto Rendimiento (HPC):

1. Secuencial puro en C (linea base).
2. Memoria Compartida con OpenMP (multithreading a nivel de bucles con 2, 4, 8 y 12 hilos).
3. Memoria Distribuida con MPI (descomposicion de dominio 1D con doble intercambio de halos y manejo de restos con Scatterv y Gatherv en 2, 4, 8 y 12 procesos).
4. Paradigma Hibrido (MPI + OpenMP) con soporte MPI_THREAD_FUNNELED.

Las pruebas experimentales se realizaron sobre resoluciones de 800x800, 2000x2000 y 5000x5000 pixeles, evaluando mascaras de media de 3x3 y 5x5, posterizaciones de K=3 y K=9 niveles, y escalando en 2, 4, 8 y 12 procesos/hilos en un procesador Intel Core i7-8700 (6 nucleos fisicos / 12 hilos logicos). Todas las mediciones en MPI e Hibrido se tomaron bajo criterio End-to-End (incluyendo MPI_Scatterv y MPI_Gatherv dentro del cronometro). Se valido ademas la equivalencia estricta bit a bit entre todos los paradigmas mediante sumas de verificacion criptografica SHA-256.


2. Pipeline Algoritmico y Fundamentos Teoricos

El procesamiento de estilo cartoon combina un mapa binario de contornos destacados superpuesto sobre una imagen cuantizada en color.

El flujo secuencial de operaciones se divide en dos ramas concurrentes:
- Rama de bordes: la imagen de entrada pasa por un filtro de media pasa-bajos (3x3 o 5x5) para reducir el ruido. Sobre la imagen suavizada se calcula la luminancia y se aplica el operador diferencial de Sobel (3x3). La magnitud del gradiente resultante se binariza mediante un umbral fijo (Threshold = 70), produciendo una mascara binaria de bordes.
- Rama de color: la imagen original de entrada se posteriza por canal RGB a K niveles discretos (K=3 o K=9).
- Fusion: finalmente, se realiza una suma saturada entre la imagen posterizada y la mascara de bordes, recortando los valores a un maximo de 255.

2.1 Formato de Entrada/Salida (PPM P6)
Se selecciono el formato Portable Pixmap binario (P6). A diferencia de formatos comprimidos (PNG o JPEG) que introducen sobrecarga de decodificacion en CPU, el formato P6 almacena un encabezado ASCII seguido de un arreglo lineal contiguo de bytes en memoria:
Tamano de datos = Ancho * Alto * 3 bytes (RGB).
Para una imagen de 5000x5000, cada buffer ocupa exactamente 75 MB.

2.2 Suavizado: Filtro de Media (Box Blur)
Reduce el ruido de alta frecuencia para evitar que el operador diferencial detecte falsos bordes. Para cada pixel (x, y) y radio de mascara R (R=1 para 3x3; R=2 para 5x5), el valor suavizado es el promedio aritmetico de todos los pixeles dentro de la ventana de tamano (2R+1) x (2R+1).
Condicion de frontera: se implemento un borde neutro negro (0, 0, 0) cuando las coordenadas exceden los limites validos de la imagen.

2.3 Resaltado de Bordes (Sobel) y Umbralado
1. Luminancia: se convierte el vecindario suavizado a escala de grises usando la norma ITU-R BT.601 optimizada con aritmetica entera:
Y = (299 * R + 587 * G + 114 * B) / 1000.
2. Gradiente Sobel: se aplican mascaras 3x3 para aproximar la derivada espacial horizontal (Gx) y vertical (Gy):
Gx usa pesos [-1, 0, +1; -2, 0, +2; -1, 0, +1].
Gy usa pesos [-1, -2, -1; 0, 0, 0; +1, +2, +1].
3. Magnitud Euclidea:
Magnitud = raiz_cuadrada(Gx * Gx + Gy * Gy).
4. Binarizacion: mascara de bordes con umbral T = 70. Si la magnitud es mayor o igual a 70, se asigna 255; de lo contrario, se asigna 0.

2.4 Posterizacion de Canales RGB
Discretiza los 256 niveles originales de cada canal en K intervalos regulares:
bucket = parte_entera((valor * K) / 256).
valor_mapeado = parte_entera((bucket * 255) / (K - 1)).
Para K=3, los niveles representativos son 0, 127 y 255. Para K=9, son 0, 31, 63, 95, 127, 159, 191, 223 y 255.

2.5 Suma Saturada
Fusion pixel a pixel con truncamiento superior en 255:
Pixel_salida = minimo(255, Pixel_posterizado + Mascara_borde).
Los contornos detectados se iluminan en blanco, generando el realce visual caracteristico del comic.


3. Estrategias de Paralelizacion e Implementacion

3.1 Version Secuencial (Linea Base)
Implementada en C99 puro. Los cronometros emplean clock_gettime con CLOCK_MONOTONIC y resolucion de nanosegundos, midiendo el computo completo del pipeline en memoria (excluyendo lectura y escritura de archivos en disco).

3.2 Paralelismo de Memoria Compartida (OpenMP)
- Directiva: #pragma omp parallel for schedule(static) sobre el bucle externo de filas.
- Planificacion: se utilizo static debido a que la carga de trabajo por fila es homogenea, eliminando la sobrecarga de despacho dinamico.
- Ausencia de Condiciones de Carrera: cada hilo computa filas disjuntas y escribe en buffers de destino independientes, garantizando seguridad entre hilos sin bloqueos criticos ni operaciones atomicas.
- Medicion de tiempo: omp_get_wtime().

3.3 Paralelismo de Memoria Distribuida (MPI)
- Descomposicion de Dominio 1D: la imagen de H filas se fragmenta horizontalmente entre P procesos.
- Manejo de Celdas Irregulares: dado que H puede no ser multiplo exacto de P, se calcularon vectores de desplazamientos (displs) y cuentas de elementos (counts), distribuyendo la carga con MPI_Scatterv y reuniendola con MPI_Gatherv.
- Doble Intercambio de Halos (Ghost Cells): para evitar dependencias cruzadas y artefactos visuales, el algoritmo ejecuta dos fases de comunicacion de frontera con MPI_Sendrecv:
  Fase 1 (Pre-Media): intercambio de R filas (1 fila para 3x3, 2 filas para 5x5) entre procesos vecinos (rank - 1 y rank + 1).
  Fase 2 (Pre-Sobel): una vez suavizada la porcion local, se intercambia 1 fila de halo sobre la imagen suavizada para que la convolucion de Sobel opere con datos validos en los limites de cada bloque.
Los procesos frontera (rank 0 y rank P-1) reemplazan las comunicaciones externas con MPI_PROC_NULL y rellenan sus halos externos con ceros neutros mediante memset.
- Medicion de tiempo (End-to-End): el cronometro principal inicia con MPI_Barrier y MPI_Wtime antes de MPI_Scatterv y finaliza despues de MPI_Gatherv, midiendo tanto el computo local y los halos como la distribucion y recoleccion de la imagen completa.

3.4 Paradigma Hibrido (MPI + OpenMP)
Se inicializo con MPI_Init_thread en modo MPI_THREAD_FUNNELED, garantizando que unicamente el hilo maestro efectue llamadas a la API de MPI mientras delega el computo intra-proceso a multiples hilos OpenMP.


4. Metodologia Experimental y Entorno de Evaluacion

4.1 Especificaciones de Hardware y Software
- Procesador: Intel Core i7-8700 CPU a 3.20 GHz.
- Cores Fisicos y Logicos: 6 nucleos fisicos y 12 hilos con Hyper-Threading habilitado.
- Jerarquia de Cache: L1d de 32 KB por core, L2 de 256 KB por core (1.5 MB total), L3 de 12 MB SmartCache compartida.
- Sistema Operativo: Windows 10 Pro (x86_64).
- Compilador: GCC 16.2.0 bajo entorno MSYS2 MinGW-w64 con flags -O3 -Wall -fopenmp.
- MPI Runtime y SDK: Microsoft MPI v10.1 (MS-MPI) vinculado con -lmsmpi.

4.2 Metricas de Desempeno
Para P unidades de procesamiento (hilos o procesos):
- Speedup (S): S = T_secuencial / T_paralelo.
- Eficiencia (E): E = (S / P) * 100% = (T_secuencial / (P * T_paralelo)) * 100%.
- Ley de Amdahl: establece el limite teorico maximo de aceleracion en funcion de la fraccion secuencial y de comunicacion no paralelizable.


5. Resultados Experimentales y Tablas Comparativas (End-to-End)

5.1 Resolucion 800 x 800 Pixeles (Filtro 5x5, K=9)
- Secuencial (1 Core): 0.031403 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (2 Hilos): 0.020667 s | Speedup: 1.52x | Eficiencia: 76.0%
- OpenMP (4 Hilos): 0.011000 s | Speedup: 2.85x | Eficiencia: 71.4%
- OpenMP (8 Hilos): 0.008667 s | Speedup: 3.62x | Eficiencia: 45.3%
- OpenMP (12 Hilos): 0.007333 s | Speedup: 4.28x | Eficiencia: 35.7%
- MPI (2 Procesos): 0.017701 s | Speedup: 1.77x | Eficiencia: 88.7%
- MPI (4 Procesos): 0.015368 s | Speedup: 2.04x | Eficiencia: 51.1%
- MPI (8 Procesos): 0.009862 s | Speedup: 3.18x | Eficiencia: 39.8%
- MPI (12 Procesos): 0.008055 s | Speedup: 3.90x | Eficiencia: 32.5%
- Hibrido (2 Proc x 2 Hilos = 4 cores): 0.014120 s | Speedup: 2.22x | Eficiencia: 55.6%

5.2 Resolucion 2000 x 2000 Pixeles (Filtro 5x5, K=9)
- Secuencial (1 Core): 0.198249 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (2 Hilos): 0.132333 s | Speedup: 1.50x | Eficiencia: 75.0%
- OpenMP (4 Hilos): 0.072667 s | Speedup: 2.73x | Eficiencia: 68.2%
- OpenMP (8 Hilos): 0.053333 s | Speedup: 3.72x | Eficiencia: 46.5%
- OpenMP (12 Hilos): 0.044333 s | Speedup: 4.47x | Eficiencia: 37.3%
- MPI (2 Procesos): 0.106540 s | Speedup: 1.86x | Eficiencia: 93.0%
- MPI (4 Procesos): 0.063985 s | Speedup: 3.10x | Eficiencia: 77.5%
- MPI (8 Procesos): 0.051820 s | Speedup: 3.83x | Eficiencia: 47.8%
- MPI (12 Procesos): 0.048924 s | Speedup: 4.05x | Eficiencia: 33.8%
- Hibrido (2 Proc x 2 Hilos = 4 cores): 0.074810 s | Speedup: 2.65x | Eficiencia: 66.3%

5.3 Resolucion 5000 x 5000 Pixeles (Filtro 5x5, K=3 y K=9)

Con K=3:
- Secuencial (1 Core): 1.246977 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (2 Hilos): 0.812000 s | Speedup: 1.54x | Eficiencia: 76.8%
- OpenMP (4 Hilos): 0.455000 s | Speedup: 2.74x | Eficiencia: 68.5%
- OpenMP (8 Hilos): 0.311000 s | Speedup: 4.01x | Eficiencia: 50.1%
- OpenMP (12 Hilos): 0.258000 s | Speedup: 4.83x | Eficiencia: 40.3%
- MPI (2 Procesos): 0.671240 s | Speedup: 1.86x | Eficiencia: 92.9%
- MPI (4 Procesos): 0.419820 s | Speedup: 2.97x | Eficiencia: 74.3%
- MPI (8 Procesos): 0.358410 s | Speedup: 3.48x | Eficiencia: 43.5%
- MPI (12 Procesos): 0.261890 s | Speedup: 4.76x | Eficiencia: 39.7%
- Hibrido (2 Proc x 2 Hilos = 4 cores): 0.539120 s | Speedup: 2.31x | Eficiencia: 57.8%

Con K=9:
- Secuencial (1 Core): 1.254905 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (2 Hilos): 0.802000 s | Speedup: 1.56x | Eficiencia: 78.2%
- OpenMP (4 Hilos): 0.435000 s | Speedup: 2.88x | Eficiencia: 72.1%
- OpenMP (8 Hilos): 0.307000 s | Speedup: 4.09x | Eficiencia: 51.1%
- OpenMP (12 Hilos): 0.254000 s | Speedup: 4.94x | Eficiencia: 41.2%
- MPI (2 Procesos): 0.674190 s | Speedup: 1.86x | Eficiencia: 93.1%
- MPI (4 Procesos): 0.429736 s | Speedup: 2.92x | Eficiencia: 73.0%
- MPI (8 Procesos): 0.366257 s | Speedup: 3.43x | Eficiencia: 42.8%
- MPI (12 Procesos): 0.265062 s | Speedup: 4.73x | Eficiencia: 39.4%
- Hibrido (2 Proc x 2 Hilos = 4 cores): 0.548343 s | Speedup: 2.29x | Eficiencia: 57.2%
- Hibrido (2 Proc x 4 Hilos = 8 cores): 0.342009 s | Speedup: 3.67x | Eficiencia: 45.9%
- Hibrido (2 Proc x 6 Hilos = 12 cores): 0.329129 s | Speedup: 3.81x | Eficiencia: 31.8%

5.4 Desglose de Comunicacion (Scatterv + Gatherv) vs Computo en MPI (5000x5000, 5x5, K=9)
- 4 Procesos:
  Computo + Halos: 0.381784 s | Scatterv: 0.020740 s | Gatherv: 0.027110 s | Total End-to-End: 0.429736 s
  Speedup Computo Puro: 3.29x (Eficiencia 82.2%) | Speedup End-to-End: 2.92x (Eficiencia 73.0%)
- 8 Procesos:
  Computo + Halos: 0.298699 s | Scatterv: 0.037454 s | Gatherv: 0.029795 s | Total End-to-End: 0.366257 s
  Speedup Computo Puro: 4.20x (Eficiencia 52.5%) | Speedup End-to-End: 3.43x (Eficiencia 42.8%)
- 12 Procesos:
  Computo + Halos: 0.208350 s | Scatterv: 0.029568 s | Gatherv: 0.026937 s | Total End-to-End: 0.265062 s
  Speedup Computo Puro: 6.02x (Eficiencia 50.2%) | Speedup End-to-End: 4.73x (Eficiencia 39.4%)


6. Discusion Tecnica y Analisis Critico

6.1 Metodologia End-to-End en MPI y Efectividad Real
Para que la comparacion frente al algoritmo secuencial sea rigurosa y justa, el cronometro de MPI e Hibrido debe incluir la distribucion de la imagen (MPI_Scatterv) y la recoleccion final (MPI_Gatherv). Al incorporar estas transferencias (que representan unos 48 a 67 ms para mover 75 MB de entrada y 75 MB de salida), las eficiencias se mantienen estrictamente por debajo del 100%, respetando la Ley de Amdahl.

6.2 Analisis de Escalabilidad en 4, 8 y 12 Procesos/Hilos
El procesador utilizado cuenta con 6 nucleos fisicos y 12 hilos logicos (Hyper-Threading):
- Con 4 procesos/hilos: cada worker se ejecuta en un nucleo fisico dedicado, alcanzando una eficiencia alta del 73.0% en MPI y 72.1% en OpenMP (Speedup ~2.9x).
- Con 8 procesos/hilos: se supera la cantidad de nucleos fisicos (6), entrando en accion el Hyper-Threading. El tiempo total baja de 0.43 s a 0.36 s en MPI (Speedup 3.43x) y a 0.31 s en OpenMP (Speedup 4.09x), mientras que la eficiencia desciende al ~43%-51% debido a que los hilos logicos comparten las unidades de ejecucion y el bus de memoria.
- Con 12 procesos/hilos: ocupando la totalidad de los hilos logicos del procesador, se alcanza el menor tiempo de ejecucion (0.265 s en MPI con Speedup 4.73x, y 0.254 s en OpenMP con Speedup 4.94x), con una eficiencia del ~40% limitada por el ancho de banda de la memoria RAM principal.

6.3 Intensidad Aritmetica: 3x3 frente a 5x5
En 5000x5000 con K=9:
- Filtro 3x3: OpenMP (4 hilos) alcanza Speedup 2.55x (Eficiencia 63.7%) y MPI (4 procesos) alcanza Speedup 2.77x (Eficiencia 69.1%).
- Filtro 5x5: OpenMP (4 hilos) alcanza Speedup 2.88x (Eficiencia 72.1%) y MPI (4 procesos) alcanza Speedup 2.92x (Eficiencia 73.0%).
El filtro 5x5 realiza 25 accesos y sumas por pixel contra 9 del filtro 3x3. Esta mayor carga computacional por byte transferido amortigua mejor el costo de sincronizacion y comunicacion.

6.4 Verificacion Criptografica Bit a Bit
Se certifico mediante el script verify_hashes.bat que las imagenes de salida de todas las versiones (Secuencial, OpenMP, MPI e Hibrido) producen exactamente el mismo hash SHA-256:
DD70A0A2133950635ECD8AE619FB45472D56588E5AA3964DE98F38B90346FBCE.


7. Conclusiones

1. Se implementaron y validaron las cuatro versiones solicitadas, corrigiendo el cronometraje en MPI e Hibrido para reflejar el tiempo End-to-End completo (incluyendo Scatterv y Gatherv).
2. Las pruebas con 4, 8 y 12 procesos demuestran un escalamiento consistente con la arquitectura de 6 nucleos fisicos y 12 hilos logicos del procesador, alcanzando un Speedup maximo de 4.73x en MPI y 4.94x en OpenMP.
3. El doble intercambio de halos con MPI_Sendrecv garantizo resultados identicos bit a bit respecto de la version secuencial sin artefactos en los bordes de particion.


8. Bibliografia y Referencias

- Pacheco, P. (2011). An Introduction to Parallel Programming. Morgan Kaufmann.
- Gropp, W., Lusk, E., & Skjellum, A. (1999). Using MPI: Portable Parallel Programming with the Message-Passing Interface. MIT Press.
- OpenMP Architecture Review Board. OpenMP Application Programming Interface Specification Version 5.2.
- Gonzalez, R. C., & Woods, R. E. (2018). Digital Image Processing (4th Edition). Pearson.
