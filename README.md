# Fanner 3000 (VST3)

Efecto de "hablarle a un ventilador": un tremolo rapido y filoso que corta la
senal como las aspas. Hecho con JUCE.

## Perillas

- **Speed**: velocidad de las aspas en cortes por segundo (0.5 a 80 Hz). Para el
  efecto clasico de ventilador prueba entre 15 y 40 Hz.
- **Depth**: cuanto se corta la senal (1 = silencio total entre aspas).
- **Shape**: de onda suave (tremolo) a corte seco (aspa).
- **Stereo**: desfasa izquierda y derecha, el sonido "gira".
- **Wobble**: irregularidad en la velocidad, como un ventilador real.
- **Mix** y **Output**: mezcla y volumen final.

## Compilar en Windows

Necesitas Visual Studio 2022 (carga "Desarrollo para el escritorio con C++"),
CMake (con "Add to PATH") y Git. Luego doble clic a `build.bat`.

El plugin queda en `%LOCALAPPDATA%\Programs\Common\VST3`. En FL Studio:
Options > Manage plugins > Find installed plugins.

## Compilar sin instalar nada

Sube esta carpeta a un repo de GitHub. En la pestana Actions, descarga el
resultado en "Artifacts".
