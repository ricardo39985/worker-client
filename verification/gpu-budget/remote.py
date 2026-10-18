import subprocess,base64,sys
from pathlib import Path
script=Path(sys.argv[2]).read_text()
bootstrap="$s=[Console]::In.ReadToEnd(); & ([scriptblock]::Create($s))"
command='powershell.exe -NoLogo -NoProfile -NonInteractive -EncodedCommand '+base64.b64encode(bootstrap.encode('utf-16le')).decode()
p=subprocess.run(['ssh','-o','BatchMode=yes','-o','ConnectTimeout=10',sys.argv[1],command],input=script,text=True)
print('SSH_EXIT='+str(p.returncode),flush=True)
sys.exit(p.returncode)
