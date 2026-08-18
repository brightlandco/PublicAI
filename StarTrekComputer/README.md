# 🛸 Computer... (Voice Assistant Sandbox)

> *"Hello, computer?"* — Inspired by the iconic scene in *Star Trek IV* where Scotty grabs a Macintosh mouse and expects a voice interface, this project drops traditional GUIs to bring a seamless, hands-free voice loop natively to your workspace.

`agent.py` is a high-performance, single-file voice assistant written in Python. It runs a zero-network local pipeline combining low-latency Voice Activity Detection (VAD), localized Speech-to-Text (STT), a nimble Large Language Model (LLM), and real-time Text-to-Speech (TTS) synthesis with strict **barge-in** interruption handling.

---

### ⚡ Core Architecture & Deep Tech Stack

The script coordinates a multi-threaded duplex audio loop using a **`PlayRing` buffer** architecture. It splits audio input streams to ensure live interruption detection remains active even while synthesizing voice feedback.

* **🎙️ Speech-to-Text (STT):** Powered by `faster-whisper` (`base.en` using `int8` quantization) for rapid, accurate local transcriptions.
* **🧠 Local LLM Intelligence:** Integrates locally runnable GGUF weights via `llama.cpp` (defaulting to the highly efficient `Qwen2.5-0.5B-Instruct` 4-bit quantized model).
* **🔊 Text-to-Speech (TTS):** Employs the `Piper` architecture (supporting direct C-bindings or immediate CLI fallbacks) utilizing the high-quality `en_US-amy-medium` voice.
* **🎛️ Voice Activity Detection (VAD):** Leverages `webrtcvad` running tightly spaced 20ms frames to lock audio ingestion instantly on vocal bursts.
* **🛑 Real-time Barge-In:** Utilizes a `Vosk` mini-grammar engine operating on parallel input frames. It checks partial speech hypotheses second-by-second for immediate cancellation terms (e.g., *"stop"*, *"computer stop"*), immediately flushing the speaker ring buffers if triggered.
* **⚙️ Signal Processing Polish:** The engine applies automatic audio clipping to prevent runaway speech generation, alongside customized `int16` head/tail silence trimming and custom fade ramps to completely remove speaker crackle.

---

### 📦 Prerequisites & System Dependencies

#### macOS System Tools
The script relies on `sounddevice` which requires `portaudio`, and `Piper` for speech synthesis. Install them via [Homebrew](https://brew.sh):
```bash
brew install portaudio piper
```

#### Python Requirements
Install the required signal processing, audio streaming, and neural networks bindings:
```bash
pip install numpy sounddevice webrtcvad faster-whisper vosk
```
*(Optional: Provide an asset token to your terminal environment if utilizing gated Hugging Face repositories: `export HF_TOKEN="your_token_here"`)*

---

### 🛠️ Quick Start

Because the pipeline is self-contained inside a standalone file, deploying the agent requires zero complex orchestration or external container layouts.

1. **Clone and Enter the Workspace:**
   ```bash
   git clone https://github.com
   cd computer-agent
   ```

2. **Execute the Standalone Script:**
   ```bash
   python agent.py
   ```

The script will automatically scan or initialize directories (`/models`, `/voices`), download required lightweight components on its first boot, and immediately begin listening on your default microphone device.

---

### ⚙️ Engine Configurations (`agent.py`)

You can modify behavior directly inside the configuration block at the top of the file:

```python
SR = 16000                # Audio Sample Rate (16kHz standard)
FRAME_MS = 20             # Processing frame window sizing
WHISPER_MODEL = "base.en" # Quantized Faster-Whisper target 
WAKE_WORD = "computer"    # Wake token to prompt actions
ENABLE_VOSK_BARGE = True  # Enable second-by-second stop detection
```

---

### 📡 Automated Action Mapping

Beyond answering general knowledge queries, the script extracts intent to interact with your physical machine environment out of the box:
* **Live Web Crawling:** Hands-free execution of search queries via automated tool calling.
* **Native Browser Launching:** Autonomously formats, escapes, and deploys local browser threads to surface query visual results instantly.
