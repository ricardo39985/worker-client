"""Queue one authorized app job for the failed asset, only if nothing is replaced."""
import json,pathlib,subprocess,sys
assert not pathlib.Path('verification/fps/e2e-queue.json').exists(), 'The authorized single app job has already been queued'
script="""import sys,os,json
sys.path.insert(0,'/src/scripts')
from migrate import connect_database
asset='00280181-051b-4823-83ff-02ffb9ef38f8'
db=connect_database(os.environ['DATABASE_URL'])
try:
 c=db.cursor()
 c.execute("SELECT i.id FROM items i JOIN item_assets a ON a.item_id=i.id WHERE a.id=%s AND a.role='primary' AND a.state='ready' AND i.deleted_at IS NULL FOR UPDATE OF i,a",(asset,))
 rows=c.fetchall()
 if len(rows)!=1:raise ValueError('Approved source no longer ready')
 item=rows[0][0]
 c.execute('SELECT count(*) FROM item_assets WHERE source_asset_id=%s',(asset,))
 if c.fetchone()[0]:raise ValueError('Existing renditions would be replaced; refusing')
 c.execute("SELECT count(*) FROM processing_jobs WHERE item_id=%s AND kind='optimize_media' AND state IN ('queued','processing') AND payload->>'asset_id'=%s",(item,asset))
 if c.fetchone()[0]:raise ValueError('An existing app job is active; refusing duplicate')
 c.execute("INSERT INTO processing_jobs(item_id,kind,payload) VALUES(%s,'optimize_media',jsonb_build_object('asset_id',%s::text)) RETURNING id",(item,asset))
 job=str(c.fetchone()[0]);db.commit()
 print(json.dumps({'job_id':job,'source_asset_id':asset,'existing_renditions':0,'queued_app_jobs':1}))
except BaseException:
 db.rollback();raise
finally:db.close()
"""
cmd=['ssh','-o','BatchMode=yes','-o','ConnectTimeout=10','-i','/home/rick/Downloads/LightsailDefaultKey-us-east-1.pem','admin@18.235.13.236','sudo -n docker exec -i organizer-prod-pairing-worker-1 python3 -']
r=subprocess.run(cmd,input=script,text=True,capture_output=True)
pathlib.Path('verification/fps/e2e-queue.json').write_text(r.stdout)
print(r.stdout);print('E2E_QUEUE_EXIT='+str(r.returncode))
if r.returncode:print(r.stderr)
sys.exit(r.returncode)
