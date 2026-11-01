#!/usr/bin/env python3
from pathlib import Path
import re
root = Path(__file__).resolve().parents[2]
audio = root/'src/frameworks/AVFAudio'
cmake = (audio/'CMakeLists.txt').read_text()
assert 'src/AVSpeech.m' in cmake
assert 'src/AVSpeechUtterance.m' not in cmake and 'src/AVSpeechSynthesizer.m' not in cmake
speech = (audio/'src/AVSpeech.m').read_text()
for name in ['AVSpeechUtterance','AVSpeechSynthesizer']:
    assert len(re.findall(r'@implementation\s+'+name+r'\b', speech)) == 1
for name in ['delegate','preUtteranceDelay','postUtteranceDelay','pitchMultiplier','volume','rate','voice']:
    assert name in speech
cv = (root/'src/frameworks/CoreVideo/src/CVPixelBuffer.c').read_text()
for name in ['CVBufferHasAttachment','CVIsCompressedPixelFormatAvailable','CVPixelBufferPoolFlush','CVImageBufferGetCleanRect']:
    assert len(re.findall(r'\b'+name+r'\([^;{}]*\)\s*\{', cv)) == 1, name
print('PASS: one speech implementation and one of each combined CoreVideo API')
