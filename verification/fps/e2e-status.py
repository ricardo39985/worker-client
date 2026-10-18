"""Read the actual coordinator attempt, route and terminal app job state."""
import json,pathlib,subprocess,sys
job=json.loads(pathlib.Path('verification/fps/e2e-queue.json').read_text())['job_id']
script="""import sys,os,json
sys.path.insert(0,'/src/scripts')
from migrate import connect_database
db=connect_database(os.environ['DATABASE_URL']);db.set_session(readonly=True);c=db.cursor()
c.execute("SELECT j.id::text,j.state,j.last_error,j.payload->'media_route',n.route_state,a.id::text,a.state,a.worker_id::text FROM processing_jobs j LEFT JOIN native_worker_jobs n ON n.job_id=j.id LEFT JOIN native_worker_attempts a ON a.job_id=j.id WHERE j.id=%s ORDER BY a.hard_deadline_ms DESC",('JOB_ID',))
print(json.dumps(c.fetchall(),default=str));db.rollback();db.close()
""".replace('JOB_ID',job)
r=subprocess.run(['ssh','-o','BatchMode=yes','-o','ConnectTimeout=10','-i','/home/rick/Downloads/LightsailDefaultKey-us-east-1.pem','admin@18.235.13.236','sudo -n docker exec -i organizer-prod-pairing-worker-1 python3 -'],input=script,text=True,capture_output=True)
pathlib.Path('verification/fps/e2e-status.json').write_text(r.stdout);print(r.stdout);print('E2E_STATUS_READ_EXIT='+str(r.returncode))
if r.returncode:print(r.stderr)
sys.exit(r.returncode)
