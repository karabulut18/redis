import socket
import time

def send_command(sock, command):
    print(f"Sending: {command.strip()}")
    sock.sendall(command.encode())
    response = sock.recv(4096).decode()
    print(f"Received: {response.strip()}")
    return response

def test():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2)
        connected = False
        for i in range(5):
            try:
                s.connect(('127.0.0.1', 6379))
                connected = True
                break
            except ConnectionRefusedError:
                print("Connection refused, retrying...")
                time.sleep(1)
        
        if not connected:
            print("Failed to connect to server")
            exit(1)

        print("Connected to server")

        # Test PING
        resp = send_command(s, "*1\r\n$4\r\nPING\r\n")
        if "+PONG" not in resp:
            print("PING failed")
            exit(1)
        
        # Test SET
        resp = send_command(s, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n")
        if "+OK" not in resp:
            print("SET failed")
            exit(1)
        
        # Test GET
        resp = send_command(s, "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n")
        if "$3\r\nbar" not in resp:
            print("GET failed")
            exit(1)
        
        print("Integration test passed!")
        s.close()
    except Exception as e:
        print(f"Test failed: {e}")
        exit(1)

if __name__ == "__main__":
    test()
