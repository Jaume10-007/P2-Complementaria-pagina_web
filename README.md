# Medidor de Frecuencia con ESP32

### Práctica complementaria – Procesadores Digitales

## Descripción del proyecto

Este proyecto consiste en la implementación de un **frecuencímetro digital utilizando un ESP32**. El sistema es capaz de medir la frecuencia de una señal externa detectando **interrupciones en un pin GPIO** y midiendo el **tiempo entre flancos consecutivos** mediante un **temporizador hardware interno del microcontrolador**.

Los periodos medidos se almacenan en una **cola circular**, que posteriormente es procesada por el programa principal para calcular los valores de **frecuencia mínima (Fmin), frecuencia media (Fmed) y frecuencia máxima (Fmax)**.

Los resultados se muestran en **tiempo real en una página web generada por el propio ESP32**, que se actualiza automáticamente para reflejar las nuevas mediciones.

Este sistema permite comprobar el funcionamiento tanto **de forma manual** (conectando el pin a GND) como mediante **un generador de funciones**, observando simultáneamente la señal en un **osciloscopio**.

---

# Objetivos del proyecto

Los objetivos principales de esta práctica complementaria son:

* Comprender el funcionamiento de las **interrupciones en microcontroladores**.
* Utilizar **temporizadores hardware del ESP32** como referencia temporal.
* Implementar una **cola circular (buffer FIFO)** para almacenar datos en tiempo real.
* Calcular estadísticas de frecuencia:

  * **Fmin**
  * **Fmed**
  * **Fmax**
* Crear una **interfaz web servida por el ESP32**.
* Mostrar resultados en **tiempo real a través de WiFi**.

---

# Funcionamiento del sistema

El funcionamiento del frecuencímetro se basa en los siguientes pasos:

1. Una señal externa llega al pin configurado del ESP32.
2. Cada flanco de la señal genera una **interrupción hardware**.
3. La rutina de interrupción (**ISR**) lee el valor del **timer hardware**.
4. Se calcula el **periodo entre interrupciones consecutivas**.
5. Este periodo se guarda en una **cola circular**.
6. El programa principal vacía la cola y convierte cada periodo en frecuencia.
7. Se actualizan los valores de **Fmin, Fmed y Fmax**.
8. Una página web servida por el ESP32 muestra los resultados y se refresca automáticamente.

---

# Arquitectura del sistema

El sistema está dividido en varios bloques funcionales:

### 1. Interrupción GPIO

Cada flanco de la señal externa genera una interrupción.

```text
Señal externa → GPIO → Interrupción
```

La ISR registra el instante exacto del evento.

---

### 2. Timer hardware

El ESP32 dispone de temporizadores internos de alta precisión.
En este proyecto se utiliza un **timer con resolución de microsegundos**.

```text
80 MHz / prescaler 80 = 1 MHz
1 tick = 1 microsegundo
```

Esto permite medir con precisión el tiempo entre interrupciones.

---

### 3. Cola circular

Los periodos medidos se almacenan en una **cola circular**.

Ventajas:

* evita pérdida de datos cuando las interrupciones llegan rápido
* desacopla la ISR del procesamiento principal
* mejora la estabilidad del sistema

La ISR **solo introduce datos en la cola**, mientras que el `loop()` **los procesa**.

---

### 4. Cálculo de frecuencias

Cada periodo `dt` almacenado en la cola se convierte en frecuencia mediante:

```
f = 1 / T
```

Como el periodo está en microsegundos:

```
f = 1000000 / dt
```

Con estos valores se calculan:

* **Fmin** → frecuencia mínima detectada
* **Fmax** → frecuencia máxima detectada
* **Fmed** → frecuencia media de todas las muestras

---

### 5. Servidor web

El ESP32 crea un **punto de acceso WiFi** y ejecuta un servidor web interno.

Al conectarse a la red WiFi del ESP32, el usuario puede acceder a la página:

```
http://192.168.4.1
```

La página muestra:

* Fmin
* Fmed
* Fmax
* número de muestras procesadas
* muestras perdidas

Los valores se actualizan automáticamente cada pocos cientos de milisegundos.

---

# Hardware utilizado

Para este proyecto se utiliza el siguiente hardware:

* ESP32
* generador de funciones (opcional)
* osciloscopio (para verificar la señal)
* cables de conexión
* ordenador con PlatformIO

---

# Conexiones

La señal a medir se conecta al pin definido en el programa:

```
GPIO 18
```

Conexión básica:

```
Generador de señal → GPIO18
GND generador → GND ESP32
```

⚠ Importante:
La señal de entrada debe ser **compatible con 3.3 V**.
No se deben aplicar **5 V directamente al ESP32**.

---

# Métodos de prueba

## Prueba manual

Para comprobar el funcionamiento básico se puede utilizar un cable:

1. conectar un cable al pin GPIO18
2. tocar momentáneamente el pin **GND**

Cada contacto genera una interrupción.

Las frecuencias medidas serán bajas e irregulares, ya que dependen del tiempo entre contactos.

---

## Prueba con generador de funciones

Una prueba más precisa consiste en utilizar un generador de señal cuadrada.

Frecuencias recomendadas para probar:

* 10 Hz
* 50 Hz
* 100 Hz
* 500 Hz
* 1 kHz

El valor de **Fmed** debería aproximarse al valor configurado en el generador.

---

## Verificación con osciloscopio

Para verificar el funcionamiento del sistema:

1. conectar el osciloscopio en paralelo con la señal
2. observar la frecuencia real de la señal
3. comparar con el valor mostrado en la página web

---

# Interfaz web

La página web generada por el ESP32 muestra:

* frecuencia mínima
* frecuencia media
* frecuencia máxima
* número de muestras procesadas
* muestras perdidas

La página se actualiza automáticamente mediante **peticiones periódicas al servidor web del ESP32**.

También incluye un botón para **reiniciar las estadísticas** sin necesidad de reiniciar el dispositivo.

---

# Entorno de desarrollo

El proyecto se ha desarrollado utilizando:

* **Visual Studio Code**
* **PlatformIO**
* **Framework Arduino**
* **ESP32**

---

# Conclusión

Este proyecto demuestra cómo implementar un **frecuencímetro digital utilizando interrupciones y temporizadores hardware en un ESP32**.

El uso de una **cola circular** permite manejar correctamente eventos de alta frecuencia sin bloquear el programa principal, mientras que la **interfaz web** proporciona una forma cómoda de visualizar los resultados en tiempo real.

Este tipo de arquitectura es común en sistemas embebidos donde se requiere medir eventos con precisión temporal y procesarlos de forma eficiente.
