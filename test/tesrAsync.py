#!/usr/bin/env python3
# network_test.py

import socket
import time
import sys
import random
import string

class ServerTester:
    def __init__(self, host='127.0.0.77', port=8080):
        self.host = host
        self.port = port

    def test_tcp_text(self, message):
        """Тестирование текстовых сообщений по TCP"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((self.host, self.port))

            sock.sendall((message + '\n').encode())
            response = sock.recv(1024).decode().strip()

            print(f"TCP TEXT: '{message}' -> '{response}'")
            sock.close()
            return response

        except Exception as e:
            print(f"TCP TEXT ERROR: {e}")
            return None

    def test_udp_text(self, message):
        """Тестирование текстовых сообщений по UDP"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(5)

            sock.sendto((message + '\n').encode(), (self.host, self.port))
            response, addr = sock.recvfrom(1024)
            response = response.decode().strip()

            print(f"UDP TEXT: '{message}' -> '{response}'")
            sock.close()
            return response

        except Exception as e:
            print(f"UDP TEXT ERROR: {e}")
            return None

    def test_tcp_binary(self, data):
        """Тестирование бинарных данных по TCP"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((self.host, self.port))

            sock.sendall(data)
            response = sock.recv(1024)

            print(f"TCP BINARY: {len(data)} bytes -> {len(response)} bytes")
            print(f"Hex: {data.hex()} -> {response.hex()}")
            sock.close()
            return response

        except Exception as e:
            print(f"TCP BINARY ERROR: {e}")
            return None

    def test_udp_binary(self, data):
        """Тестирование бинарных данных по UDP"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(5)

            sock.sendto(data, (self.host, self.port))
            response, addr = sock.recvfrom(1024)

            print(f"UDP BINARY: {len(data)} bytes -> {len(response)} bytes")
            print(f"Hex: {data.hex()} -> {response.hex()}")
            sock.close()
            return response

        except Exception as e:
            print(f"UDP BINARY ERROR: {e}")
            return None

    def test_large_data(self, size=10000):
        """Тестирование больших объемов данных"""
        print(f"Testing large data ({size} bytes)...")

        # Генерируем случайные данные
        large_data = ''.join(random.choices(string.ascii_letters + string.digits, k=size))

        # TCP
        response = self.test_tcp_text(large_data)
        if response and response == large_data:
            print("✓ Large TCP data test PASSED")
        else:
            print("✗ Large TCP data test FAILED")

        # UDP (может не работать для больших данных из-за ограничений MTU)
        if size <= 1500:  # Разумный размер для UDP
            response = self.test_udp_text(large_data[:500])  # Еще меньше для UDP
            if response and response == large_data[:500]:
                print("✓ Large UDP data test PASSED")
            else:
                print("✗ Large UDP data test FAILED")

    def test_special_characters(self):
        """Тестирование специальных символов"""
        test_cases = [
            "Line with\nnewline",
            "Tab\tseparated",
            "Text with spaces",
            "Unicode: café ñáéíóú 中文",
            "Special: !@#$%^&*()",
            "Quotes: 'single' \"double\""
        ]

        print("Testing special characters...")
        for test in test_cases:
            self.test_tcp_text(test)
            time.sleep(0.1)

    def test_command_sequence(self):
        """Тестирование последовательности команд"""
        commands = [
            "Hello",
            "/time",
            "How are you?",
            "/stats",
            "/unknown",
            "Goodbye"
        ]

        print("Testing command sequence...")
        for cmd in commands:
            self.test_tcp_text(cmd)
            time.sleep(0.2)

    def test_binary_commands(self):
        """Тестирование бинарных данных, которые могут быть интерпретированы как команды"""
        binary_tests = [
            b'/time\x00',  # Команда с нулевым байтом
            b'\x2ftime',   # '/' в hex
            b'\x00\x00/shutdown',  # Нулевые байты перед командой
            b'/stats\xff\xfe',  # Команда с специальными байтами
        ]

        print("Testing binary commands...")
        for data in binary_tests:
            self.test_tcp_binary(data)
            time.sleep(0.2)

    def test_performance(self, num_requests=100):
        """Тестирование производительности"""
        print(f"Performance test: {num_requests} requests")

        start_time = time.time()
        successful = 0

        for i in range(num_requests):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(2)
                sock.connect((self.host, self.port))
                sock.sendall(b"/time\n")
                response = sock.recv(1024)
                if response:
                    successful += 1
                sock.close()
            except:
                pass

        duration = time.time() - start_time
        print(f"Performance: {successful}/{num_requests} successful in {duration:.2f}s")
        print(f"Rate: {successful/duration:.2f} requests/second")

    def run_all_tests(self):
        """Запуск всех тестов"""
        print(f"=== Testing AsyncServer at {self.host}:{self.port} ===")

        tests = [
            ("Basic Commands", self.test_command_sequence),
            ("Special Characters", self.test_special_characters),
            ("Large Data", lambda: self.test_large_data(5000)),
            ("Binary Data", self.test_binary_commands),
            ("Performance", lambda: self.test_performance(50))
        ]

        for test_name, test_func in tests:
            print(f"\n--- {test_name} ---")
            try:
                test_func()
                time.sleep(1)  # Пауза между тестами
            except Exception as e:
                print(f"Test {test_name} failed: {e}")

if __name__ == "__main__":
    tester = ServerTester()

    if len(sys.argv) > 1:
        if sys.argv[1] == "tcp":
            tester.test_tcp_text(" ".join(sys.argv[2:]))
        elif sys.argv[1] == "udp":
            tester.test_udp_text(" ".join(sys.argv[2:]))
        elif sys.argv[1] == "binary":
            tester.test_tcp_binary(b''.join(bytes(arg, 'utf-8') for arg in sys.argv[2:]))
        else:
            tester.run_all_tests()
    else:
        tester.run_all_tests()