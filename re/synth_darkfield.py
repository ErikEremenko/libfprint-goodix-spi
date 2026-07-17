import glob, numpy as np, os
from pathlib import Path
from PIL import Image

W,H = 64,80
d = str(Path(__file__).resolve().parent / "algo-oracle")
files = sorted(glob.glob(os.path.join(d,"gdix51c0_frame_*.raw")))
frames = []
for f in files:
    a = np.fromfile(f, dtype='<u2')
    if a.size != W*H: continue
    frames.append(a.astype(np.float32))
frames = np.array(frames)   # (N, 5120)
print("frames:", frames.shape)

pmin  = frames.min(axis=0)
p10   = np.percentile(frames, 10, axis=0)
pmed  = np.percentile(frames, 50, axis=0)
pmax  = frames.max(axis=0)
print(f"per-pixel min:  mean={pmin.mean():.0f} range[{pmin.min():.0f},{pmin.max():.0f}]")
print(f"per-pixel p10:  mean={p10.mean():.0f} range[{p10.min():.0f},{p10.max():.0f}]")
print(f"per-pixel med:  mean={pmed.mean():.0f} range[{pmed.min():.0f},{pmed.max():.0f}]")
print(f"per-pixel max:  mean={pmax.mean():.0f} range[{pmax.min():.0f},{pmax.max():.0f}]")

# fixed-pattern spread of the estimated dark field (how much per-pixel structure)
print(f"dark-field(min) spatial std = {pmin.std():.1f}  (fixed-pattern noise)")

# save synthetic dark-field candidates as 16bpp LE raw
for name, arr in [("synth_min",pmin),("synth_p10",p10),("synth_med",pmed)]:
    out = np.clip(arr,0,65535).astype('<u2')
    out.tofile(os.path.join(d, name+".raw"))
    print("wrote", name+".raw")

# render the dark-field(min) as a normalized PNG for inspection
def norm_png(arr, path):
    a = arr.reshape(H,W)
    lo,hi = np.percentile(a,1), np.percentile(a,99)
    a = np.clip((a-lo)/max(hi-lo,1)*255,0,255).astype(np.uint8)
    Image.fromarray(a,'L').resize((W*4,H*4),Image.NEAREST).save(path)
norm_png(pmin, os.path.join(d,"synth_darkfield.png"))
print("wrote synth_darkfield.png")
