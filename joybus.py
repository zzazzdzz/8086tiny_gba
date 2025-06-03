import os
import sys
import struct
import time
import random
import threading
import traceback

import socket
import socketserver

data_sock_c = None
clock_sock_c = None
data_sock_await = True
clock_sock_await = True
joybus_running = False

sock_barrier = threading.Barrier(2, timeout=60)

STATE_HANDSHAKE_FIRST = 1
STATE_HANDSHAKE_SECOND = 2
STATE_HANDSHAKE_RESPONSE_FIRST = 3
STATE_HANDSHAKE_RESPONSE_SECOND = 4
STATE_READ_PACKET_LENGTH = 5
STATE_READ_PACKET_DATA = 6
STATE_PROCESS_PACKET = 7
STATE_WRITE_RESPONSE_LENGTH = 8
STATE_WRITE_RESPONSE_DATA = 9

last_clk = 0
second = 16780000 + 69000

def read_word_ratelimit(as_bytes=False):
    time.sleep(0.005)
    return read_word(as_bytes=as_bytes)

def send_word_ratelimit(x):
    time.sleep(0.005)
    return send_word(x)

def read_word(as_bytes=False):
    global last_clk
    clk = int((time.time() - last_clk) * second)
    last_clk = time.time()
    st_clk = struct.pack(">i", clk)
    st_data = b'\x14'
    # print("clock := %s" % st_clk)
    clock_sock_c.sendall(st_clk)
    # print("data  := %s" % st_data)
    data_sock_c.sendall(st_data)
    resp = data_sock_c.recv(5)
    if as_bytes: return resp[0:4]
    else: return struct.unpack("<I", resp[0:4])[0]

def send_word(x):
    global last_clk
    clk = int((time.time() - last_clk) * second)
    last_clk = time.time()
    st_clk = struct.pack(">i", clk)
    st_data = struct.pack("<BI", 0x15, x)
    # print("clock := %s" % st_clk)
    clock_sock_c.sendall(st_clk)
    # print("data  := %s" % st_data)
    data_sock_c.sendall(st_data)
    resp = data_sock_c.recv(1)
    return resp[0]

with open("bios", "rb") as f:
    bios_img = f.read() + bytes(0x8000)
with open("fd.img", "rb") as f:
    fd_img = f.read()

def joybus_handler():
    global last_clk, data_sock_c, clock_sock_c
    last_clk = time.time()
    debug_cmds = (0, 0)
    cur_state = STATE_HANDSHAKE_FIRST
    cur_packet = []
    last_recv = time.time()
    cut = []
    packet_len = 0
    while 1:
        r = read_word()
        if r == 0xb3515420:
            print("got init")
            send_word(0x4206969)
        if (r >> 24) == 0x69:
            # bios read
            at = r & 0xffffff
            v = bios_img[at]
            print("bios read req | offs=%.6x | v=%.2x" % (at, v))
            send_word(0x4206900 | v)
        if (r >> 24) == 0x42:
            # fd read
            at = r & 0xffffff
            v = fd_img[at]
            print("fd read req   | offs=%.6x | v=%.2x" % (at, v))
            send_word(0x4206900 | v)

class JoyBusClockServer(socketserver.BaseRequestHandler):
    def handle(self):
        global clock_sock_c, clock_sock_await, joybus_running
        if joybus_running:
            self.request.close()
            return
        clock_sock_c = self.request
        clock_sock_await = False
        print("Awaiting data connection...")
        sock_barrier.wait()
        # this thread is the master
        # at this point both sockets should be up
        print("Starting JoyBus communication...")
        joybus_running = True
        data_sock_c.settimeout(5)
        clock_sock_c.settimeout(5)
        try:
            joybus_handler()
        except Exception as e:
            traceback.print_exc()
        try:
            data_sock_c.close()
        except:
            pass
        try:
            clock_sock_c.close()
        except:
            pass
        print("Warning: Disconnected from the emulator")
        data_sock_await = True
        clock_sock_await = True
        sock_barrier.reset()
        joybus_running = False
        print("Awaiting connection")

class JoyBusDataServer(socketserver.BaseRequestHandler):
    def handle(self):
        global data_sock_c, data_sock_await, joybus_running
        if joybus_running:
            self.request.close()
            return
        data_sock_c = self.request
        data_sock_await = False
        print("Awaiting clock connection...")
        sock_barrier.wait()
        # this thread has served its purpose and can just wait
        time.sleep(1)
        while joybus_running:
            time.sleep(0.05)

def _server_thread_clock():
    server = socketserver.ThreadingTCPServer(('127.0.0.1', 49420), JoyBusClockServer)
    server.serve_forever()

def _server_thread_data():
    server = socketserver.ThreadingTCPServer(('127.0.0.1', 54970), JoyBusDataServer)
    server.serve_forever()
	
def start():
    threading.Thread(target=_server_thread_clock).start()
    threading.Thread(target=_server_thread_data).start()

start()