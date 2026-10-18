import subprocess,json,os,sys
from pathlib import Path
root=Path.cwd(); out=root/'verification/finish-perf'
results=[]
def run(name,args,env=None):
 with (out/(name+'.log')).open('w') as log:
  code=subprocess.run(args,cwd=root,stdout=log,stderr=subprocess.STDOUT,env=env).returncode
 results.append({'gate':name,'exit':code,'args':args})
 (out/'linux-results.json').write_text(json.dumps(results,indent=2))
 print(name+' EXIT='+str(code),flush=True)
 return code
boost='/home/rick/.hermes/cache/scratch/boost-extract'
for mode in ['linux','sanitize']:
 build=root/('build-tests' if mode=='linux' else 'build-sanitize')
 args=['cmake','-S',str(root),'-B',str(build),'-DBUILD_TESTING=ON','-DOW_BOOST_ROOT='+boost,'-DCMAKE_BUILD_TYPE='+('Release' if mode=='linux' else 'Debug')]
 if mode=='sanitize': args+=['-DOW_SANITIZE=ON','-DOW_ENABLE_IPO=OFF','-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer','-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
 if run(mode+'-configure',args): sys.exit(1)
 if run(mode+'-build',['cmake','--build',str(build),'--parallel','2']): sys.exit(1)
 inventory=subprocess.run(['ctest','--test-dir',str(build),'--show-only=json-v1'],capture_output=True,text=True)
 (out/(mode+'-discovery.json')).write_text(inventory.stdout)
 results.append({'gate':mode+'-discovery','exit':inventory.returncode})
 (out/'linux-results.json').write_text(json.dumps(results,indent=2))
 if inventory.returncode: sys.exit(inventory.returncode)
 env=os.environ.copy()
 if mode=='sanitize': env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
 for suite in json.loads(inventory.stdout)['tests']:
  name=suite['name'];code=run(mode+'-'+name,['ctest','--test-dir',str(build),'-R','^'+name+'$','-V','--output-junit',str(out/(mode+'-'+name+'.xml'))],env)
  if code: sys.exit(code)
 if any(r['exit'] for r in results): sys.exit(1)
sys.exit(0)
