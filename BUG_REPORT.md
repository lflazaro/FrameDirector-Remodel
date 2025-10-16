# Potential Bugs Identified in FrameDirector

## 1. stylesheet loading uses dangling data pointer
* **Location:** `FrameDirector/main.cpp`, `FrameDirectorApplication::setupStyle()`
* **Issue:** The code reads the stylesheet into a temporary `QByteArray` and immediately wraps it in `QLatin1String`. `QLatin1String` does **not** copy the data; it just points to the buffer owned by the temporary `QByteArray`, which is destroyed when the statement finishes. The application therefore keeps a pointer to freed memory when calling `setStyleSheet`, leading to undefined behaviour or random UI glitches at runtime.
* **Evidence:** `QString styleSheet = QLatin1String(styleFile.readAll());`【F:FrameDirector/main.cpp†L80-L83】
* **Suggested fix:** Keep the `QByteArray` alive (e.g. assign it to a variable and call `QString::fromLatin1(bytes)`), or call `QString::fromUtf8()` / `QString::fromLatin1()` directly on the returned data.

## 2. Missing `<QDateTime>` include in `Canvas.cpp`
* **Location:** `FrameDirector/Canvas.cpp`
* **Issue:** `LayerData` uses `QDateTime::currentMSecsSinceEpoch()` but the translation unit never includes `<QDateTime>`. This prevents the file from compiling with a clean environment and breaks any build that does not happen to include `<QDateTime>` earlier through transitive headers.
* **Evidence:** Includes list without `<QDateTime>` and the later use of `QDateTime`【F:FrameDirector/Canvas.cpp†L4-L53】
* **Suggested fix:** Add `#include <QDateTime>` to the top of the file.

## 3. Missing includes in `BucketFillTool.cpp`
* **Location:** `FrameDirector/BucketFillTool.cpp`
* **Issues:**
  * The file calls `QDateTime::currentMSecsSinceEpoch()` but does not include `<QDateTime>`, producing a compilation error on a clean build.【F:FrameDirector/BucketFillTool.cpp†L1-L36】【F:FrameDirector/BucketFillTool.cpp†L332-L337】
  * It also uses `std::function` inside the helper `findContainingNode` without including `<functional>`, which likewise causes compilation to fail.【F:FrameDirector/BucketFillTool.cpp†L1-L64】【F:FrameDirector/BucketFillTool.cpp†L230-L235】
* **Suggested fix:** Include the missing headers.

## 4. Audio waveform reader assumes 16‑bit PCM
* **Location:** `FrameDirector/MainWindow.cpp`, `MainWindow::createAudioWaveform`
* **Issue:** The decoder blindly casts every buffer to `const qint16*` and appends the data as 16‑bit integers. Many compressed formats (e.g. MP3, AAC) often decode to 32‑bit float or 8‑bit unsigned samples by default. Interpreting those buffers as `qint16` yields garbage amplitude values and can even read past the buffer when the frame size is smaller than assumed.
* **Evidence:** `const qint16* data = buffer.constData<qint16>();` followed by direct indexing without verifying `sampleFormat()`【F:FrameDirector/MainWindow.cpp†L1818-L1858】
* **Suggested fix:** Inspect `buffer.format().sampleFormat()` and convert/normalize samples accordingly before treating them as `qint16` data.

## 5. Audio import can hang on decoder errors
* **Location:** `FrameDirector/MainWindow.cpp`, `MainWindow::createAudioWaveform`
* **Issue:** The helper starts a `QEventLoop` and waits for `QAudioDecoder::finished`. If the decoder hits an error, no signal is connected to stop the loop, so the UI blocks indefinitely. This can happen with unsupported codecs or corrupted files.
* **Evidence:** Only `bufferReady` and `finished` are connected before `loop.exec()`; there is no connection to `errorChanged` or `errorOccurred` to quit the loop.【F:FrameDirector/MainWindow.cpp†L1818-L1834】
* **Suggested fix:** Connect `errorChanged` / `errorOccurred` to exit the loop (and surface the error) before calling `loop.exec()`.

## 6. Timeline keyframe creation fires twice
* **Location:** `FrameDirector/Timeline.cpp` and `FrameDirector/MainWindow.cpp`
* **Issue:** `Timeline::addKeyframe` directly calls `canvas->createKeyframe(frame)` and then emits `keyframeAdded`. `MainWindow` connects that signal to its own `addKeyframe()` slot, which pushes an undo command (or calls `createKeyframe` again). As a result, triggering a keyframe from the timeline invokes the creation logic twice, corrupting the undo stack and duplicating work.
* **Evidence:** Connection setup in the main window and the timeline implementation.【F:FrameDirector/MainWindow.cpp†L281-L301】【F:FrameDirector/Timeline.cpp†L1512-L1521】【F:FrameDirector/MainWindow.cpp†L2296-L2313】
* **Suggested fix:** Either stop calling `createKeyframe` inside `Timeline::addKeyframe` or remove/adjust the signal connection so the work happens in only one place.

