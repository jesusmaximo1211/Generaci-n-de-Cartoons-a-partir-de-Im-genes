# Informe Tecnico y Analisis Experimental: Pipeline de Generacion de Cartoons RGB en HPC

Catedra: Computacion Avanzada (4to Ano)
Autor: Jesus Maximo Serra
Fecha: Noviembre 2026
Repositorio: https://github.com/jesusmaximo1211/Generaci-n-de-Cartoons-a-partir-de-Im-genes


1. Resumen Ejecutivo

Este trabajo documenta el diseno, implementacion, optimizacion y analisis experimental del Practico Final Integrador de Computacion Avanzada. El objetivo central consiste en transformar imagenes digitales a color (formato binario PPM P6) hacia una estetica visual tipo comic o cartoon, explotando diferentes paradigmas de Computacion de Alto Rendimiento (HPC):

1. Secuencial puro en C (linea base).
2. Memoria Compartida con OpenMP (multithreading a nivel de bucles).
3. Memoria Distribuida con MPI (descomposicion de dominio 1D con doble intercambio de halos y manejo de restos con Scatterv y Gatherv).
4. Paradigma Hibrido (MPI + OpenMP) con soporte MPI_THREAD_FUNNELED.

Las pruebas experimentales se realizaron sobre resoluciones de 800x800, 2000x2000 y 5000x5000 pixeles, evaluando mascaras de media de 3x3 y 5x5, posterizaciones de K=3 y K=9 niveles, y escalando hasta 12 hilos en un procesador Intel Core i7-8700 de 6 nucleos fisicos. Se valido la equivalencia estricta bit a bit entre todos los paradigmas mediante sumas de verificacion criptografica SHA-256.


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
Implementada en C99 puro. Los cronometros emplean clock_gettime con CLOCK_MONOTONIC y resolucion de nanosegundos, midiendo estrictamente el computo del pipeline (excluyendo la lectura de disco y la escritura final del archivo PPM).

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

3.4 Paradigma Hibrido (MPI + OpenMP)
Se inicializo con MPI_Init_thread en modo MPI_THREAD_FUNNELED, garantizando que unicamente el hilo maestro efectue llamadas a la API de MPI mientras delega el computo intra-proceso a multiples hilos OpenMP.
Reduce el numero de mensajes en la red y el volumen de halos redundantes, permitiendo que hilos locales compartan datos en la memoria cache L2/L3 de cada procesador.


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
- Ley de Amdahl: establece el limite teorico maximo de aceleracion en funcion de la fraccion secuencial no paralelizable del algoritmo.


5. Resultados Experimentales y Tablas Comparativas

Las mediciones corresponden a ejecuciones directas con imagenes sinteticas deterministicas generadas por el algoritmo.

5.1 Resolucion 800 x 800 Pixeles

Filtro 5x5, K=3:
- Secuencial (1 Core): 0.031273 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (4 Hilos): 0.012000 s | Speedup: 2.61x | Eficiencia: 65.2%
- MPI (2 Procesos): 0.016611 s | Speedup: 1.88x | Eficiencia: 94.0%
- MPI (4 Procesos): 0.012482 s | Speedup: 2.51x | Eficiencia: 62.7%
- Hibrido (2 Proc x 2 Hilos): 0.010277 s | Speedup: 3.04x | Eficiencia: 76.0%

Filtro 5x5, K=9:
- Secuencial (1 Core): 0.031403 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (4 Hilos): 0.011000 s | Speedup: 2.85x | Eficiencia: 71.3%
- MPI (1 Proceso): 0.037038 s | Speedup: 0.85x | Eficiencia: 85.0%
- MPI (2 Procesos): 0.016880 s | Speedup: 1.86x | Eficiencia: 93.0%
- MPI (4 Procesos): 0.014456 s | Speedup: 2.17x | Eficiencia: 54.3%
- Hibrido (2 Proc x 2 Hilos): 0.015170 s | Speedup: 2.07x | Eficiencia: 51.8%


5.2 Resolucion 2000 x 2000 Pixeles

Filtro 5x5, K=3:
- Secuencial (1 Core): 0.195083 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (4 Hilos): 0.081000 s | Speedup: 2.41x | Eficiencia: 60.2%
- MPI (2 Procesos): 0.102127 s | Speedup: 1.91x | Eficiencia: 95.5%
- MPI (4 Procesos): 0.060771 s | Speedup: 3.21x | Eficiencia: 80.3%
- Hibrido (2 Proc x 2 Hilos): 0.075339 s | Speedup: 2.59x | Eficiencia: 64.7%

Filtro 5x5, K=9:
- Secuencial (1 Core): 0.198249 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (2 Hilos): 0.132333 s | Speedup: 1.50x | Eficiencia: 75.0%
- OpenMP (4 Hilos): 0.072667 s | Speedup: 2.73x | Eficiencia: 68.2%
- MPI (2 Procesos): 0.098412 s | Speedup: 2.01x | Eficiencia: 100.7%
- MPI (4 Procesos): 0.058310 s | Speedup: 3.40x | Eficiencia: 85.0%
- Hibrido (2 Proc x 2 Hilos): 0.069415 s | Speedup: 2.86x | Eficiencia: 71.4%


5.3 Resolucion 5000 x 5000 Pixeles

Filtro 5x5, K=3:
- Secuencial (1 Core): 1.246977 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (2 Hilos): 0.812000 s | Speedup: 1.54x | Eficiencia: 76.8%
- OpenMP (4 Hilos): 0.455000 s | Speedup: 2.74x | Eficiencia: 68.5%
- MPI (1 Proceso): 1.230111 s | Speedup: 1.01x | Eficiencia: 101.4%
- MPI (2 Procesos): 0.628614 s | Speedup: 1.98x | Eficiencia: 99.2%
- MPI (4 Procesos): 0.345338 s | Speedup: 3.61x | Eficiencia: 90.3%
- Hibrido (2 Proc x 2 Hilos): 0.371329 s | Speedup: 3.36x | Eficiencia: 83.9%

Filtro 5x5, K=9:
- Secuencial (1 Core): 1.261651 s | Speedup: 1.00x | Eficiencia: 100.0%
- OpenMP (2 Hilos): 0.802000 s | Speedup: 1.57x | Eficiencia: 78.6%
- OpenMP (4 Hilos): 0.435000 s | Speedup: 2.90x | Eficiencia: 72.5%
- MPI (1 Proceso): 1.261469 s | Speedup: 1.00x | Eficiencia: 100.0%
- MPI (2 Procesos): 0.629406 s | Speedup: 2.00x | Eficiencia: 100.0%
- MPI (4 Procesos): 0.360692 s | Speedup: 3.50x | Eficiencia: 87.5%
- Hibrido (2 Proc x 2 Hilos): 0.408332 s | Speedup: 3.09x | Eficiencia: 77.2%


5.4 Medicion Real del Impacto de Scatterv y Gatherv en MPI
Para evaluar el costo exacto de la distribucion y recoleccion de datos frente al tiempo de computo puro sobre 5000x5000 (Filtro 5x5, K=9, tamano total de imagen = 75 MB):
- MPI con 1 Proceso:
  Computo local + Halos: 1.261469 s
  Comunicacion: Scatterv = 0.018433 s | Gatherv = 0.018291 s
  Tiempo Total End-to-End: 1.298227 s (Sobrecarga de Scatter/Gather: 2.9%)
- MPI con 2 Procesos:
  Computo local + Halos: 0.629406 s (Speedup computacional: 2.00x, Eficiencia: 100.0%)
  Comunicacion: Scatterv = 0.024881 s | Gatherv = 0.019676 s
  Tiempo Total End-to-End: 0.674190 s (Speedup real end-to-end: 1.87x, Eficiencia: 93.5%)
- MPI con 4 Procesos:
  Computo local + Halos: 0.360692 s (Speedup computacional: 3.50x, Eficiencia: 87.5%)
  Comunicacion: Scatterv = 0.022045 s | Gatherv = 0.021976 s
  Tiempo Total End-to-End: 0.404788 s (Speedup real end-to-end: 3.11x, Eficiencia: 77.8%)


5.5 Escalabilidad Extensiva OpenMP (Cores Fisicos y Hyper-Threading)
Evaluacion de la saturacion de nucleos en resolucion 5000x5000 (Filtro 5x5, K=9, Tiempo secuencial base = 1.2616 s):
- 1 Hilo (Secuencial base): 1.2616 s | Speedup: 1.00x | Eficiencia: 100.0%
- 2 Hilos (Cores fisicos): 0.8020 s | Speedup: 1.57x | Eficiencia: 78.6%
- 4 Hilos (Cores fisicos): 0.4350 s | Speedup: 2.90x | Eficiencia: 72.5%
- 6 Hilos (Todos los cores fisicos): 0.3610 s | Speedup: 3.49x | Eficiencia: 58.2%
- 8 Hilos (Cores logicos con HT): 0.3070 s | Speedup: 4.11x | Eficiencia: 51.4%
- 12 Hilos (Maximo logico con HT): 0.2540 s | Speedup: 4.97x | Eficiencia: 41.4%


6. Discusion Tecnica y Analisis Critico

6.1 Comparativa K=3 frente a K=9
Al contrastar los niveles de posterizacion para una misma resolucion y filtro:
En 5000x5000 con Filtro 5x5:
- Secuencial con K=3 toma 1.2469 s, mientras que con K=9 toma 1.2616 s (variacion menor al 1.2%).
- OpenMP con 4 hilos toma 0.4550 s con K=3 y 0.4350 s con K=9.
- MPI con 4 procesos toma 0.3453 s con K=3 y 0.3606 s con K=9.

Fundamento: el costo computacional de la posterizacion por pixel consiste en una multiplicacion entera, un corrimiento o division por 256, y un mapeo aritmético directo. Este costo es independiente del valor de K (K=3 y K=9 ejecutan exactamente la misma cantidad de operaciones de instruccion por byte). La leve variacion observada responde al ruido estadistico y a fluctuaciones termicas del procesador durante la corrida.

6.2 Esclarecimiento del Comportamiento en MPI (Salto de 1 a 2 Procesos)
En corridas iniciales de benchmarking se habian registrado cifras aparentemente anomalas (Speedups de 16x y 25x). Al realizar una auditoria estricta del entorno bajo Windows y Microsoft MPI, se detecto que el ejecutable mpiexec.exe descarta los argumentos vacios ("") de la linea de comandos, provocando que los parametros de dimension se desplazaran y se evaluara una imagen reducida.
Una vez subsanado este comportamiento y medidas las dimensiones reales de 5000x5000:
- Secuencial: 1.2616 s.
- MPI con 1 Proceso: 1.2614 s (Speedup 1.00x).
- MPI con 2 Procesos: 0.6294 s (Speedup 2.00x, Eficiencia 100.0%).
- MPI con 4 Procesos: 0.3606 s (Speedup 3.50x, Eficiencia 87.5%).

Los resultados demuestran un escalamiento casi lineal perfecto (Speedup de 2.00x con 2 procesos), coherente con la fisica del hardware y descartando cualquier hipotesis de superaceleracion magica. Cuando se incorpora el tiempo de Scatterv y Gatherv, el Speedup real end-to-end se posiciona en 1.87x para 2 procesos y 3.11x para 4 procesos, reflejando fielmente el costo de transferir 75 MB entre procesos en un unico nodo.

6.3 Cuello de Botella de Memoria Compartida en OpenMP
A medida que se incrementa el numero de hilos de 2 a 12, el tiempo continua reduciendose (de 0.802 s a 0.254 s), pero la eficiencia decae del 78.6% al 41.4%.
Este comportamiento responde a dos causas principales:
1. Saturacion del Bus de Memoria: todos los hilos compiten por el mismo ancho de banda del canal dual DDR4 hacia la memoria RAM principal.
2. Hyper-Threading: los hilos logicos 7 al 12 comparten las unidades de ejecucion de enteros y punto flotante con los nucleos fisicos primarios, produciendo mejoras asintoticas sublineales.

6.4 Verificacion Criptografica Bit a Bit
Para certificar que el paralelismo y el intercambio de halos no alteraron las fronteras ni introdujeron artefactos numericos, se calculo el hash criptografico SHA-256 de las imagenes generadas bajo todas las variantes.

Procedimiento y Comando Ejecutado:
Se utilizo la imagen sintetica patron de 800x800 con Filtro 5x5, K=9 y Umbral 70 generada por el script verify_hashes.bat en la raiz del repositorio:
Comando:
powershell -Command "Get-FileHash test_*.ppm -Algorithm SHA256"

Resultados obtenidos:
- test_seq.ppm:  DD70A0A2133950635ECD8AE619FB45472D56588E5AA3964DE98F38B90346FBCE
- test_omp.ppm:  DD70A0A2133950635ECD8AE619FB45472D56588E5AA3964DE98F38B90346FBCE
- test_mpi1.ppm: DD70A0A2133950635ECD8AE619FB45472D56588E5AA3964DE98F38B90346FBCE
- test_mpi2.ppm: DD70A0A2133950635ECD8AE619FB45472D56588E5AA3964DE98F38B90346FBCE
- test_mpi4.ppm: DD70A0A2133950635ECD8AE619FB45472D56588E5AA3964DE98F38B90346FBCE
- test_hyb.ppm:  DD70A0A2133950635ECD8AE619FB45472D56588E5AA3964DE98F38B90346FBCE

La coincidencia absoluta de los seis valores hash confirma que la descomposicion por bloques de filas y el esquema de doble halo de 1 y 2 celdas replican con exactitud milimetrica el resultado secuencial original.


7. Conclusiones

1. Se implementaron con exito los cuatro paradigmas de computo solicitados por la catedra, garantizando consistencia matematica y portabilidad completa en entornos Windows y Linux.
2. La inclusion de una segunda fase de intercambio de halos para el operador Sobel demostro ser indispensable para erradicar discontinuidades espaciales en los limites de los procesos MPI.
3. El analisis detallado de la comunicacion en MPI demostro que la transferencia de datos (Scatterv + Gatherv) para 75 MB insume aproximadamente 40 a 45 milisegundos en memoria local, permitiendo calcular con rigor tanto el Speedup computacional puro como el Speedup global de la aplicacion.
4. Tanto K=3 como K=9 presentan tiempos de computo equivalentes debido a que la formula de cuantizacion uniforme insume identica cantidad de ciclos de instruccion.


8. Bibliografia y Referencias

- Pacheco, P. (2011). An Introduction to Parallel Programming. Morgan Kaufmann.
- Gropp, W., Lusk, E., & Skjellum, A. (1999). Using MPI: Portable Parallel Programming with the Message-Passing Interface. MIT Press.
- OpenMP Architecture Review Board. OpenMP Application Programming Interface Specification Version 5.2.
- Gonzalez, R. C., & Woods, R. E. (2018). Digital Image Processing (4th Edition). Pearson.
- ITU-R Recommendation BT.601: Studio encoding parameters of digital television for standard 4:3 and wide-screen 16:9 aspect ratios.
