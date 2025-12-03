import socket
import threading
import random
import time

SERVER_IP = "127.0.0.1"
SERVER_PORT = 12000

# commands that clients randomly send
COMMANDS = [
    "say$ hello",
    "say$ test message",
    "say$ spam",
    "rename$ UserX",
    "mute$ Alice",
    "unmute$ Alice",
]

def client_thread(id):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("", 0))  # random client port

    name = f"User{id}"
    s.sendto(f"conn${name}".encode(), (SERVER_IP, SERVER_PORT))

    for _ in range(200):
        cmd = random.choice(COMMANDS)

        # random target for sayto
        if random.random() < 0.2:
            target = f"User{random.randint(0, 19)}"
            cmd = f"sayto${target} hi"

        s.sendto(cmd.encode(), (SERVER_IP, SERVER_PORT))

        # read responses non-blocking
        s.settimeout(0.01)
        try:
            data, _ = s.recvfrom(1024)
            # print(f"{name} <- {data.decode().strip()}")
        except:
            pass


    # finally disconnect
    s.sendto(f"disconn$".encode(), (SERVER_IP, SERVER_PORT))


def main():
    NUM_CLIENTS = 20   # increase to 50 or 100 for heavier testing
    threads = []

    for i in range(NUM_CLIENTS):
        t = threading.Thread(target=client_thread, args=(i,))
        t.start()
        threads.append(t)
        time.sleep(0.03)

    for t in threads:
        t.join()

    print("Stress test complete.")


if __name__ == "__main__":
    main()
