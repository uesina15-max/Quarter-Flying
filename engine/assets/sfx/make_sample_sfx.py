"""
샘플 효과음 생성기 (docs/USABILITY_IMPROVEMENT_PLAN.md X-2)

샘플 프로젝트의 효과음(Hit.wav, Whoosh.wav, Coin.wav)을 코드로 합성한다 - 외부 음원이 아니라서 저작권 문제가 없고,
어떻게 만들었는지 이 파일이 기록이다. 다시 만들려면:

    python make_sample_sfx.py        (이 폴더에서)

Build Game은 씬·액션이 참조하는 파일만 복사하므로 이 스크립트는 게임에 들어가지 않는다.
"""

import math
import os
import random
import struct
import wave

RATE = 44100


def write_wav(name, samples):
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), name)
    peak = max(1e-9, max(abs(s) for s in samples))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(32767 * 0.8 * s / peak)) for s in samples))
    print(f"wrote {path} ({len(samples) / RATE:.2f}s)")


def hit(duration=0.25):
    rnd = random.Random(1)
    out = []
    for i in range(int(RATE * duration)):
        t = i / RATE
        env = math.exp(-t * 18.0)
        freq = 140.0 * math.exp(-t * 6.0) + 60.0          # 떨어지는 저음
        out.append(env * (math.sin(2 * math.pi * freq * t) + 0.35 * (rnd.random() * 2 - 1) * math.exp(-t * 60.0)))
    return out


def whoosh(duration=0.45):
    rnd = random.Random(2)
    out, lp = [], 0.0
    for i in range(int(RATE * duration)):
        t = i / RATE
        env = math.sin(math.pi * t / duration) ** 2
        cutoff = 0.02 + 0.25 * (t / duration)            # 점점 밝아지는 노이즈
        lp += cutoff * ((rnd.random() * 2 - 1) - lp)
        out.append(env * lp)
    return out


def coin(duration=0.3):
    out = []
    for i in range(int(RATE * duration)):
        t = i / RATE
        freq = 988.0 if t < 0.07 else 1319.0              # B5 -> E6
        env = math.exp(-max(0.0, t - 0.07) * 12.0)
        out.append(env * (math.sin(2 * math.pi * freq * t) + 0.3 * math.sin(4 * math.pi * freq * t)))
    return out


if __name__ == "__main__":
    write_wav("Hit.wav", hit())
    write_wav("Whoosh.wav", whoosh())
    write_wav("Coin.wav", coin())
