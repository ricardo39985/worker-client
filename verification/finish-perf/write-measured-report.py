import json,re
from pathlib import Path
out=Path(__file__).resolve().parent
log=(out/'13-measurements.log').read_text()
assert 'PERFORMANCE_GATE_EXIT=0' in log
assert log.count('FULL_PARITY ')==36
assert len(re.findall(r'QUALITY_EXIT=.*?/0',log))==4
s=json.loads((out/'fresh-results.json').read_text())['summary']
for size in ('1080p','4k'):
 for mode in ('baseline-normal','fused-normal'):assert s[size][mode]['repeats']==3
text="""## Fresh paired performance after merging main

The retained CUDA decode and inspected-frame poster reuse both improve measured
work. No additional runtime optimization was introduced or reverted here. The
full two-output shared-decode experiment stays excluded: its earlier roughly
1.022x single-job gain does not justify two simultaneous sessions when admission
reserves one sequential session per physical card. Nothing bypasses that budget.

Fresh measurements used the merged `0771b79` native measurement binary on Danny's
mapped RTX 3050, CUDA ordinal 1. Final `f56952e` changes reporting only; the media
factory, timing, process/resource code and measurement tool are unchanged.
Generated H.264 SDR 900-frame / 30-second inputs at 30 fps, AAC, 1080p and 4K;
NVENC p4/VBR/CQ24 and full dimensions before/after. Three repetitions, medians.
Normal priority, 1 configured CPU (5% on the 20-logical-CPU host), 2048 MiB offline
fixture, 100 ms poll. This fixture does not replace runtime workload-sized RAM
reservations. Full software output validation is included. Source generation and
quality analysis are outside job totals; download/upload/coordinator latency is
unmeasured. Caches were not flushed and external load was not controlled.

Every complete job produces H.264, HEVC and the poster. Wall/CPU seconds include
all timed computation, input/output hashing, inspection and poster. Stage medians
need not sum to the median total. CPU is controller plus drained child-tree CPU.

| Size | Job wall before -> after (s) | Job CPU before -> after (s) | Actual wall multiplier | CPU multiplier |
|---|---:|---:|---:|---:|
"""
for size in ('1080p','4k'):
 b,a=(s[size][m] for m in ('baseline-normal','fused-normal'))
 text+=f"| {size} | {b['wall_s']:.3f} -> {a['wall_s']:.3f} | {b['cpu_s']:.3f} -> {a['cpu_s']:.3f} | {b['wall_s']/a['wall_s']:.3f}x | {b['cpu_s']/a['cpu_s']:.3f}x |\n"
text+='\n| Size / stage | Wall before -> after (s) | CPU before -> after (s) | Wall multiplier |\n|---|---:|---:|---:|\n'
def sumstage(row,keys):return {v:sum(row['stages'][k][v] for k in keys) for v in ('wall_s','cpu_s')}
for size in ('1080p','4k'):
 b,a=(s[size][m] for m in ('baseline-normal','fused-normal'))
 rows=[('input SHA-256',['input_hash/source.bin'],['input_hash/source.bin']),('inspection + poster',['input_inspection/source.bin','poster/poster.webp'],['inspection_poster/source.bin'])]
 for codec,file in [('H.264','feed.mp4'),('HEVC','optimized.mp4')]:
  for stage in ('transcode','validation'):rows.append((codec+' '+stage,[stage+'/'+file],[stage+'/'+file]))
  rows.append((codec+' compute subtotal',['transcode/'+file,'validation/'+file],['transcode/'+file,'validation/'+file]))
 rows.append(('all output SHA-256',['output_hash/'+f for f in ('feed.mp4','optimized.mp4','poster.webp')],['output_hash/'+f for f in ('feed.mp4','optimized.mp4','poster.webp')]))
 for label,bkeys,akeys in rows:
  before,after=sumstage(b,bkeys),sumstage(a,akeys)
  text+=f"| {size} / {label} | {before['wall_s']:.3f} -> {after['wall_s']:.3f} | {before['cpu_s']:.3f} -> {after['cpu_s']:.3f} | {before['wall_s']/after['wall_s']:.3f}x |\n"
text+="""
Validation and hashing have no optimization applied; small differences there are
run-to-run variation, not a claimed gain. Both full software validations remain
mandatory. The offline tool permits fifteen minutes to measure cost; the runtime
30-second validation guard is unchanged. Earlier Low-priority 4K checks exceed
that guard and are not accepted-runtime claims. Historical Low results, separate
seven-pair poster measurements and source-profile parity are in
[MEASURED.md](../gpu-performance/MEASURED.md).

SSIM and PSNR filters actually ran against the before output for both codecs at
both sizes. All four report **SSIM 1.000000, PSNR infinity**; every measured
complete-job artifact also matches its original reference SHA-256 (36 checks).
All 900 frames, dimensions, timing and audio bytes therefore remain identical.
Throughput inputs are generated H.264; earlier eight short H.264/HEVC input/output
parity cases do not establish full-duration HEVC-input throughput. These are
synthetic offline computation results, not a coordinator or network-job speedup.

Raw [measurement and quality log](13-measurements.log), [stage data and ranges](fresh-results.json).

## Landing and installation

Pending in this running ledger. Do not infer success from prepared scripts.
"""
p=out/'REPORT.md';old=p.read_text();old=old[:old.index('## Fresh measurements, landing and installation')];p.write_text(old+text)
