"""Drives the screen through every state. Usage: make demo"""
import json, sys, threading, time
import serial

port = sys.argv[1]
s = serial.Serial(port, 115200, timeout=0.1)

def send(**m):
    s.write((json.dumps(m) + "\n").encode())

def reader():
    while True:
        line = s.readline().decode(errors="ignore").strip()
        if line:
            print("<-", line)

threading.Thread(target=reader, daemon=True).start()
send(t="hello", v=1)

def beat(sec):
    for _ in range(int(sec)):
        send(t="ping")
        time.sleep(1)

beat(2)
send(t="state", project="flightsim", status="working", title="Refactoring the physics step", progress=0.2)
beat(3)
send(t="msg", text="Ran 214 tests, all passing", level="info")
beat(2)
send(t="msg", text="Lint warnings in 3 files", level="warn")
send(t="state", status="working", title="Refactoring the physics step", progress=0.65)
beat(3)
send(t="ask", id="a1", prompt="Delete 12 unused files?", options=["Delete", "Show list", "Skip"])
print("waiting for a tap on the screen (30 s)...")
beat(30)
send(t="state", status="done", title="All done", progress=1)
send(t="msg", text="Finished in 4m 12s", level="info")
beat(5)
send(t="state", status="error", title="Build failed: missing symbol in ui.cpp")
send(t="msg", text="Build failed", level="error")
beat(5)
print("stopping pings: screen should return to the nebula in ~3 s")
time.sleep(6)
