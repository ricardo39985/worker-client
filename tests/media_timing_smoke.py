"""Real-media contract for rendition timing; uses generated pixels only.

The runtime validator uses the approved FFmpeg (not FFprobe). FFprobe here is
an independent test oracle, including decoded frames, not just packet counts.
"""
import argparse
import json
import pathlib
import subprocess
from fractions import Fraction

p = argparse.ArgumentParser()
p.add_argument('--probe', required=True)
p.add_argument('--workspace', required=True)
a = p.parse_args()
root = pathlib.Path(a.workspace)
root.mkdir(exist_ok=True)


def run(cmd):
    return subprocess.check_output(cmd, stderr=subprocess.PIPE, timeout=60)


def evidence(file):
    return subprocess.run(
        ['ffmpeg', '-nostdin', '-hide_banner', '-v', 'info', '-i', str(file),
         '-map', '0:v:0', '-an', '-c:v', 'copy', '-f', 'framehash',
         '-hash', 'adler32', '-'], stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=True, timeout=60).stdout


def timeline(file):
    d = json.loads(run([
        'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_frames',
        '-show_entries', 'stream=r_frame_rate,avg_frame_rate,time_base,duration:'
        'frame=best_effort_timestamp', '-of', 'json', str(file)]))
    stream = d['streams'][0]
    tb = Fraction(stream['time_base'])
    frames = [int(x['best_effort_timestamp']) * tb for x in d['frames']]
    first = frames[0]
    packets = json.loads(run([
        'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_packets',
        '-show_entries', 'packet=pts,duration', '-of', 'json', str(file)]))
    # Demuxed packet durations belong to decode-order sample tables. Do not
    # mistake the duration on the greatest PTS for the final display hold.
    duration = sum(int(x['duration']) for x in packets['packets']) * tb
    return stream, [pts - first for pts in frames], duration


for name, rate, count in [('integer', '30', 90),
                          ('fractional', '30000/1001', 90),
                          ('vfr', '30', 90),
                          ('rounded-vfr', '30', 717),
                          ('single-frame', '30000/1001', 1)]:
    work = root / name
    work.mkdir(exist_ok=True)
    source = work / 'source.bin'
    args = ['ffmpeg', '-nostdin', '-v', 'error', '-y', '-f', 'lavfi', '-i',
            'testsrc2=size=128x72:rate=' + rate, '-frames:v', str(count)]
    if name == 'vfr':
        args += ['-vf', r'settb=1/90000,setpts=N*3000+if(eq(mod(N\,7)\,0)\,1\,0)',
                 '-enc_time_base:v', '1:90000', '-fps_mode', 'passthrough']
    if name == 'rounded-vfr':
        # Same count, 30k time base, 1000/1151/1152 sample-interval distribution
        # and rounded 29.92 tbr as the refused upload. No personal media fixture.
        args += ['-vf', 'settb=1/30000,setpts=N*1000+round(floor(N/63)*1668/11)',
                 '-enc_time_base:v', '1:30000', '-fps_mode', 'passthrough']
    run(args + ['-c:v', 'libx264', '-threads', '1', '-f', 'mp4', str(source)])
    expected, pts, duration = timeline(source)
    source_log = work / 'timing.log'
    source_log.write_bytes(evidence(source))
    if name == 'rounded-vfr':
        assert expected['r_frame_rate'] == '359/12', expected
        assert b'29.92 tbr' in source_log.read_bytes()
    for line in run([a.probe, 'video', 'encode', str(work)]).decode().splitlines():
        role, mime, file, *args = line.split('\t')
        if role == 'thumbnail':
            continue
        run(['ffmpeg', *args])
        output = work / file
        actual, out, out_duration = timeline(output)
        print(name, role, 'source=', expected, 'output=', actual, flush=True)
        # Nominal/VUI and container-average rates may legitimately differ.
        assert out == pts, 'Dropped, duplicated or retimed decoded frames'
        assert out_duration == duration, 'Total video sample duration changed'
        output_log = work / (file + '.timing.log')
        output_log.write_bytes(evidence(output))
        run([a.probe, 'validate', str(source_log), str(output_log)])
        run(['ffmpeg', '-nostdin', '-v', 'error', '-xerror', '-i', str(output),
             '-f', 'null', '-'])
print('Frame timing contracts passed')
