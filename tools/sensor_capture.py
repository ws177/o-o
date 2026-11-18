import os
import serial, time, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DUR = 45
OUT = os.path.join(ROOT, ".tools", "sensor_log.txt")
sp = serial.Serial("COM12", 115200, timeout=0.1)
t0 = time.time()
with open(OUT, "w", encoding="utf-8") as f:
    buf = b""
    while time.time() - t0 < DUR:
        buf += sp.read(512)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            s = line.decode("latin1", "ignore").strip()
            if "als=" in s and "acc=" in s:
                f.write("%6.2f  %s\n" % (time.time() - t0, s))
                f.flush()
sp.close()
print("done")
