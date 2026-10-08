import subprocess,pathlib,json,urllib.request,time,math,base64,hashlib,resource
import argparse, socket, secrets, tempfile
parser=argparse.ArgumentParser(description='Real pinned CPU model smoke; local authoring test, not a Windows runtime claim.')
parser.add_argument('--text-only',action='store_true');parser.add_argument('--server',required=True);parser.add_argument('--model-directory',required=True)
a=parser.parse_args();exe=pathlib.Path(a.server).resolve();models=pathlib.Path(a.model_directory).resolve()
for name,expected in [('embeddinggemma-2-Q8_0.gguf','2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135'),('mmproj-embeddinggemma-2-Q8_0.gguf','c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f')]:
 with (models/name).open('rb') as f: assert hashlib.file_digest(f,'sha256').hexdigest()==expected
scratch=tempfile.TemporaryDirectory(prefix='ow-embedding-smoke-');root=pathlib.Path(scratch.name)
with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
secret=secrets.token_hex(32)
log=(root/'cpu-smoke.log').open('w')
args=[str(exe),'--model',str(models/'embeddinggemma-2-Q8_0.gguf'),'--mmproj',str(models/'mmproj-embeddinggemma-2-Q8_0.gguf'),'--no-mmproj-offload','--image-max-tokens','256','--mtmd-batch-max-tokens','256','--host','127.0.0.1','--port',str(port),'--api-key',secret,'--embeddings','--pooling','mean','--parallel','1','--ctx-size','2048','--batch-size','2048','--ubatch-size','2048','--threads','1','--threads-batch','1','--n-gpu-layers','0','--no-webui']
if a.text_only:
 for option in ('--mmproj','--image-max-tokens','--mtmd-batch-max-tokens'):
  i=args.index(option);del args[i:i+2]
 args.remove('--no-mmproj-offload')
p=subprocess.Popen(args,stdout=log,stderr=log)
def request(value):
 data=json.dumps({'model':'embeddinggemma-2','input':[value]}).encode()
 req=urllib.request.Request('http://127.0.0.1:'+str(port)+'/v1/embeddings',data,headers={'Content-Type':'application/json','Authorization':'Bearer '+secret})
 with urllib.request.urlopen(req,timeout=90) as r:doc=json.load(r)
 vector=doc['data'][0]['embedding'];assert len(vector)==768 and all(math.isfinite(v) for v in vector)
 norm=sum(v*v for v in vector);assert norm>1e-20

 return [v/math.sqrt(norm) for v in vector]
def sample(kind,color):
 f=root/(kind+'-'+color+('.wav' if kind=='audio' else '.jpg'))
 source='sine=frequency='+('440' if color=='red' else '880')+':duration=10' if kind=='audio' else 'color=c='+color+':s=256x256'
 args=['ffmpeg','-nostdin','-v','error','-y','-f','lavfi','-i',source]
 args+=['-ar','16000','-ac','1','-c:a','pcm_s16le'] if kind=='audio' else ['-frames:v','1','-c:v','mjpeg','-threads','1']
 subprocess.run(args+[str(f)],check=True,timeout=15)
 return base64.b64encode(f.read_bytes()).decode()
try:
 for i in range(120):
  if p.poll() is not None:raise RuntimeError('Model failed: '+(root/'cpu-smoke.log').read_text()[-2500:])
  try:
   with urllib.request.urlopen('http://127.0.0.1:'+str(port)+'/health',timeout=1) as r:
    if r.status==200:break
  except Exception:time.sleep(1)
 else:raise RuntimeError('Model readiness timeout')
 vectors=[]
 for txt in ['title: none | text: A red flower','title: none | text: A passenger train']:
  started=time.monotonic();v=request(txt);vectors.append(v);print('PASS text',len(v),'seconds',round(time.monotonic()-started,3),flush=True)
 assert vectors[0]!=vectors[1]
 if not a.text_only:
  images=[]
  for color in ['red','blue']:
   content={'type':'image_url','image_url':{'url':'data:image/jpeg;base64,'+sample('image',color)}}
   started=time.monotonic();v=request({'content':[content]});images.append(v);print('PASS image',color,len(v),'seconds',round(time.monotonic()-started,3),flush=True)
  assert images[0]!=images[1]
  v=request({'content':[{'type':'image_url','image_url':{'url':'data:image/jpeg;base64,'+sample('image',c)}} for c in ['red','blue','red','blue']]});print('PASS video frame sequence',len(v),flush=True)
  audios=[]
  for color in ['red','blue']:
   v=request({'content':[{'type':'input_audio','input_audio':{'data':sample('audio',color),'format':'wav'}}]});audios.append(v);print('PASS audio',color,len(v),flush=True)
  assert audios[0]!=audios[1]
 print('ALL REAL CPU MODEL PROBES PASSED',flush=True)
except Exception as e:print('MODEL PROBE FAILURE',repr(e),flush=True);raise
finally:
 p.terminate()
 try:p.wait(timeout=10)
 except subprocess.TimeoutExpired:p.kill();p.wait()
 log.close()
 print('Peak child RSS KiB',resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,flush=True)
 scratch.cleanup()
