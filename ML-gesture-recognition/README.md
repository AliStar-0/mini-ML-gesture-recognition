# ML Gesture Recognition

An ESP32 that recognizes hand gestures with a neural network running on the board itself. Hold a button, draw a shape in the air, and the ESP32 tells you which gesture it was. No cloud, no ML library: the inference code is about 40 lines of C++.

## How it works

1. An MPU6050 measures acceleration and rotation 100 times per second while the button is held.
2. The recording is resized to 50 time steps, so fast and slow versions of a gesture look alike. That gives 50 x 6 = 300 numbers.
3. A small neural network (300 → 32 → 16 → one output per gesture) picks the gesture.

The network is trained on your PC with scikit-learn and exported as a C header (`include/model.h`), which is compiled into the firmware.

## Parts

| Part | Notes |
| --- | --- |
| ESP32 dev board | Any classic ESP32 DevKit (the code targets `esp32dev`) |
| MPU6050 module | Usually sold as GY-521 |
| 4 jumper wires | |

The button is the BOOT button already on the dev board, and the LED is the on-board one.

## Wiring

| MPU6050 | ESP32 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

Leave XDA, XCL, AD0 and INT unconnected.

Optional: for a handheld build, wire a push button between GPIO 4 and GND and set `PIN_BUTTON = 4` in `include/config.h`.

## Build it

### The double-click way (Windows)

No terminal needed. The scripts use the Python that ships with PlatformIO.

1. Double-click `setup.bat` once. It creates `.venv` and installs the Python packages.
2. In VS Code, pick **env:collect** in the PlatformIO bar and click Upload.
3. Double-click `record.bat`, type a gesture name, and record. Repeat for each gesture.
4. Double-click `train.bat`.
5. In VS Code, pick **env:recognize**, click Upload, and open the Serial Monitor.

### The command-line way

You need [PlatformIO](https://platformio.org/) in VS Code and Python 3.

```
pip install -r tools/requirements.txt
```

### 1. Record gestures

Upload the `collect` firmware (it is the default environment):

```
pio run -e collect -t upload
```

Close the serial monitor, then record each gesture. About 30 recordings per gesture is a good start:

```
python tools/collect.py --label circle
python tools/collect.py --label swipe
python tools/collect.py --label flick
```

For each recording: hold the button, draw the gesture, release. The LED is on while recording. Each gesture is saved as one row in `data/<label>.csv`.

### 2. Train

```
python tools/train.py
```

This prints the accuracy on recordings the model has never seen, saves `docs/confusion_matrix.png`, and writes `include/model.h`.

### 3. Recognize

```
pio run -e recognize -t upload
pio device monitor
```

Draw a gesture and the monitor shows something like:

```
Gesture: circle (97%)  inference <time> us
```

To make gestures do something, edit `onGesture()` in `src/main_recognize.cpp`.

## Tips for good accuracy

- Hold the board the same way every time. The model sees gravity, so a different grip looks like a different gesture.
- Pick gestures that feel clearly different from each other.
- Vary speed and size a little while recording so the model learns the variation.
- If two gestures get confused, the confusion matrix shows which ones. Record more of those.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| `MPU6050 not found` | Check the four wires. If AD0 is tied to 3V3, set `MPU_ADDR = 0x69` in `include/config.h`. |
| `Could not open COM…` | The PlatformIO serial monitor is still open. Close it. |
| Board will not boot | You are holding BOOT while it resets. Let go and press EN. |
| Always "Unknown gesture" | Retrain with more recordings, or lower `MIN_CONFIDENCE` in `include/config.h`. |
| `WARNING: … placeholder` | You have not run `tools/train.py` yet. |

## Project layout

```
include/config.h         pins, sample rate, window size
include/model.h          trained weights (generated)
src/imu.cpp              MPU6050 driver
src/capture.cpp          hold-to-record capture
src/gesture_features.h   resizes a recording to 300 model inputs
src/nn.h                 neural network inference
src/main_collect.cpp     firmware 1: record data
src/main_recognize.cpp   firmware 2: recognize gestures
tools/collect.py         saves recordings to data/
tools/train.py           trains and exports the model
setup.bat record.bat train.bat   double-click helpers for Windows
```

## Results

Fill these in from your own run:

| Metric | Value |
| --- | --- |
| Gestures | |
| Recordings per gesture | |
| Test accuracy | |
| Inference time | |
| Model size | |
