import json,subprocess,sys,pathlib
from fractions import Fraction
root=pathlib.Path(sys.argv[1]);rows=[]
def probe(file):
 d=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=r_frame_rate,avg_frame_rate,time_base,nb_frames,width,height:stream_side_data=rotation:packet=pts','-show_packets','-of','json',str(file)]))
 stream=d['streams'][0];tb=Fraction(stream['time_base']);pts=sorted(int(x['pts'])*tb for x in d['packets']);return stream,pts
for work in sorted(root.glob('after-*')):
 if not work.is_dir():continue
 name=work.name.removeprefix('after-').split('-nvenc')[0]
 source=root/({'rejected':'rejected-source.mp4','rejected2':'rejected2-source.mp4','real3':'real-source.mp4'}.get(name,name+'.mp4'))
 expected,pts=probe(source)
 for output in ['feed.mp4','optimized.mp4']:
  actual,frames=probe(work/output)
  equal=frames==pts
  row={'sample':name,'mode':work.name,'output':output,'source':expected,'result':actual,'frames':len(frames),'exact_pts':equal}
  rows.append(row)
  assert Fraction(expected['r_frame_rate'])==Fraction(actual['r_frame_rate']),row
  assert equal,row
  assert (expected['width'],expected['height'])==(actual['width'],actual['height']),row
  assert expected.get('side_data_list',[])==actual.get('side_data_list',[]),row
out=pathlib.Path('verification/fps/after-rates.json');out.write_text(json.dumps(rows,indent=2))
print('FFPROBE_RATE_AND_TIMESTAMP_CHECK_EXIT=0 OUTPUTS='+str(len(rows)))
for r in rows:print(r['sample'],r['mode'],r['output'],r['source']['r_frame_rate']+' -> '+r['result']['r_frame_rate'],'FRAMES='+str(r['frames']),'EXACT_PTS='+str(r['exact_pts']))
