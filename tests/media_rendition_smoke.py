"""Run the actual installed command profile against synthetic media only."""
import argparse
import json
import pathlib
import shutil
import subprocess
import tempfile

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--probe',type=pathlib.Path,required=True);args=p.parse_args()
    for name in ('ffmpeg','ffprobe'):
        if not shutil.which(name):p.error(name+' must already be installed; this test does not bootstrap tools')
    def run(command):return subprocess.run(command,check=True,timeout=30,capture_output=True).stdout
    with tempfile.TemporaryDirectory(prefix='ow-rendition-test-') as temporary:
        root=pathlib.Path(temporary)
        for mode in ('video','image'):
            workspace=root/mode;workspace.mkdir();source=workspace/('source.mp4' if mode=='video' else 'source.jpg')
            cmd=['ffmpeg','-nostdin','-v','error','-f','lavfi','-i','color=c=blue:s=128x72:r=12']
            cmd+=['-t','1','-an','-c:v','libx264','-pix_fmt','yuv420p'] if mode=='video' else ['-frames:v','1']
            run(cmd+['-threads','1',str(source)])
            shutil.copyfile(source,workspace/'source.bin')
            for line in run([str(args.probe.resolve()),mode,'encode',str(workspace)]).decode().splitlines():
                role,mime,filename,*arguments=line.split('\t')
                run(['ffmpeg',*arguments])
                doc=json.loads(run(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=codec_name,width,height,r_frame_rate:format=duration','-of','json',str(workspace/filename)]))
                stream=doc['streams'][0]
                expected='webp' if role=='thumbnail' or mode=='image' else 'hevc' if role=='feed_optimized' else 'h264'
                assert stream['codec_name']==expected
                assert (stream['width'],stream['height'])==(128,72)
                if mode=='video' and role!='thumbnail':
                    assert stream['r_frame_rate']=='12/1'
                    assert abs(float(doc['format']['duration'])-1)<=.1
                run(['ffmpeg','-nostdin','-v','error','-xerror','-threads','1','-i',str(workspace/filename),'-threads','1','-f','null','-'])
    print('Synthetic full-size H.264, HEVC and WebP profiles decoded successfully')

if __name__=='__main__':main()
