#!/usr/bin/env python3
# macOS single-file voice agent with per-second Vosk "match scores"
# - STT: faster-whisper  | VAD: webrtcvad
# - TTS: Piper (binding preferred, CLI fallback)
# - LLM: llama.cpp (local)
# - Wake: say "computer …"
# - Barge-in (while speaking): Vosk mini-grammar (stop/computer stop/hey jarvis/hey computer)
# - Per-second logs show Vosk partial hypothesis and similarity scores
# - No CLI flags needed

import os, sys, re, queue, webbrowser, requests, wave, math, traceback, platform, subprocess, json, time, threading, collections, shutil, zipfile
from datetime import datetime
from urllib.parse import quote

import numpy as np
import sounddevice as sd
import webrtcvad
from faster_whisper import WhisperModel

# ---------- Optional libs (OWW disabled by default) ----------
OWW_AVAILABLE = False
try:
    from openwakeword.model import Model as OWWModel
    from openwakeword.utils import download_models as oww_download_models
    OWW_AVAILABLE = True
except Exception:
    OWW_AVAILABLE = False

VOSK_AVAILABLE = False
try:
    import vosk
    VOSK_AVAILABLE = True
except Exception:
    VOSK_AVAILABLE = False

# ---------- Piper binding ----------
PiperVoice = None
try:
    from piper.voice import PiperVoice as _PV
    PiperVoice = _PV
except Exception as e:
    print(f"[init] Piper binding unavailable; will use CLI if needed. {e}")

# ---------- Config ----------
SR = 16000
FRAME_MS = 20
VAD_AGGR = 1
MAX_UTT_SEC = 25
SILENCE_FUSE_MS = 1200
WHISPER_MODEL = "base.en"
CT2_COMPUTE = "int8"

WAKE_WORD = "computer"

PIPER_VOICE_ID = "en_US-amy-medium"
VOICES_DIR = "voices"

TRIM_HEAD_TAIL_MS = 60
TRIM_THRESH = 300
FADE_MS = 5
MAX_TTS_SECONDS = 20

ENABLE_LLM = True
ENABLE_OWW = False          # keep off; Apple Silicon models often all-zero
ENABLE_VOSK_BARGE = True

LLM_MODELS_DIR = "models"
LLM_CANDIDATES = [
    ("Qwen2.5-0.5B-Instruct", "qwen2.5-0.5b-instruct-q4_k_m.gguf", [
        "https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/qwen2.5-0.5b-instruct-q4_k_m.gguf?download=1",
    ]),
]
_LLAMA = None
_HF_TOKEN = os.environ.get("HF_TOKEN")

# ---------- Utils ----------
def log_exc(prefix, e):
    print(f"{prefix}: {e}")
    tb = "".join(traceback.format_exception(type(e), e, e.__traceback__))
    print(tb)

def cwd_path(name): return os.path.join(os.getcwd(), name)

def write_wav_mono_int16(path, pcm_bytes, sample_rate):
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1); wf.setsampwidth(2); wf.setframerate(sample_rate); wf.writeframes(pcm_bytes)

# ---------- Resample int16 -> int16 ----------
def resample_int16(pcm_bytes, src_sr, dst_sr):
    if src_sr == dst_sr: return pcm_bytes
    x = np.frombuffer(pcm_bytes, dtype=np.int16).astype(np.float32)
    if x.size == 0: return b""
    n_src = x.shape[0]
    n_dst = int(round(n_src * (dst_sr / float(src_sr))))
    if n_dst <= 0: return b""
    x /= 32768.0
    src_idx = np.arange(n_src, dtype=np.float32)
    dst_idx = np.linspace(0, n_src - 1, n_dst, dtype=np.float32)
    y = np.clip(np.interp(dst_idx, src_idx, x), -1.0, 1.0)
    return (y * 32767.0).astype(np.int16).tobytes()

# ---------- TTS polish ----------
def _apply_fade_int16(arr_i16, sr, fade_ms=FADE_MS):
    if fade_ms <= 0: return arr_i16
    n = arr_i16.shape[0]
    k = max(1, int(sr * fade_ms / 1000.0)); k = min(k, n // 2)
    if k <= 0: return arr_i16
    ramp = np.linspace(0.0, 1.0, k, dtype=np.float32)
    out = arr_i16.astype(np.float32)
    out[:k] *= ramp; out[-k:] *= ramp[::-1]
    return np.clip(out, -32768, 32767).astype(np.int16)

def _trim_silence_int16(arr_i16, sr, max_ms=TRIM_HEAD_TAIL_MS, thr=TRIM_THRESH):
    if max_ms <= 0: return arr_i16, 0, 0
    max_samples = int(sr * max_ms / 1000.0)
    n = arr_i16.shape[0]
    head_end = 0; limit = min(n, max_samples)
    while head_end < limit and abs(int(arr_i16[head_end])) <= thr: head_end += 1
    tail_start = n; limit2 = max(0, n - max_samples)
    while tail_start > limit2 and abs(int(arr_i16[tail_start-1])) <= thr: tail_start -= 1
    if head_end == 0 and tail_start == n: return arr_i16, 0, 0
    return arr_i16[head_end:tail_start], head_end, n - tail_start

def polish_pcm(pcm_bytes, sr):
    a = np.frombuffer(pcm_bytes, dtype=np.int16)
    orig = a.shape[0]
    a2, htrim, ttrim = _trim_silence_int16(a, sr, TRIM_HEAD_TAIL_MS, TRIM_THRESH)
    a3 = _apply_fade_int16(a2, sr, FADE_MS)
    print(f"[tts] polish: samples {orig} -> {a3.shape[0]} (trim {htrim}/{ttrim} samples, fade {FADE_MS}ms)")
    return a3.tobytes()

def clip_wav_seconds(pcm_bytes, sample_rate, max_seconds):
    if not max_seconds or max_seconds <= 0: return pcm_bytes
    max_samples = int(sample_rate * max_seconds)
    arr = np.frombuffer(pcm_bytes, dtype=np.int16)
    if arr.shape[0] <= max_samples: return pcm_bytes
    print(f"[tts] clipping playback to {max_seconds}s (from {arr.shape[0]/sample_rate:.2f}s)")
    return arr[:max_samples].tobytes()

# ---------- Duplex I/O ----------
class PlayRing:
    def __init__(self, sr: int, max_seconds=30):
        self.sr = sr
        self.max_len = int(sr * max_seconds)
        self.buf = collections.deque()
        self.n_samples = 0
        self.lock = threading.Lock()
    def clear(self):
        with self.lock:
            self.buf.clear(); self.n_samples = 0
    def append(self, a_f32: np.ndarray):
        if a_f32 is None or a_f32.size == 0: return
        a = a_f32.astype(np.float32).reshape(-1)
        with self.lock:
            self.buf.append(a); self.n_samples += a.shape[0]
            while self.n_samples > self.max_len and self.buf:
                head = self.buf[0]
                drop = min(self.n_samples - self.max_len, head.shape[0])
                if drop >= head.shape[0]:
                    self.buf.popleft(); self.n_samples -= head.shape[0]
                else:
                    self.buf[0] = head[drop:].copy(); self.n_samples -= drop
    def pop_into(self, n: int) -> np.ndarray:
        out = np.zeros(n, dtype=np.float32)
        if n <= 0: return out
        k = 0
        with self.lock:
            while k < n and self.buf:
                chunk = self.buf[0]; need = n - k; take = min(need, chunk.shape[0])
                out[k:k+take] = chunk[:take]
                if take == chunk.shape[0]: self.buf.popleft()
                else: self.buf[0] = chunk[take:]
                k += take; self.n_samples -= take
        return out
    def has_audio(self) -> bool:
        with self.lock: return self.n_samples > 0

class DuplexAudio:
    def __init__(self, sr=SR, frame_ms=FRAME_MS, input_device=None, output_device=None, debug=False):
        self.sr = sr; self.frame = int(sr * frame_ms / 1000)
        self.input_device = input_device; self.output_device = output_device
        self.debug = debug
        self.play_ring = PlayRing(sr)
        self.input_q = queue.Queue(maxsize=512)
        self.stop_q  = queue.Queue(maxsize=2048)
        self.out_rms_q = collections.deque(maxlen=32)
        self.stream = None
        self.last_play_start = 0.0

    def _cb(self, indata, outdata, frames, time_info, status):
        if status: print(f"[io] status: {status}")
        # output
        try:
            out = self.play_ring.pop_into(frames) if self.play_ring.has_audio() else np.zeros(frames, dtype=np.float32)
            outdata[:,0] = out
            if out.size:
                rms = float(np.sqrt(np.mean(out**2) + 1e-12))
                self.out_rms_q.append(rms)
        except Exception as e:
            log_exc("[io] speaker fill failed", e); outdata[:,0] = 0.0
        # input
        try:
            mic_f32 = indata.reshape(-1).astype(np.float32)
            # always feed barge-in
            try:
                self.stop_q.put_nowait(mic_f32.copy())
            except queue.Full:
                try: _ = self.stop_q.get_nowait()
                except Exception: pass
                try: self.stop_q.put_nowait(mic_f32.copy())
                except Exception: pass
            # STT only when not speaking
            if not self.play_ring.has_audio():
                pcm_i16 = (np.clip(mic_f32, -1.0, 1.0) * 32768.0).astype(np.int16).tobytes()
                try:
                    self.input_q.put_nowait(pcm_i16)
                except queue.Full:
                    try: _ = self.input_q.get_nowait()
                    except Exception: pass
                    try: self.input_q.put_nowait(pcm_i16)
                    except Exception: pass
        except Exception as e:
            log_exc("[io] mic enqueue failed", e)

    def start(self):
        if self.stream: return
        print(f"[io] opening full-duplex stream @ {self.sr} Hz, frame={self.frame}, in_dev={self.input_device}, out_dev={self.output_device}")
        self.stream = sd.Stream(samplerate=self.sr, blocksize=self.frame, dtype='float32',
                                channels=1, callback=self._cb, device=(self.input_device, self.output_device))
        self.stream.start()

    def stop(self):
        try:
            if self.stream:
                self.stream.stop(); self.stream.close()
        except Exception as e: log_exc("[io] stop failed", e)
        self.stream = None

    def iter_input_bytes(self):
        while True:
            try: yield self.input_q.get(timeout=2.0)
            except queue.Empty: yield b""

    def play_bytes_int16(self, pcm_bytes_i16):
        if not self.play_ring.has_audio():
            self.last_play_start = time.time()
        a32 = np.frombuffer(pcm_bytes_i16, dtype=np.int16).astype(np.float32) / 32768.0
        self.play_ring.append(a32)

    def stop_playback(self): self.play_ring.clear()
    def is_playing(self): return self.play_ring.has_audio()
    def pop_stop_frame(self, timeout=0.2):
        try: return self.stop_q.get(timeout=timeout)
        except queue.Empty: return None
    def smoothed_out_db(self):
        if not self.out_rms_q: return -120.0
        rms = max(self.out_rms_q)
        return 20.0 * math.log10(max(rms, 1e-8))

# ---------- VAD ----------
class VADSegmenter:
    def __init__(self, sample_rate=SR, frame_ms=FRAME_MS, aggressiveness=VAD_AGGR,
                 silence_fuse_ms=SILENCE_FUSE_MS, max_utt_sec=MAX_UTT_SEC, debug=False):
        self.vad = webrtcvad.Vad(aggressiveness); self.sr = sample_rate
        self.frame_bytes = int(sample_rate * frame_ms / 1000) * 2
        self.silence_fuse_frames = int(silence_fuse_ms / frame_ms)
        self.max_frames = int((max_utt_sec * 1000) / frame_ms)
        self.debug = debug
        print(f"[vad] frame_bytes={self.frame_bytes}, fuse_frames={self.silence_fuse_frames}, max_frames={self.max_frames}")
    def stream_segments(self, audio_bytes_iter):
        silence_run = 0; speaking = False; voiced = []; idx = 0
        def chunker(buf, chunk_bytes):
            rem = b""
            for b in buf:
                rem += b
                while len(rem) >= chunk_bytes:
                    yield rem[:chunk_bytes]; rem = rem[chunk_bytes:]
        for frame in chunker(audio_bytes_iter, self.frame_bytes):
            idx += 1
            try: is_sp = self.vad.is_speech(frame, self.sr)
            except Exception: is_sp = False
            if self.debug and idx % 10 == 0:
                print(f"[vad] frame#{idx} speech={is_sp} voiced_len={len(voiced)}")
            if is_sp:
                if not speaking: print("[vad] speech started")
                speaking = True; voiced.append(frame); silence_run = 0
            else:
                if speaking:
                    silence_run += 1
                    if silence_run >= self.silence_fuse_frames or len(voiced) >= self.max_frames:
                        print("[vad] speech ended"); yield b"".join(voiced)
                        voiced, speaking, silence_run = [], False, 0

# ---------- TTS ----------
class SpeakerPiper:
    def __init__(self, model_path: str, config_path: str, io: DuplexAudio, debug=False):
        self.model_path = model_path; self.config_path = config_path; self.io = io
        self.use_binding = PiperVoice is not None
        self.sample_rate_src = 22050; self.debug = debug
        if self.use_binding:
            try:
                print("[tts] loading Piper voice (binding)…")
                self.voice = PiperVoice.load(model_path, config_path=config_path)
                self.sample_rate_src = int(self.voice.config.sample_rate)
                print(f"[tts] voice sample_rate={self.sample_rate_src}")
            except Exception as e:
                log_exc("[tts] binding load failed", e); self.use_binding = False
                try:
                    with open(config_path, "r", encoding="utf-8") as f: cfg = json.load(f)
                    self.sample_rate_src = int(cfg.get("sample_rate", 22050))
                except Exception: self.sample_rate_src = 22050

    def _chunk_to_bytes(self, chunk):
        if isinstance(chunk, (bytes, bytearray, memoryview)): return bytes(chunk)
        for attr in ("audio_int16_bytes", "_audio_int16_bytes"):
            if hasattr(chunk, attr):
                v = getattr(chunk, attr); return bytes(v) if isinstance(v, (bytes, bytearray, memoryview)) else v
        for attr in ("audio_int16_array", "audio_float_array", "_audio_int16_array"):
            if hasattr(chunk, attr):
                arr = getattr(chunk, attr)
                if arr is None: continue
                if hasattr(arr, "dtype") and "float" in str(arr.dtype):
                    a = np.clip(arr, -1.0, 1.0).astype(np.float32)
                    return (a * 32767.0).astype(np.int16).tobytes()
                else:
                    return np.asarray(arr, dtype=np.int16).tobytes()
        if isinstance(chunk, np.ndarray): return chunk.astype(np.int16).tobytes()
        return None

    def synth_binding_to_bytes(self, text: str):
        frames = []; sr = None
        try:
            for chunk in self.voice.synthesize(text):
                b = self._chunk_to_bytes(chunk)
                if b: frames.append(b)
                if sr is None and hasattr(chunk, "sample_rate") and getattr(chunk, "sample_rate"):
                    sr = int(chunk.sample_rate)
        except Exception as e:
            log_exc("[tts] synth (binding) exception", e); return None, None
        if not frames: print("[tts] binding produced no frames."); return None, None
        return b"".join(frames), (sr or self.sample_rate_src)

    def synth_cli_to_wav(self, text: str, out_wav: str):
        try:
            cmd = [sys.executable, "-m", "piper", "--model", self.model_path,
                   "--config", self.config_path, "--output_file", out_wav, "--text", text]
            res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if res.returncode != 0:
                print(f"[tts] CLI error code {res.returncode}")
                if res.stdout: print("[tts] CLI stdout:", res.stdout)
                if res.stderr: print("[tts] CLI stderr:", res.stderr)
                return None, None
            with wave.open(out_wav, "rb") as wf:
                sr = wf.getframerate(); data = wf.readframes(wf.getnframes())
            return data, sr
        except Exception as e:
            log_exc("[tts] CLI invocation failed", e); return None, None

    def speak(self, text: str) -> bool:
        try:
            pcm_src, sr_src = (None, None)
            if self.use_binding:
                pcm_src, sr_src = self.synth_binding_to_bytes(text)
                if pcm_src is None: print("[tts] binding produced no audio; trying CLI…")
            if pcm_src is None:
                tmp = cwd_path("piper_output_src.wav")
                pcm_src, sr_src = self.synth_cli_to_wav(text, tmp)
                if pcm_src is None:
                    print("[tts] ALL TTS PATHS FAILED — no audio produced."); return False
            pcm_src = polish_pcm(pcm_src, sr_src)
            pcm_src = clip_wav_seconds(pcm_src, sr_src, MAX_TTS_SECONDS)
            try: write_wav_mono_int16(cwd_path("piper_output_src.wav"), pcm_src, sr_src)
            except Exception: pass
            pcm_16k = resample_int16(pcm_src, sr_src, SR)
            try: write_wav_mono_int16(cwd_path("piper_output_16k.wav"), pcm_16k, SR)
            except Exception: pass
            secs = len(pcm_16k)/2/SR
            print(f"[tts] ready (16k) {secs:.2f}s -> enqueue")
            self.io.play_bytes_int16(pcm_16k)
            return True
        except Exception as e:
            log_exc("[tts] speak failed", e); return False

    def speak_startup_blocking(self, text: str) -> bool:
        ok = self.speak(text)
        if not ok: return False
        t0 = time.time()
        while self.io.is_playing() and (time.time() - t0) < 5.0:
            time.sleep(0.05)
        print("[startup-tts] played via duplex stream")
        return True

# ---------- LLM (llama.cpp) ----------
def try_prepare_single(url, out_path):
    headers = {}
    if _HF_TOKEN: headers["Authorization"] = f"Bearer {_HF_TOKEN}"
    print(f"[dl] GET {url}")
    with requests.get(url, stream=True, timeout=120, headers=headers) as r:
        r.raise_for_status()
        with open(out_path, "wb") as f:
            for chunk in r.iter_content(chunk_size=1<<16):
                if chunk: f.write(chunk)
    return os.path.isfile(out_path) and os.path.getsize(out_path) > (10<<20)

def auto_prepare_llm(model_dir=LLM_MODELS_DIR):
    os.makedirs(model_dir, exist_ok=True)
    for friendly, filename, urls in LLM_CANDIDATES:
        path = os.path.join(model_dir, filename)
        if os.path.isfile(path) and os.path.getsize(path) > (10<<20):
            print(f"[llm] model present: {path}"); return path
        for url in urls:
            try:
                if try_prepare_single(url, path):
                    print(f"[llm] model ready: {path}"); return path
            except Exception as e:
                print(f"[llm] download failed: {e}")
    print("[llm] All model URLs failed; continuing without local LLM."); return None

def init_llm(model_path):
    global _LLAMA
    if not model_path: return None
    try:
        from llama_cpp import Llama
    except Exception:
        print("[llm] llama-cpp-python not installed."); return None
    try:
        _LLAMA = Llama(model_path=model_path, n_ctx=1024, n_threads=os.cpu_count() or 4, verbose=False)
        print("[llm] initialized local LLM."); return _LLAMA
    except Exception as e:
        log_exc("[llm] init failed", e); return None

_SANITIZE_RE = re.compile(r'\s*(?:\[/?INST\]|\[/?OUTLINER\]|\[/?SYSTEM\])\s*', re.I)
def sanitize_llm_text(s: str) -> str:
    s = _SANITIZE_RE.sub(' ', s); s = s.replace("{", "").replace("}", ""); return " ".join(s.split())

def llm_answer(question: str) -> str:
    if _LLAMA is None: return "LLM not enabled."
    try:
        prompt = ("You are a concise, careful assistant. Answer in 2–4 short sentences. "
                  "Avoid hallucinations and speculation.\n"
                  f"Question: {question.strip()}\nAnswer:")
        out = _LLAMA(prompt=prompt, max_tokens=120, temperature=0.4, top_p=0.9, repeat_penalty=1.1, stop=["</s>"])
        text = (out.get("choices", [{}])[0].get("text", "") or "").strip()
        return sanitize_llm_text(text) or "I don't have an answer."
    except Exception as e:
        log_exc("[llm] generation failed", e); return "Sorry, local LLM failed."

# ---------- Intents ----------
DOMAIN_RE = re.compile(r'^[\w.-]+\.[a-z]{2,}(/.*)?$', re.I)
TLD_RE   = re.compile(r'\.(com|org|net|io|ai|dev|app|edu|gov|co|us|uk|ca|de|fr|jp|au|in|it|es|nl|se|no|ch|be|br|mx|news)\b', re.I)
_PUNC_CHARS = ".,;:!?\"'()[]{}-"

HELP_TEXT = """\
Available commands:
• open <url|domain|app> — e.g., "open google.com", "open https://example.com", "open notes"
• search <query> — web search via DuckDuckGo
• time / date — current time or date
• volume — "set volume to 30 percent", "mute", "unmute", "louder", "quieter"
• ask <question> — ask the local LLM
• stop — while I'm speaking, say "stop" / "computer stop" / "hey jarvis" / "hey computer" to interrupt immediately
• help — shows this list
"""

def normalize_phrase_for_intent(s: str) -> str:
    return s.strip().strip(f" \t\r\n{_PUNC_CHARS}")

def normalize_open_target(s: str):
    s = re.sub(r'^(?:at|to)\s+', '', s.strip(), flags=re.I)
    s = s.strip(f" \t\r\n{_PUNC_CHARS}")
    if s.lower().startswith("www."): return "https://" + s, "url"
    if re.match(r'^(https?://)', s, re.I): return s, "url"
    if TLD_RE.search(s) and " " not in s: return "https://" + s, "domain"
    return s, "raw"

def open_app(app_name: str) -> bool:
    try:
        res = subprocess.run(["open", "-a", app_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if res.returncode == 0: return True
        cap = " ".join(w.capitalize() for w in app_name.split())
        if cap != app_name:
            res = subprocess.run(["open", "-a", cap], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            return res.returncode == 0
        return False
    except Exception as e:
        log_exc("[open_app] failed", e); return False

def set_volume(percent: int) -> bool:
    pct = max(0, min(100, int(percent)))
    try:
        return subprocess.run(["osascript", "-e", f"set volume output volume {pct}"]).returncode == 0
    except Exception as e:
        log_exc("[volume] set failed", e); return False

def change_volume(delta: int) -> bool:
    try:
        res = subprocess.run(["osascript", "-e", 'output volume of (get volume settings)'], stdout=subprocess.PIPE, text=True)
        cur = int(res.stdout.strip())
    except Exception:
        cur = 50
    return set_volume(cur + delta)

def mute_volume(on: bool) -> bool:
    try:
        return subprocess.run(["osascript", "-e", f"set volume output muted {'true' if on else 'false'}"]).returncode == 0
    except Exception as e:
        log_exc("[volume] mute failed", e); return False

def _maybe_open_domain(text: str):
    t = text.strip(); low = t.lower()
    m = re.match(r'^open([a-z0-9._\-]+(?:\.[a-z]{2,})(?:/.*)?)$', low)
    if m:
        domain = m.group(1); url, _ = normalize_open_target(domain)
        webbrowser.open(url); return f"Opening {url}"
    if DOMAIN_RE.match(low):
        url, _ = normalize_open_target(low)
        webbrowser.open(url); return f"Opening {url}"
    return None

def simple_agent(text: str, llm_enabled: bool, io: DuplexAudio) -> str:
    t = normalize_phrase_for_intent(text); tl = t.lower()

    if tl == "stop":
        if io.is_playing(): io.stop_playback(); return "(stopped speaking)"
        return "Nothing is playing."

    if tl in ("help", "commands", "what are available commands", "what can you do"):
        return HELP_TEXT

    opened = _maybe_open_domain(t)
    if opened: return opened

    if tl.startswith("ask "):
        q = t[4:].strip()
        return llm_answer(q) if (llm_enabled and q) else ("LLM not enabled." if not llm_enabled else "What should I ask?")

    if tl.startswith("open"):
        m = re.match(r'^open\s*(.*)$', t, flags=re.I)
        query = (m.group(1) if m else "").strip()
        if query:
            target, kind = normalize_open_target(query)
            if kind in ("url", "domain"):
                webbrowser.open(target); return f"Opening {target}"
            if open_app(query): return f"Opening the app {query}"
            url = f"https://duckduckgo.com/?q={quote(query)}"
            webbrowser.open(url); return f"Searching for {query}"
        return "Say: open <site or app>"

    if tl.startswith("search "):
        q = t[7:].strip()
        url = f"https://duckduckgo.com/?q={quote(q)}"
        webbrowser.open(url); return f"Searching for {q}"

    m = re.search(r'(?:set )?volume (?:to )?(\d{1,3})\s*percent', tl)
    if m:
        pct = int(m.group(1)); return ("Setting volume to %d percent" % pct) if set_volume(pct) else "Sorry, I couldn't set the volume."
    if "unmute" in tl: return "Unmuted" if mute_volume(False) else "Sorry, I couldn't unmute."
    if "mute" in tl:    return "Muted" if mute_volume(True) else "Sorry, I couldn't mute."
    if "louder" in tl or "increase volume" in tl: return "Louder" if change_volume(+10) else "Sorry, I couldn't change the volume."
    if "quieter" in tl or "decrease volume" in tl or "lower volume" in tl: return "Quieter" if change_volume(-10) else "Sorry, I couldn't change the volume."

    if "what time" in tl or "current time" in tl or tl == "time":
        now = datetime.now().strftime("%I:%M %p").lstrip("0"); return f"It is {now}"
    if "what's the date" in tl or "what is the date" in tl or tl == "date":
        today = datetime.now().strftime("%A, %B %d, %Y"); return f"Today is {today}"

    return llm_answer(t) if llm_enabled else "Say 'help' for commands."

# ---------- Wake extraction ----------
_PUNC = r"\.,;:!\?\"'()\[\]\{\}-"
def extract_command_with_wake(text, wake_word):
    if not wake_word: return True, text.strip(), "no_wake_required"
    raw = text.strip(); low = raw.lower()
    pat = rf"""^\s*(?:hey[\s{_PUNC}]+)?{re.escape(wake_word)}(?:[\s{_PUNC}]+)(.*)$"""
    m = re.match(pat, low)
    if m:
        start = m.start(1); cmd = raw[start:].strip()
        return True, cmd, "wake_matched"
    return False, "", "wake_not_found"

# ---------- Selftest tone ----------
def tone_test(output_device=None):
    sr = 22050; secs = 1.0
    n = int(sr * secs); t = np.arange(n) / sr
    wave_f32 = (0.2 * np.sin(2 * math.pi * 440 * t)).astype(np.float32)
    pcm_i16 = (np.clip(wave_f32, -1.0, 1.0) * 32767.0).astype(np.int16).tobytes()
    path = cwd_path("tone_test.wav")
    try:
        with wave.open(path, "wb") as wf:
            wf.setnchannels(1); wf.setsampwidth(2); wf.setframerate(sr); wf.writeframes(pcm_i16)
        print(f"[selftest] wrote {path} ({secs:.2f}s @ {sr} Hz)")
    except Exception as e:
        log_exc("[selftest] failed writing tone_test.wav", e)
    try:
        sd.play(wave_f32, sr, blocking=True, device=output_device)
        print("[selftest] tone playback done")
    except Exception as e:
        log_exc("[selftest] tone playback error", e)

# ---------- Downloads ----------
def ensure_dir(path): os.makedirs(path, exist_ok=True); return path
def download_file(url, dest_path):
    print(f"[dl] GET {url}")
    with requests.get(url, stream=True, timeout=120) as r:
        r.raise_for_status()
        with open(dest_path, "wb") as f:
            for chunk in r.iter_content(chunk_size=1<<16):
                if chunk: f.write(chunk)

def piper_voice_paths(voice_id, out_dir=VOICES_DIR):
    onnx = os.path.join(out_dir, f"{voice_id}.onnx")
    cfg  = os.path.join(out_dir, f"{voice_id}.onnx.json")
    return onnx, cfg

def auto_download_piper_voice(voice_id=PIPER_VOICE_ID, out_dir=VOICES_DIR):
    ensure_dir(out_dir)
    onnx_path, cfg_path = piper_voice_paths(voice_id, out_dir)
    if os.path.isfile(onnx_path) and os.path.isfile(cfg_path):
        print(f"[piper] voice exists: {onnx_path}, {cfg_path}")
        return onnx_path, cfg_path
    parts = voice_id.split("-")
    lang_region, name, quality = parts[0], parts[1], "-".join(parts[2:])
    lang = lang_region.split("_")[0]
    base = "https://huggingface.co/rhasspy/piper-voices/resolve/main"
    url_dir = f"{base}/{lang}/{lang_region}/{name}/{quality}"
    onnx_url = f"{url_dir}/{voice_id}.onnx?download=1"
    cfg_url  = f"{url_dir}/{voice_id}.onnx.json?download=1"
    print(f"[piper] downloading {voice_id} …")
    download_file(onnx_url, onnx_path); download_file(cfg_url, cfg_path)
    return onnx_path, cfg_path

# ---------- Vosk helpers ----------
def _looks_like_vosk_model(path: str) -> bool:
    return os.path.isdir(os.path.join(path, "am")) or os.path.isfile(os.path.join(path, "conf", "model.conf"))

def _prepare_vosk_model_dir(root="vosk-mini-en") -> str:
    root = cwd_path(root)
    ensure_dir(root)
    # If the root already contains a valid model dir (maybe nested), find it
    for d in [root] + [os.path.join(root, x) for x in os.listdir(root)]:
        if os.path.isdir(d) and _looks_like_vosk_model(d):
            return d
    # Otherwise (re)download and extract
    url = "https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip"
    zpath = cwd_path("vosk_small_en.zip")
    if not os.path.isfile(zpath) or os.path.getsize(zpath) < (10<<20):
        download_file(url, zpath)
    # Clean root, then extract
    for name in os.listdir(root):
        p = os.path.join(root, name)
        try:
            if os.path.isdir(p): shutil.rmtree(p)
            else: os.remove(p)
        except Exception: pass
    with zipfile.ZipFile(zpath, 'r') as zf:
        zf.extractall(root)
    # Find inner model dir
    candidates = [os.path.join(root, d) for d in os.listdir(root) if os.path.isdir(os.path.join(root, d))]
    for c in candidates:
        if _looks_like_vosk_model(c): return c
    return root

# ---------- String similarity (0..1) ----------
def _lev_ratio(a: str, b: str) -> float:
    a = a.lower().strip(); b = b.lower().strip()
    if not a and not b: return 1.0
    if not a or not b: return 0.0
    la, lb = len(a), len(b)
    dp = list(range(lb+1))
    for i in range(1, la+1):
        prev = dp[0]; dp[0] = i
        ca = a[i-1]
        for j in range(1, lb+1):
            cost = 0 if ca == b[j-1] else 1
            prev, dp[j] = dp[j], min(dp[j]+1, dp[j-1]+1, prev+cost)
    dist = dp[lb]
    return max(0.0, 1.0 - dist / max(la, lb))

# ---------- ALWAYS-ON barge-in thread (Vosk only, with per-second scores) ----------
class BargeIn(threading.Thread):
    """
    While TTS is playing:
      - Feed Vosk in ~0.8s windows
      - Log partial hypothesis + similarity scores each second
      - Trigger stop when hypothesis matches any hotword
      - Loudness fallback
    """
    TRIGGERS = ("stop", "computer stop", "hey jarvis", "hey computer")

    def __init__(self, io, use_vosk=True, margin_db=10.0, arm_delay_ms=200, mic_floor_db=-45.0, debug=True):
        super().__init__(daemon=True)
        self.io = io
        self.use_vosk = use_vosk and VOSK_AVAILABLE
        self.margin_db = float(margin_db)
        self.arm_delay_ms = float(arm_delay_ms)
        self.mic_floor_db = float(mic_floor_db)
        self.debug = debug
        self._stop_evt = threading.Event()
        self.need = int(0.8 * SR)  # ~0.8s window
        self._accum = np.zeros(0, dtype=np.float32)
        self._last_meter = 0.0
        self._last_scores = 0.0

        # Vosk
        self.vosk = None
        self.vosk_rec = None

    def db(self, x): return 20.0 * math.log10(max(x, 1e-8))

    def _init_vosk(self):
        if not self.use_vosk: return
        try:
            model_dir = _prepare_vosk_model_dir("vosk-mini-en")
            print(f"[vosk] using model dir: {model_dir}")
            self.vosk_model = vosk.Model(model_dir)
            grammar = '["stop", "computer stop", "hey jarvis", "hey computer", "[unk]"]'
            self.vosk_rec = vosk.KaldiRecognizer(self.vosk_model, SR, grammar)
            self.vosk = True
            print("[vosk] mini-grammar ready.")
        except Exception as e:
            log_exc("[vosk] init failed", e)
            self.vosk = None

    def run(self):
        self._init_vosk()

        while not self._stop_evt.is_set():
            frame = self.io.pop_stop_frame(timeout=0.2)
            if frame is None: continue
            self._accum = np.concatenate([self._accum, frame])
            if self._accum.shape[0] < self.need: continue

            buf = self._accum[-self.need:].copy()
            self._accum = self._accum[-self.need//2:]

            mic_rms = float(np.sqrt(np.mean(buf**2) + 1e-12))
            mic_db = self.db(mic_rms)
            out_db = self.io.smoothed_out_db()
            now = time.time()

            if self.debug and self.io.is_playing() and (now - self._last_meter) >= 1.0:
                print(f"[meter] mic={mic_db:5.1f} dB  out~={out_db:5.1f} dB  playing={self.io.is_playing()}")
                self._last_meter = now

            if not self.io.is_playing(): continue
            if (now - self.io.last_play_start)*1000.0 < self.arm_delay_ms: continue
            if mic_db <= self.mic_floor_db: continue

            # ---- Vosk grammar with partials + per-second scores ----
            if self.vosk:
                try:
                    pcm_i16 = (np.clip(buf, -1.0, 1.0) * 32768.0).astype(np.int16).tobytes()
                    matched_text = ""
                    if self.vosk_rec.AcceptWaveform(pcm_i16):
                        res = json.loads(self.vosk_rec.Result() or "{}")
                        matched_text = (res.get("text") or "").strip()
                    else:
                        pres = json.loads(self.vosk_rec.PartialResult() or "{}")
                        matched_text = (pres.get("partial") or "").strip()

                    # compute similarity scores to triggers
                    if (now - self._last_scores) >= 1.0:
                        scores = {t: round(_lev_ratio(matched_text, t), 2) for t in self.TRIGGERS}
                        print(f"[vosk-partial] partial={matched_text!r}  " +
                              "  ".join([f"{k.replace(' ', '_')}={v:.2f}" for k,v in scores.items()]))
                        self._last_scores = now

                    # match policy: exact phrase OR high similarity OR substring
                    mt = matched_text.lower()
                    for trig in self.TRIGGERS:
                        if not mt: continue
                        if trig in mt or _lev_ratio(mt, trig) >= 0.85:
                            print(f"[barge] vosk → '{trig}' match → stop")
                            self.io.stop_playback()
                            try: self.vosk_rec.Reset()
                            except Exception: pass
                            time.sleep(0.2)
                            break
                    else:
                        # Loudness fallback
                        if (mic_db - out_db) >= (self.margin_db + 4.0):
                            print("[barge] loudness margin → stop")
                            self.io.stop_playback(); time.sleep(0.2)
                except Exception as e:
                    log_exc("[vosk] recog failed", e)

    def stop(self): self._stop_evt.set()

# ---------- Main ----------
def main():
    print(f"[env] Python {sys.version.split()[0]}, platform={platform.platform()}, machine={platform.machine()}")
    try:
        default_in, default_out = sd.default.device
        print(f"[audio] default devices -> in:{default_in} out:{default_out}")
    except Exception as e:
        print(f"[audio] could not query default devices: {e}")

    tone_test(output_device=sd.default.device[1] if sd.default.device else None)

    onnx_path, cfg_path = auto_download_piper_voice(PIPER_VOICE_ID, VOICES_DIR)

    io = DuplexAudio(sr=SR, frame_ms=FRAME_MS)
    io.start()

    print(f"[init] Loading Piper voice: {os.path.basename(onnx_path)}")
    speaker = SpeakerPiper(onnx_path, cfg_path, io=io)

    print(f"[init] Loading faster-whisper: {WHISPER_MODEL} ({CT2_COMPUTE}) …")
    stt_model = WhisperModel(WHISPER_MODEL, compute_type=CT2_COMPUTE)

    llm_enabled = ENABLE_LLM
    if llm_enabled:
        mp = auto_prepare_llm(); 
        if not init_llm(mp):
            print("[llm] unavailable; continuing without."); llm_enabled = False

    vad = VADSegmenter(aggressiveness=VAD_AGGR, silence_fuse_ms=SILENCE_FUSE_MS, max_utt_sec=MAX_UTT_SEC, debug=False)
    print(f"[ready] Say '{WAKE_WORD} …' (Ctrl+C to quit)")

    try: speaker.speak_startup_blocking("Ready. Say 'computer help'. While I'm speaking, say 'stop' to interrupt.")
    except Exception as e: log_exc("[startup] greeting failed", e)

    # Always-on barge-in thread (Vosk only)
    barge = BargeIn(io, use_vosk=ENABLE_VOSK_BARGE, margin_db=10.0, mic_floor_db=-45.0, debug=True)
    barge.start()

    audio_iter = io.iter_input_bytes()
    def pcm16_to_float32(pcm16):
        a = np.frombuffer(pcm16, dtype=np.int16).astype(np.float32); a /= 32768.0; return a

    try:
        for utt_bytes in vad.stream_segments(audio_iter):
            if not utt_bytes: print("[stt] (empty frame)"); continue
            if io.is_playing():
                print("[barge] speaking; STT paused (say 'stop')"); continue

            wav = pcm16_to_float32(utt_bytes)
            segments, _ = stt_model.transcribe(wav, language="en", vad_filter=False, beam_size=1)
            text = " ".join(s.text.strip() for s in segments).strip()
            if not text: print("[stt] (empty transcript)"); continue

            print(f"[you] {text}")
            triggered, cmd, reason = extract_command_with_wake(text, WAKE_WORD)
            print(f"[wake] triggered={triggered} reason={reason} cmd='{cmd}'")
            if not triggered: continue

            reply = simple_agent(cmd, llm_enabled, io)
            log_line = reply.replace("\n", " ")
            print(f"[bot] {log_line[:200]}{'…' if len(log_line)>200 else ''}")
            if reply and not reply.startswith("("):
                speaker.speak(reply)

    except KeyboardInterrupt:
        print("\n[exit] Bye!")
    finally:
        try:
            barge.stop(); io.stop()
        except Exception: pass

if __name__ == "__main__":
    main()
