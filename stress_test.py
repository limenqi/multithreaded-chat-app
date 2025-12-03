import socket
import threading
import random
import time

SERVER_ADDR = ("127.0.0.1", 5000)
NUM_CLIENTS = 50          # Increase to 200+ for heavy testing
COMMANDS_PER_CLIENT = 20

def client_thread(client_id):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    name = f"user{client_id}"

    # 1. Always CONNECT first
    connect_msg = f"conn${name}".encode()
    s.sendto(connect_msg, SERVER_ADDR)

    # 2. Wait a bit so server can register this client
    time.sleep(0.01)

    # 3. Now send commands
    for _ in range(COMMANDS_PER_CLIENT):
        cmd_type = random.choice(["say", "say", "sayto", "mute", "unmute", "rename"])

        if cmd_type == "say":
            msg = f"say$Hello_from_{name}"
        elif cmd_type == "sayto":
            msg = f"sayto$user{random.randint(0, NUM_CLIENTS-1)}$Hello"
        elif cmd_type == "mute":
            msg = f"mute$user{random.randint(0, NUM_CLIENTS-1)}"
        elif cmd_type == "unmute":
            msg = f"unmute$user{random.randint(0, NUM_CLIENTS-1)}"
        elif cmd_type == "rename":
            new_name = f"user{client_id}_renamed"
            msg = f"rename${new_name}"
        else:
            msg = "bad$cmd"

        s.sendto(msg.encode(), SERVER_ADDR)

        # Slight delay to spread load
        time.sleep(random.uniform(0.001, 0.005))

    # 4. Disconnect cleanly
    s.sendto(f"bye${name}".encode(), SERVER_ADDR)
    s.close()

def main():
    threads = []
    for i in range(NUM_CLIENTS):
        t = threading.Thread(target=client_thread, args=(i,))
        t.start()
        threads.append(t)
        time.sleep(0.005)  # Stagger connections

    for t in threads:
        t.join()

    print("Stress test complete.")

if __name__ == "__main__":
    main()
