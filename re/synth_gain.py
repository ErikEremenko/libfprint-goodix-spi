import glob, numpy as np, os
from pathlib import Path
W,H=64,80
d = str(Path(__file__).resolve().parent / "algo-oracle")
frames=[]
for f in sorted(glob.glob(os.path.join(d,"gdix51c0_frame_*.raw"))):
    a=np.fromfile(f,dtype='<u2')
    if a.size==W*H: frames.append(a.astype(np.float32))
frames=np.array(frames)
# per-pixel responsiveness proxy = dynamic range across captures
rng = np.percentile(frames,90,axis=0)-np.percentile(frames,10,axis=0)
rng = np.clip(rng, 1, None)
ref = np.median(rng)
# gain equalizes response: gain[i] = 8192 * ref/range[i]  (8192 = unity)
gain = np.clip(8192.0*ref/rng, 512, 32767).astype('<u2')
gain.tofile(os.path.join(d,"synth_gain.raw"))
print(f"gain: unity=8192 ref_range={ref:.0f} mean={gain.mean():.0f} range[{gain.min()},{gain.max()}] std={gain.std():.0f}")
print("wrote synth_gain.raw")
