"""Summarize native experiment stdout; input clips and outputs stay off Git."""
import json
import re
import statistics
import sys
from pathlib import Path

jobs = []
quality = []
job = None
for line in Path(sys.argv[1]).read_text().splitlines():
    if line.startswith("JOB "):
        job = dict(re.findall(r"(\w+)=(\S+)", line))
        job["stages"] = []
    elif line.startswith("TIMING stage=") and job is not None:
        sample = dict(re.findall(r"(\w+)=(\S+)", line))
        for key in ("wall_us", "controller_cpu_us", "child_cpu_us", "launch_us", "root_lifetime_us"):
            sample[key] = int(sample[key]) if sample[key] != "unavailable" else None
        job["stages"].append(sample)
    elif line.startswith("TIMING total ") and job is not None:
        totals = dict(re.findall(r"(\w+)=(\S+)", line))
        job["wall_s"] = int(totals["job_wall_us"]) / 1e6
        job["cpu_s"] = int(totals["job_cpu_us"]) / 1e6
        jobs.append(job)
        job = None
    elif line.startswith(("QUALITY ", "PARITY ", "FULL_PARITY ", "SOURCE_QUALITY ")):
        quality.append(line)

summary = {}
for size in ("1080p", "4k"):
    summary[size] = {}
    for mode in ("baseline", "device", "shared", "baseline-full", "fused-full", "baseline-normal", "fused-normal", "poster-separate", "poster-fused"):
        samples = [j for j in jobs if j["size"] == size and j["mode"] == mode]
        if not samples:
            continue
        row = {"repeats": len(samples)}
        for key in ("wall_s", "cpu_s"):
            row[key] = statistics.median(j[key] for j in samples)
            row[key + "_range"] = [min(j[key] for j in samples), max(j[key] for j in samples)]
        row["stages"] = {}
        for stage, artifact in sorted({(s["stage"], s["artifact"]) for j in samples for s in j["stages"]}):
            stages = [s for j in samples for s in j["stages"] if (s["stage"], s["artifact"]) == (stage, artifact)]
            row["stages"][stage + "/" + artifact] = {
                "wall_s": statistics.median(s["wall_us"] / 1e6 for s in stages),
                "cpu_s": statistics.median((s["child_cpu_us"] + s["controller_cpu_us"]) / 1e6 for s in stages),
                "launch_ms": statistics.median(s["launch_us"] / 1e3 for s in stages),
                "poll_drain_ms": statistics.median((s["wall_us"] - s["root_lifetime_us"]) / 1e3 for s in stages),
            }
        summary[size][mode] = row
    if "baseline" in summary[size]:
        before = summary[size]["baseline"]
        for mode, after in summary[size].items():
            reference = summary[size].get("baseline-full", before) if mode.endswith("-full") else summary[size].get("baseline-normal", before) if mode.endswith("-normal") else before
            if mode.startswith("poster-"):
                after["preparation_multiplier"] = summary[size]["poster-separate"]["wall_s"] / after["wall_s"]
                continue
            after["job_multiplier"] = reference["wall_s"] / after["wall_s"]
            if mode == "shared":
                continue
            for artifact in ("feed.mp4", "optimized.mp4"):
                keys = [stage + "/" + artifact for stage in ("transcode", "validation")]
                after[artifact + "_compute_multiplier"] = sum(reference["stages"][key]["wall_s"] for key in keys) / sum(after["stages"][key]["wall_s"] for key in keys)
print(json.dumps({"summary": summary, "jobs": jobs, "quality": quality}, indent=2))
