"""Behavior contract: actual rendition commands preserve frame timing."""
import argparse,json,pathlib,subprocess,shutil
from fractions import Fraction
p=argparse.ArgumentParser();p.add_argument('--probe',required=True);p.add_argument('--workspace',required=True);a=p.parse_args()
root=pathlib.Path(a.workspace);root.mkdir(exist_ok=True)
def run(cmd):return subprocess.check_output(cmd,stderr=subprocess.PIPE,timeout=60)
def timing(file):
 d=json.loads(run(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=r_frame_rate,avg_frame_rate,nb_frames:frame=best_effort_timestamp_time','-show_frames','-of','json',str(file)]))
 return d['streams'][0],[Fraction(x['best_effort_timestamp_time']) for x in d['frames']]
for name,rate in [('integer','30'),('fractional','30000/1001'),('vfr','30')]:
 work=root/name;work.mkdir(exist_ok=True);source=work/'source.bin'
 args=['ffmpeg','-nostdin','-v','error','-y','-f','lavfi','-i','testsrc2=size=128x72:rate='+rate,'-t','3']
 if name=='vfr':args += ['-vf',"settb=1/90000,setpts=N*3000+if(eq(mod(N\\,7)\\,0)\\,1\\,0)",'-enc_time_base:v','1:90000','-fps_mode','passthrough']
 run(args+['-c:v','libx264','-threads','1','-f','mp4',str(source)])
 expected,pts=timing(source)
 evidence=subprocess.run(['ffmpeg','-nostdin','-hide_banner','-v','info','-i',str(source),'-map','0:v:0','-an','-c:v','copy','-f','framehash','-hash','adler32','-'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True).stdout
 (work/'timing.log').write_bytes(evidence)
 for line in run([a.probe,'video','encode',str(work)]).decode().splitlines():
  role,mime,file,*args=line.split('\t')
  if role=='thumbnail':continue
  run(['ffmpeg',*args]);actual,out=timing(work/file)
  print(name,role,'source=',expected,'output=',actual,flush=True)
  assert Fraction(actual['r_frame_rate'])==Fraction(expected['r_frame_rate']), 'Mismatched output rate must be rejected'
  assert Fraction(actual['avg_frame_rate'])==Fraction(expected['avg_frame_rate']), 'Container average rate changed'
  assert out==pts, 'Dropped, duplicated or retimed frames'
print('Frame timing contracts passed')
