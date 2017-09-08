#!/usr/bin/env python3

import socket
import os
import time
import re
from subprocess import check_output, DEVNULL, PIPE, Popen


fast = True
IP = '127.0.0.1'
PORT = 9000
UDP_SERVER_PATH = '../asio_udp_srv/'
SOCKET = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
SENDING_TIME = 5


def send_udp_packages(ip, port, port_count, packet_size, sending_time):
    packet = ('x'*packet_size).encode()
    end_time = time.time() + sending_time
    iter = 0
    while time.time() < end_time:
        SOCKET.sendto(packet, (ip, port + (iter % port_count)))
        iter += 1


def run_udp_server(*args):
    os.chdir(UDP_SERVER_PATH)
    return Popen(['make', 'ARGS="{}"'.format(' '.join(str(x) for x in args)), 'run'], stderr=DEVNULL, stdout=PIPE)


def stop_udp_server(ip, port):
    # time.sleep(1)
    for _ in range(100000):
        SOCKET.sendto('exit'.encode(), (ip, port))


def get_test_result_from_output(output):
    for line in output.splitlines():
        if "i={}".format(SENDING_TIME - 2) in line:
            print(line)
            return list(map(float, re.findall(r'[-+]?\d*\.\d+|\d+', line)))


def run_test(*args):
    proc = run_udp_server(args)
    print('udp server started with args: ', args)
    if 'mport' in args:
        send_udp_packages(IP, PORT, args[1], 1024, SENDING_TIME)
    else:
        send_udp_packages(IP, PORT, 1, 1024, SENDING_TIME)
    stop_udp_server(IP, PORT)
    print('udp server stopped')
    out, err = proc.communicate()
    ret = get_test_result_from_output(out.decode('utf8'))
    return args, ret[1:]

if __name__ == "__main__":
    num_inbuf_list = set(list(range(1, 8)) + list(range(8, 16, 2)) + list(range(16, 32, 4)) + [32] + [32+x*x for x in range(4, 8)])
    num_socket_list = set(list(range(1, 8)) + list(range(8, 16, 2)) + list(range(16, 32, 4)) + [32] + [32+x*x for x in range(4, 8)])
    max_buf_socket_spread = 2
    num_ios_list = [1, 2, 128, 1024]
    num_thread_per_ios_list = [1, 2, 3, 4, 5, 6, 7, 8, 16, 128]
    work_list = [0, 10]
    max_port_multiport = 2
    max_mt = 2
    if fast:
        num_inbuf_list = [1, 8, 32, 64]
        num_socket_list = [1, 8, 32, 64]
        num_ios_list = [1, 2, 128, 1024]
        num_thread_per_ios_list = [1, 2, 8, 16, 128]
    with open('result.txt', 'w') as f:
        for num_inbuf in num_inbuf_list:
            for num_socket in num_socket_list:
                for buf_socket_spread in range(max_buf_socket_spread):
                    for num_ios in num_ios_list:
                        for num_thread_per_ios in num_thread_per_ios_list:
                            for work in work_list:
                                for port_multiport in range(max_port_multiport):
                                    for mt in range(max_mt):
                                        args, ret = run_test(num_inbuf, num_socket, buf_socket_spread, num_ios, num_thread_per_ios, work, 'mport' if port_multiport == 1 else '', 'mt_strand' if mt == 1 else 'mt_mutex')
                                        f.write(str(args))
                                        f.write('\t')
                                        f.write(str(ret))
                                        f.write('\n')
