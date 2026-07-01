import os
import pty
import sys
import time
import select

def expect(fd, patterns, timeout=30):
    buf = b""
    start = time.time()
    while time.time() - start < timeout:
        r, _, _ = select.select([fd], [], [], 0.5)
        if fd in r:
            try:
                chunk = os.read(fd, 1024)
                if not chunk: break
                buf += chunk
                sys.stdout.buffer.write(chunk)
                sys.stdout.flush()
                for i, p in enumerate(patterns):
                    if p.encode() in buf:
                        return i, buf
            except OSError:
                break
    return -1, buf

def send(fd, cmd):
    os.write(fd, cmd.encode() + b"\n")
    time.sleep(1)

def run():
    pid, fd = pty.fork()
    if pid == 0:
        os.execvp("ssh", ["ssh", "dellcloud@10.32.45.220"])
    else:
        # Wait for dellcloud prompt
        idx, _ = expect(fd, ["password:", "Welcome", "dellcloud"])
        if idx == 0:
            send(fd, "cePES110!")
            expect(fd, ["dellcloud@"])
        
        # Now on dellcloud, ssh to atlantica
        send(fd, "ssh -o StrictHostKeyChecking=no cp04@atlantica.lad.pucrs.br")
        idx, _ = expect(fd, ["password:"])
        if idx == 0:
            send(fd, "chanel04")
        
        idx, _ = expect(fd, ["cp04@atlantica"])
        if idx != 0:
            print("Failed to reach atlantica")
            return
            
        print("\n\n--- CONNECTED TO ATLANTICA ---")
        
        # NO SCANCEL HERE! We want to keep the old job running!

        send(fd, "cd trabalho_4_new")
        expect(fd, ["cp04@atlantica"])
        
        # Read payload
        with open("payload_db.b64", "r") as f:
            b64 = f.read().replace('\n', '')
            
        # write base64 in chunks
        chunk_size = 1000
        send(fd, "echo -n '' > payload_db.b64")
        for i in range(0, len(b64), chunk_size):
            chunk = b64[i:i+chunk_size]
            send(fd, f"echo -n '{chunk}' >> payload_db.b64")
            time.sleep(0.1)
            
        send(fd, "base64 -d payload_db.b64 | tar xz")
        expect(fd, ["cp04@atlantica"])
        
        send(fd, "rm -f resultados_db.log")
        expect(fd, ["cp04@atlantica"])

        # Submit to SLURM in background!
        send(fd, "sbatch submit_db.sbatch")
        expect(fd, ["Submitted batch job"])
        expect(fd, ["cp04@atlantica"])
        
        print("\n\n--- JOB SUBMITTED TO SLURM ---")
        send(fd, "squeue -u cp04")
        expect(fd, ["cp04@atlantica"])
        
        send(fd, "exit")
        expect(fd, ["dellcloud@"])
        send(fd, "exit")
        os.waitpid(pid, 0)

if __name__ == "__main__":
    run()
