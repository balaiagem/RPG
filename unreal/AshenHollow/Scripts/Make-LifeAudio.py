"""Original procedural Foley and a seamless forest bed; no third-party recordings."""
import math
import os
import random
import struct
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Art', 'Audio')
os.makedirs(OUT, exist_ok=True)
RATE = 22050
rng = random.Random(731)

def save(name, samples):
    with wave.open(os.path.join(OUT, name + '.wav'), 'wb') as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(b''.join(struct.pack('<h', round(max(-1, min(1, x))*32760)) for x in samples))

for variant in range(3):
    samples = []
    low = 0
    for i in range(int(RATE * .24)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low = low * .68 + noise * .32
        envelope = min(1, t/.003) * math.exp(-t*28)
        crunch = noise * .17 * math.exp(-((t-.045)/.022)**2)
        samples.append(envelope*(low*.5 + math.sin(t*math.tau*(82+variant*13))*.23) + crunch)
    save('Footstep' + str(variant+1), samples)

samples = []
for i in range(int(RATE*.4)):
    t=i/RATE
    envelope=min(1,t/.002)*math.exp(-t*22)
    samples.append(envelope*(rng.uniform(-1,1)*.23 + math.sin(math.tau*135*t)*.25
                            + math.sin(math.tau*820*t)*.06))
save('Impact', samples)

# A periodic bed and wrapped bird phrases avoid a seam at the loop boundary.
duration=32
samples=[0.0]*(RATE*duration)
low=0
for i in range(len(samples)):
    t=i/RATE
    low=low*.97+rng.uniform(-1,1)*.03
    samples[i]=low*(.14+.035*math.sin(math.tau*t/duration))
for start in (3.2, 10.8, 19.3, 26.1):
    for phrase in range(3):
        length=.19+phrase*.025
        phase=0
        for j in range(int(length*RATE)):
            t=j/RATE
            phase+=math.tau*(2400+700*math.sin(t/length*math.pi)+phrase*220)/RATE
            at=int((start+phrase*.27)*RATE)+j
            samples[at%len(samples)]+=.035*math.sin(phase)*math.sin(math.pi*t/length)**2
seam=int(.15*RATE)
for i in range(seam):
    mix=i/seam
    samples[-seam+i]=samples[-seam+i]*(1-mix)+samples[i]*mix
save('Forest', samples)
print('Saved 5 original WAV files to', OUT)
