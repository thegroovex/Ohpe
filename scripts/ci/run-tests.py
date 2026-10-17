#!/usr/bin/env python3
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Runs the CTest tests of a build tree in parallel, splitting doctest binaries per test case.

CTest runs each registered test as one process, so one long doctest binary (core_tests,
plugin_tests) keeps a single core busy for many minutes. This runner reads the tests from CTest
(command, environment, working directory - so a test added with add_test() is picked up without
touching CI), asks every doctest binary named with --split for its number of test cases and runs
each test case as its own process (--first=i --last=i) on a pool of workers. Every split process
must report exactly one executed test case, and every test case of every split binary must run,
so the coverage is exactly that of a plain `ctest` run.

Test cases tagged doctest::test_suite ("sliced") (see tests/TestSlice.h) loop over many independent
items; they run --slices times, each process with OHPE_TEST_SLICE=k/slices doing every slices-th item.

    scripts/ci/run-tests.py --build-dir build [-C Release] [-j N] [--split core_tests,plugin_tests]
                            [-R regex] [-E regex] [--timings file.json]

Each worker gets its own HOME / TMPDIR (TEMP / TMP on Windows), so test cases that run at the same
time do not share settings or temporary files. Exit code 0 only when everything passed.
"""

import argparse
import concurrent.futures as cf
import json
import os
import re
import subprocess
import sys
import tempfile
import threading
import time

GH = os.environ.get("GITHUB_ACTIONS") == "true"


def ctest_tests(build_dir, config):
    out = subprocess.run(["ctest", "--test-dir", build_dir, "-C", config, "--show-only=json-v1"],
                         check=True, capture_output=True, text=True).stdout
    data = json.loads(out)
    tests = []
    for t in data.get("tests", []):
        props = {p["name"]: p.get("value") for p in t.get("properties", [])}
        if "command" not in t or not t["command"]:
            raise SystemExit(f"error: test {t['name']} has no command for configuration {config}")
        env = {}
        for kv in props.get("ENVIRONMENT") or []:
            k, _, v = kv.partition("=")
            env[k] = v
        if props.get("DISABLED"):
            print(f"note: {t['name']} is disabled in CTest - skipped as ctest would")
            continue
        tests.append({"name": t["name"], "command": t["command"], "env": env,
                      "cwd": props.get("WORKING_DIRECTORY") or build_dir})
    return tests


def run_capture(cmd, env, cwd, timeout):
    p = subprocess.run(cmd, env=env, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       timeout=timeout)
    return p.returncode, p.stdout.decode("utf-8", "replace")


def listed(out):
    """Test case names from doctest --list-test-cases: the block between the first two rules."""
    blocks = out.split("=" * 79)
    if len(blocks) < 3:
        return []
    return [l.rstrip("\r") for l in blocks[1].splitlines() if l.strip() and not l.startswith("[doctest]")]


def doctest_cases(test, env):
    rc, out = run_capture(test["command"] + ["--count"], env, test["cwd"], 600)
    m = re.search(r"passing the current filters:\s*(\d+)", out)
    if rc != 0 or not m:
        raise SystemExit(f"error: {test['name']} does not answer doctest --count (rc {rc}):\n{out}")
    count = int(m.group(1))
    rc, out = run_capture(test["command"] + ["--list-test-cases"], env, test["cwd"], 600)
    names = listed(out)
    if len(names) != count:
        names = [""] * count  # names only label the log
    sliced = set()
    rc, out = run_capture(test["command"] + ["--list-test-cases", "--test-suite=sliced"], env, test["cwd"], 600)
    if rc == 0:
        sliced = set(listed(out))
    return count, names, sliced


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("-C", "--config", default="Release")
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--split", default="", help="comma-separated CTest names of doctest binaries")
    ap.add_argument("-R", "--include", default="")
    ap.add_argument("-E", "--exclude", default="")
    ap.add_argument("--slices", type=int, default=8, help="processes per test case of the 'sliced' suite")
    ap.add_argument("--shard", default="", help="i/n: run only every n-th process, starting at i (several CI runners)")
    ap.add_argument("--timeout", type=int, default=3600, help="seconds per process")
    ap.add_argument("--timings", default="", help="json file: read to schedule long test cases first, rewritten after the run")
    args = ap.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    split = {s for s in args.split.split(",") if s}
    tests = ctest_tests(build_dir, args.config)
    if args.include:
        tests = [t for t in tests if re.search(args.include, t["name"])]
    if args.exclude:
        tests = [t for t in tests if not re.search(args.exclude, t["name"])]
    if not tests:
        raise SystemExit("error: no tests selected")
    missing = split - {t["name"] for t in tests}
    if missing:
        raise SystemExit(f"error: --split names unknown tests: {sorted(missing)}")

    base_env = dict(os.environ)
    jobs = []  # (key, test, index or None, label, slice "k/n" or None)
    expected = {}
    for t in tests:
        env = dict(base_env, **t["env"])
        if t["name"] in split:
            count, names, sliced = doctest_cases(t, env)
            if count == 0:
                raise SystemExit(f"error: {t['name']} has no test cases")
            n = 0
            for i in range(1, count + 1):
                if names[i - 1] and names[i - 1] in sliced and args.slices > 1:
                    for k in range(args.slices):
                        jobs.append((f"{t['name']}#{i}.{k}", t, i, names[i - 1], f"{k}/{args.slices}"))
                        n += 1
                else:
                    jobs.append((f"{t['name']}#{i}", t, i, names[i - 1], None))
                    n += 1
            expected[t["name"]] = n
            if sliced:
                print(f"{t['name']}: {len(sliced)} test cases in {args.slices} slices each", flush=True)
        else:
            jobs.append((t["name"], t, None, "", None))

    expected_total = len(jobs)
    if args.shard:
        # The full process list is built the same way on every runner (same binaries, same
        # arguments), so taking every n-th entry gives each process to exactly one shard.
        si, sn = (int(x) for x in args.shard.split("/"))
        if not (0 <= si < sn):
            raise SystemExit(f"error: bad --shard {args.shard}")
        jobs = [j for pos, j in enumerate(jobs) if pos % sn == si]
        print(f"shard {si}/{sn}: {len(jobs)} of {expected_total} processes", flush=True)
        for name in expected:
            expected[name] = sum(1 for j in jobs if j[1]["name"] == name)
        if not jobs:
            print("nothing to run in this shard")
            return 0

    timings = {}
    if args.timings and os.path.exists(args.timings):
        try:
            with open(args.timings) as f:
                timings = json.load(f)
        except Exception:
            timings = {}
    # longest first (known timings), unknown ones keep their order after the known long ones
    def tkey(j):  # timings follow the test case name (indices move when cases are added)
        return f"{j[1]['name']}:{j[3]}{'@' + j[4] if j[4] else ''}" if j[3] else j[0]
    jobs.sort(key=lambda j: -timings.get(tkey(j), 0.0))

    total_cases = sum(expected.values())
    print(f"{len(tests)} CTest tests, {len(jobs)} processes "
          f"({total_cases} of them from splitting {', '.join(sorted(split)) or '-'}), {args.jobs} workers", flush=True)

    slots = list(range(args.jobs))
    slot_lock = threading.Lock()
    root = tempfile.mkdtemp(prefix="ohpe-tests-")
    print_lock = threading.Lock()
    results = []

    def run(job):
        key, t, idx, label, sl = job
        with slot_lock:
            slot = slots.pop()
        try:
            home = os.path.join(root, f"w{slot}")
            tmp = os.path.join(home, "tmp")
            os.makedirs(tmp, exist_ok=True)
            env = dict(base_env, **t["env"])
            if sl:
                env["OHPE_TEST_SLICE"] = sl
            if os.name == "nt":
                env["TEMP"] = env["TMP"] = tmp
            else:
                env["HOME"] = home
                env["TMPDIR"] = tmp
            cmd = list(t["command"])
            if idx is not None:
                # doctest's summary counts every case passing the filters, not the executed range;
                # --duration=true prints one "<seconds> s: <name>" line per executed test case
                cmd += [f"--first={idx}", f"--last={idx}", "--duration=true"]
            start = time.time()
            try:
                rc, out = run_capture(cmd, env, t["cwd"], args.timeout)
            except subprocess.TimeoutExpired:
                rc, out = -999, f"TIMEOUT after {args.timeout} s"
            dur = time.time() - start
            ok = rc == 0
            note = ""
            if ok and idx is not None:
                executed = re.findall(r"^\d+(?:\.\d+)? s: (.*)$", out, re.M)
                ran = len(executed)
                if ran == 1 and not label:
                    label = executed[0].strip()
                if ran != 1:
                    ok, note = False, f" (expected exactly 1 executed test case, got {ran})"
            res = (key, label, ok, dur, rc, out, note, tkey(job))
            with print_lock:
                status = "PASS" if ok else "FAIL"
                print(f"{status} {key:<18} {dur:7.1f} s  {label}{note}", flush=True)
                if ok:  # p37: measurement lines a test prints on purpose (OHPE-STATS / OHPE-R) also show when it passes
                    for line in out.splitlines():
                        if line.startswith(("OHPE-STATS", "OHPE-R ")):
                            print("  " + line)
                if not ok:
                    if GH:
                        print(f"::group::output of {key} (rc {rc})")
                    print(out)
                    if GH:
                        print("::endgroup::")
                        print(f"::error title=test failed::{key} {label}{note}")
                    sys.stdout.flush()
            return res
        finally:
            with slot_lock:
                slots.append(slot)

    t0 = time.time()
    with cf.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for r in ex.map(run, jobs):
            results.append(r)
    wall = time.time() - t0

    failed = [r for r in results if not r[2]]
    ran = {}
    for key, *_ in results:
        name = key.split("#")[0]  # noqa
        ran[name] = ran.get(name, 0) + 1
    for name, count in expected.items():
        if ran.get(name) != count:
            failed.append((name, "", False, 0, 0, "", f"ran {ran.get(name)} of {count} processes", name))
            print(f"FAIL {name}: ran {ran.get(name)} of {count} processes")

    cpu = sum(r[3] for r in results)
    print("\nslowest:")
    for r in sorted(results, key=lambda r: -r[3])[:15]:
        print(f"  {r[3]:7.1f} s  {r[0]:<18} {r[1]}")
    print(f"\n{len(results) - len([r for r in results if not r[2]])}/{len(results)} processes passed, "
          f"wall {wall:.0f} s, summed {cpu:.0f} s ({cpu / max(wall, 1):.1f}x)")

    if args.timings:
        new = dict(timings)
        for r in results:
            new[r[7]] = round(r[3], 2)
        with open(args.timings, "w") as f:
            json.dump(new, f, indent=1, sort_keys=True)

    if failed:
        print(f"\n{len(failed)} FAILED")
        return 1
    print("\nALL SELECTED TESTS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
