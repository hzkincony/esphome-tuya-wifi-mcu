"""Compile and drive a real ESPHome host process over UART and the native API."""

import asyncio
from collections import Counter
import json
import math
import os
from pathlib import Path
import pty
import socket
import subprocess
import sys
import tempfile
import time
import tty
import unittest

from aioesphomeapi import APIClient, APIConnectionError


HERE = Path(__file__).resolve().parent
TIMEOUT = 5.0
HEARTBEAT = 0x00
PRODUCT_QUERY = 0x01
WORK_MODE_QUERY = 0x02
WIFI_STATE = 0x03
DP_DOWNLOAD = 0x06
DP_UPLOAD = 0x07
STATE_QUERY = 0x08


def frame(command, payload=b""):
    data = b"\x55\xaa\x00" + bytes([command]) + len(payload).to_bytes(2, "big") + payload
    return data + bytes([sum(data) & 0xFF])


def bool_dp(dp_id, value):
    return bytes([dp_id, 1, 0, 1, int(value)])


def value_dp(dp_id, value):
    return bytes([dp_id, 2, 0, 4]) + value.to_bytes(4, "big")


class FirmwareTestResult(unittest.TextTestResult):
    def print_firmware_log(self, test):
        log_path = getattr(test, "log_path", None)
        if log_path is not None and log_path.exists():
            self.stream.writeln(f"\n--- ESPHome log: {test.id()} ---")
            self.stream.writeln(log_path.read_text(errors="replace"))

    def addError(self, test, err):
        super().addError(test, err)
        self.print_firmware_log(test)

    def addFailure(self, test, err):
        super().addFailure(test, err)
        self.print_firmware_log(test)


class HostE2ETest(unittest.IsolatedAsyncioTestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory(prefix="tuya-host-e2e-")
        cls.addClassCleanup(cls.workspace.cleanup)
        # ESPHome 2026.8 accepts /tmp/name, but not the deeper /dev/pts/N path.
        cls.uart_path = Path(f"{cls.workspace.name}.uart")
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            cls.api_port = sock.getsockname()[1]
        subprocess.run(
            [
                sys.executable, "-m", "esphome",
                "-s", "uart_path", str(cls.uart_path),
                "-s", "api_port", str(cls.api_port),
                "compile", str(HERE / "host.yaml"),
            ],
            check=True,
            timeout=600,
        )
        cls.binary = HERE / ".esphome/build/tuya-host-e2e/.pioenvs/tuya-host-e2e/program"
        if not cls.binary.is_file():
            raise RuntimeError(f"ESPHome host executable not found: {cls.binary}")

    async def asyncSetUp(self):
        self.master, self.slave = pty.openpty()
        self.addCleanup(os.close, self.master)
        self.addCleanup(os.close, self.slave)
        tty.setraw(self.slave)
        os.set_blocking(self.master, False)
        self.uart_path.symlink_to(os.ttyname(self.slave))
        self.addCleanup(self.uart_path.unlink, missing_ok=True)
        self.rx = bytearray()
        self.frames = asyncio.Queue()
        self.loop = asyncio.get_running_loop()
        self.loop.add_reader(self.master, self.read_uart)
        self.addCleanup(self.loop.remove_reader, self.master)
        log_dir = HERE / ".esphome/logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        self.log_path = log_dir / f"{self._testMethodName}.log"
        self.log = self.log_path.open("wb")
        self.addCleanup(self.log.close)
        self.process = subprocess.Popen(
            [str(self.binary)],
            cwd=self.workspace.name,
            env={**os.environ, "ESPHOME_PREFDIR": str(Path(self.workspace.name) / self._testMethodName)},
            stdout=self.log,
            stderr=subprocess.STDOUT,
        )
        self.addAsyncCleanup(self.stop_host)
        self.client = APIClient("127.0.0.1", self.api_port, None)
        self.addAsyncCleanup(self.disconnect_client)
        async with asyncio.timeout(TIMEOUT):
            while True:
                self.assert_running()
                try:
                    await self.client.connect(login=True, log_errors=False)
                    break
                except APIConnectionError:
                    await asyncio.sleep(0.05)
        entities, services = await asyncio.wait_for(self.client.list_entities_services(), TIMEOUT)
        self.entities = {entity.name: entity.key for entity in entities}
        self.services = {service.name: service for service in services}
        self.states = {}
        self.client.subscribe_states(self.on_state)
        await self.wait_state("Local Relay", lambda state: not state.state)
        await self.expect_bool("Local Input", False)
        await self.expect_bool("Tuya Input", False)
        await self.wait_state("Local Dimmer", lambda state: not state.state)
        await self.wait_state("Tuya Dimmer", lambda state: not state.state)
        await self.collect_frames(0.15)

    async def disconnect_client(self):
        await asyncio.wait_for(self.client.disconnect(), TIMEOUT)

    async def stop_host(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                await asyncio.to_thread(self.process.wait, timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                await asyncio.to_thread(self.process.wait, timeout=3)
        self.log.flush()

    async def asyncTearDown(self):
        self.assert_running()

    def assert_running(self):
        self.assertIsNone(self.process.poll(), "ESPHome host exited unexpectedly; see firmware log")

    def read_uart(self):
        try:
            chunk = os.read(self.master, 65536)
        except BlockingIOError:
            return
        self.rx.extend(chunk)
        while len(self.rx) >= 6:
            length = int.from_bytes(self.rx[4:6], "big") + 7
            if self.rx[:2] != b"\x55\xaa" or length > 1031:
                self.frames.put_nowait(bytes(self.rx))
                self.rx.clear()
                return
            if len(self.rx) < length:
                return
            self.frames.put_nowait(bytes(self.rx[:length]))
            del self.rx[:length]

    def send(self, command, payload=b""):
        self.send_raw(frame(command, payload))

    def send_raw(self, data):
        self.assert_running()
        self.assertEqual(os.write(self.master, data), len(data))

    async def next_frame(self, timeout=TIMEOUT):
        self.assert_running()
        raw = await asyncio.wait_for(self.frames.get(), timeout)
        self.assertEqual(raw[:3], b"\x55\xaa\x03", raw.hex())
        self.assertEqual(len(raw), int.from_bytes(raw[4:6], "big") + 7, raw.hex())
        self.assertEqual(raw[-1], sum(raw[:-1]) & 0xFF, raw.hex())
        return raw[3], raw[6:-1]

    async def collect_frames(self, duration):
        frames = []
        deadline = time.monotonic() + duration
        while (remaining := deadline - time.monotonic()) > 0:
            try:
                frames.append(await self.next_frame(remaining))
            except TimeoutError:
                break
        self.assert_running()
        self.assertFalse(self.rx, f"Incomplete outgoing frame: {self.rx.hex()}")
        return frames

    async def expect_frames(self, expected):
        actual = [await self.next_frame() for _ in expected]
        self.assertEqual(Counter(actual), Counter(expected))
        self.assertEqual(await self.collect_frames(0.15), [], "Unexpected duplicate/echo frames")

    def on_state(self, state):
        self.states[state.key] = state

    async def wait_state(self, name, predicate):
        key = self.entities[name]
        deadline = time.monotonic() + TIMEOUT
        while time.monotonic() < deadline:
            self.assert_running()
            state = self.states.get(key)
            if state is not None and predicate(state):
                return state
            await asyncio.sleep(0.01)
        self.fail(f"Timed out waiting for {name}; last state: {self.states.get(key)}")

    async def expect_bool(self, name, value):
        await self.wait_state(
            name,
            lambda state: not getattr(state, "missing_state", False) and state.state == value,
        )

    async def expect_light(self, name, brightness):
        await self.wait_state(
            name,
            lambda state: state.state == (brightness != 0)
            and (brightness == 0 or math.isclose(state.brightness, brightness, abs_tol=0.001)),
        )

    async def query_states(self, relay=False, sensor=False, brightness=0):
        self.send(STATE_QUERY)
        await self.expect_frames([
            (DP_UPLOAD, bool_dp(1, relay)),
            (DP_UPLOAD, bool_dp(2, sensor)),
            (DP_UPLOAD, value_dp(3, brightness)),
        ])

    async def test_handshake_and_state_query(self):
        self.send(HEARTBEAT)
        await self.expect_frames([(HEARTBEAT, b"\x00")])
        self.send(HEARTBEAT)
        await self.expect_frames([(HEARTBEAT, b"\x01")])
        self.send(PRODUCT_QUERY)
        command, payload = await self.next_frame()
        self.assertEqual(command, PRODUCT_QUERY)
        self.assertEqual(json.loads(payload), {"p": "hoste2etest", "v": "1.2.3", "m": 0})
        self.send(WORK_MODE_QUERY)
        await self.expect_frames([(WORK_MODE_QUERY, b"")])
        self.send(WIFI_STATE, b"\x04")
        await self.expect_frames([(WIFI_STATE, b"")])
        await self.query_states()

    async def test_remote_multi_dp_updates_bound_entities(self):
        payload = bool_dp(1, True) + bool_dp(2, True) + value_dp(3, 42)
        self.send(DP_DOWNLOAD, payload)
        await self.expect_frames([
            (DP_UPLOAD, bool_dp(1, True)),
            (DP_UPLOAD, bool_dp(2, True)),
            (DP_UPLOAD, value_dp(3, 42)),
        ])
        for name in ("Local Relay", "Tuya Relay", "Local Input", "Tuya Input"):
            await self.expect_bool(name, True)
        for name in ("Local Dimmer", "Tuya Dimmer"):
            await self.expect_light(name, 0.42)
        for name in ("Local Output", "Tuya Output"):
            await self.wait_state(name, lambda state: math.isclose(state.state, 0.42 ** 2.8, abs_tol=0.001))
        await self.query_states(relay=True, sensor=True, brightness=42)
        self.send(DP_DOWNLOAD, bool_dp(1, False) + bool_dp(2, False) + value_dp(3, 0))
        await self.expect_frames([
            (DP_UPLOAD, bool_dp(1, False)),
            (DP_UPLOAD, bool_dp(2, False)),
            (DP_UPLOAD, value_dp(3, 0)),
        ])
        for name in ("Local Relay", "Tuya Relay", "Local Input", "Tuya Input"):
            await self.expect_bool(name, False)
        for name in ("Local Dimmer", "Tuya Dimmer"):
            await self.expect_light(name, 0)
        for name in ("Local Output", "Tuya Output"):
            await self.wait_state(name, lambda state: state.state == 0)

    async def test_local_switch_and_input_report_to_tuya(self):
        for value in (True, False):
            self.client.switch_command(self.entities["Local Relay"], value)
            await self.expect_frames([(DP_UPLOAD, bool_dp(1, value))])
            await self.expect_bool("Tuya Relay", value)
            await asyncio.wait_for(
                self.client.execute_service(self.services["set_input"], {"value": value}), TIMEOUT,
            )
            await self.expect_frames([(DP_UPLOAD, bool_dp(2, value))])
            await self.expect_bool("Tuya Input", value)
        self.client.switch_command(self.entities["Tuya Relay"], True)
        await self.expect_frames([(DP_UPLOAD, bool_dp(1, True))])
        await self.expect_bool("Local Relay", True)

    async def test_local_light_transition_reports_logical_brightness(self):
        self.client.light_command(
            self.entities["Local Dimmer"], state=True, brightness=0.73, transition_length=0.3,
        )
        await self.expect_frames([(DP_UPLOAD, value_dp(3, 73))])
        await self.wait_state("Local Output", lambda state: math.isclose(state.state, 0.73 ** 2.8, abs_tol=0.001))
        self.assertEqual(await self.collect_frames(0.2), [])
        await self.query_states(brightness=73)
        self.client.light_command(self.entities["Local Dimmer"], state=False, transition_length=0)
        await self.expect_frames([(DP_UPLOAD, value_dp(3, 0))])
        self.client.light_command(self.entities["Tuya Dimmer"], state=True, brightness=0.28, transition_length=0.3)
        await self.expect_frames([(DP_UPLOAD, value_dp(3, 28))])
        await self.wait_state("Tuya Output", lambda state: math.isclose(state.state, 0.28 ** 2.8, abs_tol=0.001))
        self.assertEqual(await self.collect_frames(0.2), [])
        await self.query_states(brightness=28)

    async def test_repeated_download_is_acknowledged_once(self):
        for _ in range(2):
            self.send(DP_DOWNLOAD, bool_dp(1, True))
            await self.expect_frames([(DP_UPLOAD, bool_dp(1, True))])
        await self.expect_bool("Local Relay", True)

    async def test_invalid_downloads_do_not_change_state(self):
        bad_checksum = bytearray(frame(DP_DOWNLOAD, bool_dp(1, True)))
        bad_checksum[-1] ^= 0x80
        self.send_raw(bad_checksum)
        for payload in (
            bool_dp(99, True),
            value_dp(1, 1),
            bool_dp(1, 2),
            value_dp(3, 101),
            bool_dp(1, True) + b"\x02\x01\x00",
            b"",
        ):
            self.send(DP_DOWNLOAD, payload)
        await self.query_states()
        for name in ("Local Relay", "Tuya Relay", "Local Input", "Tuya Input"):
            await self.expect_bool(name, False)
        for name in ("Local Dimmer", "Tuya Dimmer"):
            await self.expect_light(name, 0)
        self.send(DP_DOWNLOAD, bool_dp(1, True))
        await self.expect_frames([(DP_UPLOAD, bool_dp(1, True))])
        await self.expect_bool("Local Relay", True)

    async def test_fragmented_frame_and_idle_timeout_recovery(self):
        data = frame(DP_DOWNLOAD, value_dp(3, 65))
        for chunk in (data[:1], data[1:5], data[5:8], data[8:]):
            self.send_raw(chunk)
            await asyncio.sleep(0.02)
        await self.expect_frames([(DP_UPLOAD, value_dp(3, 65))])
        await self.expect_light("Local Dimmer", 0.65)
        self.send_raw(frame(DP_DOWNLOAD, bool_dp(1, True))[:-2])
        await asyncio.sleep(0.4)
        self.send(HEARTBEAT)
        await self.expect_frames([(HEARTBEAT, b"\x00")])
        await self.query_states(brightness=65)

    async def test_oversized_frame_skips_embedded_commands(self):
        embedded = frame(DP_DOWNLOAD, bool_dp(1, True))
        self.send_raw(frame(DP_DOWNLOAD, embedded + bytes(1025 - len(embedded))))
        self.send(HEARTBEAT)
        await self.expect_frames([(HEARTBEAT, b"\x00")])
        await self.query_states()


if __name__ == "__main__":
    unittest.main(testRunner=unittest.TextTestRunner(resultclass=FirmwareTestResult, verbosity=2))
