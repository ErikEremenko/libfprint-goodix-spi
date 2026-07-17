#!/usr/bin/env python3
"""Drive winedbg reliably via pexpect: wait for each Wine-dbg> prompt before
sending the next command. Usage: wdrv.py <cmdfile> <exe-dospath> <args...>
Commands in cmdfile, one per line (blank lines ignored)."""
import sys, os, pexpect

cmdfile = sys.argv[1]
dosexe  = sys.argv[2]
args    = sys.argv[3:]
cmds = [l.rstrip("\n") for l in open(cmdfile) if l.strip()]

env = dict(os.environ); env["WINEDEBUG"] = "-all"
child = pexpect.spawn("winedbg", [dosexe, *args], env=env, encoding="utf-8",
                      timeout=80, codec_errors="replace", dimensions=(200, 400))
PROMPT = "Wine-dbg>"
buf = []
def wait_prompt(tag=""):
    try:
        child.expect(PROMPT)
        out = child.before
    except pexpect.TIMEOUT:
        out = child.before + "\n[TIMEOUT]"
    except pexpect.EOF:
        out = child.before + "\n[EOF]"
    sys.stdout.write(out)
    sys.stdout.flush()
    return out

wait_prompt("init")
for c in cmds:
    child.sendline(c)
    sys.stdout.write(f"\n>>> {c}\n")
    if child.eof():
        break
    wait_prompt(c)
try:
    child.expect(pexpect.EOF, timeout=10)
    sys.stdout.write(child.before)
except Exception:
    pass
