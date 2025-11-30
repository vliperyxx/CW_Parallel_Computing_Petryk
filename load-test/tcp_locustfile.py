import socket
import time
import random
from locust import User, task, events, constant
import gevent

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8080

SEARCH_PHRASES = ["one of the", "this is a", "very good", "waste of time", "to be a", "part of the", "end of the"]

class TcpClient:
    def __init__(self):
        self.sock = None

    def connect(self):
        start_time = time.time()
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((SERVER_HOST, SERVER_PORT))
            self.sock.settimeout(10.0)

            data = b""
            while True:
                try:
                    chunk = self.sock.recv(1)
                except socket.timeout:
                    break
                if not chunk:
                    break
                data += chunk
                if chunk == b"\n":
                    break

            message = data.decode("utf-8", errors="ignore").strip()

            if "SERVER_BUSY" in message:
                self.sock.settimeout(60.0)

                try:
                    welcome_data = self.sock.recv(1024)
                    if b"Welcome" not in welcome_data:
                        raise Exception("Did not receive Welcome after wait")
                except socket.timeout:
                    raise Exception("Timeout waiting in queue")

                total_time = int((time.time() - start_time) * 1000)
                events.request.fire(
                    request_type="TCP",
                    name="/connect_queue",
                    response_time=total_time,
                    response_length=len(data) + len(welcome_data),
                    exception=None,
                )
            else:
                total_time = int((time.time() - start_time) * 1000)
                events.request.fire(
                    request_type="TCP",
                    name="/connect",
                    response_time=total_time,
                    response_length=len(data),
                    exception=None,
                )

            return True

        except Exception as e:
            total_time = int((time.time() - start_time) * 1000)
            events.request.fire(
                request_type="TCP",
                name="/connect_fail",
                response_time=total_time,
                response_length=0,
                exception=e,
            )
            self.close()
            return False

    def send_command(self, command_str, report_name):
        if not self.sock:
            return False

        start_time = time.time()
        if not command_str.endswith("\n"):
            command_str += "\n"

        try:
            self.sock.sendall(command_str.encode("utf-8"))

            if report_name == "/quit":
                response = b""
            else:
                self.sock.settimeout(30.0)
                response = b""
                while True:
                    try:
                        chunk = self.sock.recv(4096)
                        if not chunk:
                            break
                        response += chunk
                        if len(chunk) < 4096:
                            break
                    except socket.timeout:
                        break

            total_time = int((time.time() - start_time) * 1000)
            events.request.fire(
                request_type="TCP",
                name=report_name,
                response_time=total_time,
                response_length=len(response),
                exception=None,
            )
            return True

        except Exception as e:
            total_time = int((time.time() - start_time) * 1000)
            events.request.fire(
                request_type="TCP",
                name=report_name,
                response_time=total_time,
                response_length=0,
                exception=e,
            )
            self.close()
            return False

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except:
                pass
            self.sock = None

class CppServerUser(User):
    wait_time = constant(2)

    def on_start(self):
        self.client = TcpClient()
        if not self.client.connect():
            self.stop()

    def on_stop(self):
        if self.client and self.client.sock:
            self.client.send_command("quit", "/quit")
            self.client.close()

    @task(5)
    def search_phrase(self):
        if self.client.sock:
            phrase = random.choice(SEARCH_PHRASES)
            self.client.send_command(f"search {phrase}", "/search")
            gevent.sleep(0.5)

    @task(1)
    def get_snippet(self):
        if self.client.sock:
            self.client.send_command("getsnippet 0", "/getsnippet")
