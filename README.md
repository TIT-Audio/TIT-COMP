# TIT COMP v0.1

Compresor gratuito con sidechain interno y externo, EQ de 10 bandas en el sidechain, oversampling hasta x8 y modos de carácter propio. Formatos: **VST3 (Windows y macOS)** y **AU (macOS)**. Sin AAX, sin Standalone.

## Funciones
- Threshold, Ratio, Knee, Attack, Release, Hold, Lookahead (hasta 20 ms), Range, Stereo Link, Mix y Output.
- Sidechain interno o externo (una sola entrada externa por instancia) con **EQ de 10 bandas** (Bell, shelves, cortes y tilt). Un clic en la gráfica crea una banda; doble clic sobre una banda la enciende o apaga; clic derecho la borra.
- **SC Listen** para oír el sidechain ya filtrado.
- Oversampling Off / 2x / 4x / 8x. El plugin informa de su latencia al DAW.
- Medidor de reducción de ganancia (GR).
- Modos: Clean, **VCA TIT**, **FET TIT**, **FET TIT B**, **OP TIT**, **FAIR TIT**. Cada modo cambia el detector, la rodilla mínima y la balística (tiempos de ataque y liberación), con valores medidos sobre compresores reales (solo el comportamiento dinámico). FAIR TIT es un único modo: ataque fijo ~1.5 ms, rodilla ancha y liberación de una o dos etapas según el mando Release. Al elegir uno se cargan sus valores de partida, que luego se pueden mover.

## Compilar
Sube todos los archivos a la raíz del repositorio y `build.yml` a `.github/workflows/build.yml`. Añade el archivo `LICENSE` (AGPLv3). El workflow genera el artefacto `TITCOMP-all`.
macOS sin firmar: `xattr -cr "TIT COMP.vst3"` (o .component).

## Licencia
TIT COMP es gratuito y se distribuye bajo la licencia GNU AGPLv3 (ver el archivo LICENSE).
Está construido con JUCE 8. El código fuente está disponible en este repositorio.

## Límites por modo (v0.4)
Cada modo respeta al compresor original medido: solo se pueden mover sus mandos y con sus valores.
- VCA TIT: ataque 0.1/0.3/1/3/10/30 ms, release 0.1/0.3/0.6/1.2 s + botón Auto Release, ratio 2/4/10, rodilla fija.
- FET TIT / FET TIT B: ataque 0.02–0.8 ms, release 50–1100 ms, ratio 4/8/12/20, rodilla fija.
- OP TIT: sin ataque, release ni ratio.
- FAIR TIT: ataque fijo; release = 6 posiciones de Time Constant; ratio y rodilla fijos.

## Controles por modo (v0.5)
Cada modo muestra solo los mandos de su compresor original:
- VCA TIT: Threshold, Ratio, Attack, Release (+Auto), Make-up.
- FET TIT / FET TIT B: Input, Output, Attack 1–7, Release 1–7, Ratio 4/8/12/20.
- OP TIT: Peak Reduction y Gain.
- FAIR TIT: Input Gain, Threshold, Time Constant 1–6, Output, Mix.
- Clean: todos los mandos libres (con Auto Release).
Medidor horizontal IN / GR / OUT arriba y EQ del sidechain debajo.


## Nota

TIT COMP emula el comportamiento dinamico (curva, ataque, recuperacion) de cada compresor, medido con compresores de referencia. Es compresion digital limpia: no anade saturacion ni color.
