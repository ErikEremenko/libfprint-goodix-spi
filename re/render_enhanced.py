import subprocess, os, glob, numpy as np
from pathlib import Path
from PIL import Image
W,H=64,80
d = str(Path(__file__).resolve().parent / "algo-oracle")
out_dir = str(Path(__file__).resolve().parent / "enhanced_png")
os.makedirs(out_dir, exist_ok=True)
env=dict(os.environ, WINEDEBUG="-all", _ZO_DOCTOR="0")

# pick a spread of frames (by id)
ids=["159395043721","159351312191","159354165143","159383139258",
     "159353166503","159422600222","159354886408","159370973775"]
C="900"
tiles=[]
for fid in ids:
    frame=f"gdix51c0_frame_{fid}.raw"
    # loader.exe <frame> <baseline.raw> <gain_uniform=0> <C>
    subprocess.run(["wine","loader.exe",frame,"synth_min.raw","0",C],
                   cwd=d, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    enh=os.path.join(d,"preproc_out.raw")
    if not os.path.exists(enh):
        print("no output for",fid); continue
    a=np.fromfile(enh,dtype=np.uint8)
    if a.size!=W*H:
        print("bad size",fid,a.size); continue
    img=a.reshape(H,W)
    # raw capture too (normalized) for side-by-side
    rawa=np.fromfile(os.path.join(d,frame),dtype='<u2').astype(np.float32).reshape(H,W)
    lo,hi=np.percentile(rawa,1),np.percentile(rawa,99)
    rawn=np.clip((rawa-lo)/max(hi-lo,1)*255,0,255).astype(np.uint8)
    # save individual enhanced png (8x scale)
    Image.fromarray(img,'L').resize((W*6,H*6),Image.NEAREST).save(os.path.join(out_dir,f"enh_{fid}.png"))
    # build a raw|enhanced tile
    gap=np.full((H,4),128,np.uint8)
    tile=np.hstack([rawn,gap,img])
    tiles.append((fid,tile))
    print(f"{fid}: enhanced min={img.min()} max={img.max()} mean={img.mean():.0f} nz={np.count_nonzero(img)}/{img.size}")

# montage: rows of (raw | enhanced), scaled
if tiles:
    rows=[]
    for fid,t in tiles:
        big=Image.fromarray(t,'L').resize((t.shape[1]*5,H*5),Image.NEAREST)
        rows.append(np.array(big))
    hgap=np.full((8,rows[0].shape[1]),200,np.uint8)
    stacked=[]
    for r in rows: stacked.extend([r,hgap])
    montage=np.vstack(stacked[:-1])
    Image.fromarray(montage,'L').save(os.path.join(out_dir,"montage_raw_vs_enhanced.png"))
    print("wrote montage_raw_vs_enhanced.png  (left=raw capture, right=enhanced)")
print("out dir:", out_dir)
