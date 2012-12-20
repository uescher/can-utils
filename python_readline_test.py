import subprocess

p = subprocess.Popen(["./can-read"], stdout=subprocess.PIPE)

while p.poll() is None:
    l = p.stdout.readline()
    print l
