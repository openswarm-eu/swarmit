#!/usr/bin/env python

import logging
import time

from dataclasses import dataclass
from enum import Enum

import click
import serial
import structlog

from tqdm import tqdm
from cryptography.hazmat.primitives import hashes
from rich.live import Live
from rich.table import Table

from dotbot.logger import LOGGER
from dotbot.hdlc import hdlc_encode, HDLCHandler, HDLCState
from dotbot.protocol import PROTOCOL_VERSION
from dotbot.serial_interface import SerialInterface, SerialInterfaceException


SERIAL_PORT = "/dev/ttyACM0"
BAUDRATE = 1000000
CHUNK_SIZE = 128
SWARMIT_PREAMBLE = bytes([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07])

# DK
# 0x24374c76b5cf8604,

DEVICES_IDS = [
    "0x61d2bc40c9864d0a",  # DB_old
    "0x7edc9c15171b71d5",  # DB1
    "0x298708dfd8e95c9b",  # DB2
    "0x6761d5b474d7becd",  # DB3
    "0xb9fa5113b820a2df",  # DB4
    "0xa2b55971529aa02c",  # DB5
    "0xde297dd2e6d83274",  # DB6
    "0xeb4dbfb6ad0f512c",  # DB8
    "0x9903ef26257feb31",  # DB10
    "0x0809c4bdd6f5e687",  # DB11
]


class NotificationType(Enum):
    """Types of notifications."""

    SWARMIT_NOTIFICATION_STATUS = 0
    SWARMIT_NOTIFICATION_OTA_START_ACK = 1
    SWARMIT_NOTIFICATION_OTA_CHUNK_ACK = 2
    SWARMIT_NOTIFICATION_EVENT_GPIO = 3
    SWARMIT_NOTIFICATION_EVENT_LOG = 4


class RequestType(Enum):
    """Types of requests."""

    SWARMIT_REQ_EXPERIMENT_START = 1
    SWARMIT_REQ_EXPERIMENT_STOP = 2
    SWARMIT_REQ_EXPERIMENT_STATUS = 3
    SWARMIT_REQ_OTA_START = 4
    SWARMIT_REQ_OTA_CHUNK = 5


class StatusType(Enum):
    """Types of device status."""

    Ready = 0
    Running = 1
    Off = 2


@dataclass
class DataChunk:
    """Class that holds data chunks."""

    index: int
    size: int
    data: bytes


class SwarmitStartExperiment:
    """Class used to start an experiment."""

    def __init__(self, port, baudrate, firmware):
        self.serial = SerialInterface(port, baudrate, self.on_byte_received)
        self.hdlc_handler = HDLCHandler()
        self.start_ack_received = False
        self.firmware = bytearray(firmware.read()) if firmware is not None else None
        self.last_acked_index = -1
        self.chunks = []
        self.fw_hash = None
        self.device_id = None
        self.acked_ids = []
        # Just write a single byte to fake a DotBot gateway handshake
        self.serial.write(int(PROTOCOL_VERSION).to_bytes(length=1))

    def on_byte_received(self, byte):
        self.hdlc_handler.handle_byte(byte)
        if self.hdlc_handler.state == HDLCState.READY:
            payload = self.hdlc_handler.payload
            if not payload:
                return
            deviceid_ack = hex(int.from_bytes(payload[0:8], byteorder="little"))
            if deviceid_ack not in self.acked_ids:
                self.acked_ids.append(deviceid_ack)
            if payload[8] == NotificationType.SWARMIT_NOTIFICATION_OTA_START_ACK.value:
                self.start_ack_received = True
            elif (
                payload[8] == NotificationType.SWARMIT_NOTIFICATION_OTA_CHUNK_ACK.value
            ):
                self.last_acked_index = int.from_bytes(
                    payload[9:14], byteorder="little"
                )

    def init(self):
        digest = hashes.Hash(hashes.SHA256())
        chunks_count = int(len(self.firmware) / CHUNK_SIZE) + int(
            len(self.firmware) % CHUNK_SIZE != 0
        )
        for chunk_idx in range(chunks_count):
            if chunk_idx == chunks_count - 1:
                chunk_size = len(self.firmware) % CHUNK_SIZE
            else:
                chunk_size = CHUNK_SIZE
            data = self.firmware[
                chunk_idx * CHUNK_SIZE : chunk_idx * CHUNK_SIZE + chunk_size
            ]
            digest.update(data)
            self.chunks.append(
                DataChunk(
                    index=chunk_idx,
                    size=chunk_size,
                    data=data,
                )
            )
        print(f"Radio chunks ({CHUNK_SIZE}B): {len(self.chunks)}")
        self.fw_hash = digest.finalize()

    def _send_start_ota(self, device_id):
        buffer = bytearray()
        buffer += SWARMIT_PREAMBLE
        buffer += int(device_id, 16).to_bytes(length=8, byteorder="little")
        buffer += int(RequestType.SWARMIT_REQ_OTA_START.value).to_bytes(
            length=1, byteorder="little"
        )
        buffer += len(self.firmware).to_bytes(length=4, byteorder="little")
        buffer += self.fw_hash
        self.serial.write(hdlc_encode(buffer))

    def start_ota(self, device_ids):
        if device_ids == DEVICES_IDS:
            print("Broadcast start ota notification...")
            self._send_start_ota("0")
            self.acked_ids = []
            timeout = 0  # ms
            while timeout < 10000 and sorted(self.acked_ids) != sorted(device_ids):
                timeout += 1
                time.sleep(0.0001)
        else:
            self.acked_ids = []
            for device_id in device_ids:
                print(f"Sending start ota notification to {device_id}...")
                self._send_start_ota(device_id)
                timeout = 0  # ms
                self.start_ack_received = False
                while (
                    self.start_ack_received is False
                    and timeout < 10000
                    and device_id not in self.acked_ids
                ):
                    timeout += 1
                    time.sleep(0.0001)
        return self.acked_ids

    def send_chunk(self, chunk, device_id):
        send_time = time.time()
        send = True
        tries = 0

        def is_chunk_acknowledged():
            if device_id == "0":
                return self.last_acked_index == chunk.index and sorted(
                    self.acked_ids
                ) == sorted(DEVICES_IDS)
            else:
                return (
                    self.last_acked_index == chunk.index and device_id in self.acked_ids
                )

        self.acked_ids = []
        while tries < 3:
            if is_chunk_acknowledged():
                break
            if send is True:
                buffer = bytearray()
                buffer += SWARMIT_PREAMBLE
                buffer += int(device_id, 16).to_bytes(length=8, byteorder="little")
                buffer += int(RequestType.SWARMIT_REQ_OTA_CHUNK.value).to_bytes(
                    length=1, byteorder="little"
                )
                buffer += int(chunk.index).to_bytes(length=4, byteorder="little")
                buffer += int(chunk.size).to_bytes(length=1, byteorder="little")
                buffer += chunk.data
                self.serial.write(hdlc_encode(buffer))
                send_time = time.time()
                tries += 1
            time.sleep(0.001)
            send = time.time() - send_time > 0.1
        else:
            raise Exception(f"chunk #{chunk.index} not acknowledged. Aborting.")
        self.last_acked_index = -1
        self.last_deviceid_ack = None

    def transfer(self, device_ids):
        data_size = len(self.firmware)
        progress = tqdm(
            range(0, data_size), unit="B", unit_scale=False, colour="green", ncols=100
        )
        progress.set_description(f"Loading firmware ({int(data_size / 1024)}kB)")
        for chunk in self.chunks:
            if sorted(device_ids) == sorted(DEVICES_IDS):
                self.send_chunk(chunk, "0")
            else:
                for device_id in device_ids:
                    self.send_chunk(chunk, device_id)
            progress.update(chunk.size)
        progress.close()

    def _send_start(self, device_id):
        buffer = bytearray()
        buffer += SWARMIT_PREAMBLE
        buffer += int(device_id, 16).to_bytes(length=8, byteorder="little")
        buffer += int(RequestType.SWARMIT_REQ_EXPERIMENT_START.value).to_bytes(
            length=1, byteorder="little"
        )
        self.serial.write(hdlc_encode(buffer))

    def start(self, device_ids):
        if device_ids == DEVICES_IDS:
            self._send_start("0")
        else:
            for device_id in device_ids:
                self._send_start(device_id)


class SwarmitStopExperiment:
    """Class used to stop an experiment."""

    def __init__(self, port, baudrate):
        self.serial = SerialInterface(port, baudrate, lambda x: None)
        self.hdlc_handler = HDLCHandler()
        # Just write a single byte to fake a DotBot gateway handshake
        self.serial.write(int(PROTOCOL_VERSION).to_bytes(length=1))

    def _send_stop(self, device_id):
        buffer = bytearray()
        buffer += SWARMIT_PREAMBLE
        buffer += int(device_id, 16).to_bytes(length=8, byteorder="little")
        buffer += int(RequestType.SWARMIT_REQ_EXPERIMENT_STOP.value).to_bytes(
            length=1, byteorder="little"
        )
        self.serial.write(hdlc_encode(buffer))

    def stop(self, device_ids):
        if device_ids == DEVICES_IDS:
            self._send_stop("0")
        else:
            for device_id in device_ids:
                self._send_stop(device_id)


class SwarmitMonitorExperiment:
    """Class used to monitor an experiment."""

    def __init__(self, port, baudrate, device_ids):
        self.logger = LOGGER.bind(context=__name__)
        self.hdlc_handler = HDLCHandler()
        self.serial = SerialInterface(port, baudrate, self.on_byte_received)
        self.last_deviceid_notification = None
        self.device_ids = device_ids
        # Just write a single byte to fake a DotBot gateway handshake
        self.serial.write(int(PROTOCOL_VERSION).to_bytes(length=1))

    def on_byte_received(self, byte):
        if self.hdlc_handler is None:
            return
        self.hdlc_handler.handle_byte(byte)
        if self.hdlc_handler.state == HDLCState.READY:
            payload = self.hdlc_handler.payload
            if not payload:
                return
            deviceid = int.from_bytes(payload[0:8], byteorder="little")
            if deviceid not in self.device_ids:
                return
            event = payload[8]
            timestamp = int.from_bytes(payload[9:13], byteorder="little")
            data_size = int(payload[13])
            data = payload[14 : data_size + 14]
            logger = self.logger.bind(
                deviceid=hex(deviceid),
                notification=event,
                time=timestamp,
                data_size=data_size,
                data=data,
            )
            if event == NotificationType.SWARMIT_NOTIFICATION_EVENT_GPIO.value:
                logger.info(f"GPIO event")
            elif event == NotificationType.SWARMIT_NOTIFICATION_EVENT_LOG.value:
                logger.info(f"LOG event")

    def monitor(self):
        while True:
            time.sleep(0.01)


class SwarmitStatusExperiment:
    """Class used to get the status of experiment."""

    def __init__(self, port, baudrate):
        self.logger = LOGGER.bind(context=__name__)
        self.hdlc_handler = HDLCHandler()
        self.serial = SerialInterface(port, baudrate, self.on_byte_received)
        self.last_deviceid_notification = None
        # Just write a single byte to fake a DotBot gateway handshake
        self.serial.write(int(PROTOCOL_VERSION).to_bytes(length=1))
        self.status_data = dict.fromkeys(DEVICES_IDS, StatusType.Off)
        self.resp_ids = []
        self.table = Table()
        self.table.add_column("Device ID", style="magenta", no_wrap=True)
        self.table.add_column("Status", style="green")

    def on_byte_received(self, byte):
        if self.hdlc_handler is None:
            return
        self.hdlc_handler.handle_byte(byte)
        if self.hdlc_handler.state == HDLCState.READY:
            payload = self.hdlc_handler.payload
            if not payload:
                return
            deviceid_resp = hex(int.from_bytes(payload[0:8], byteorder="little"))
            if deviceid_resp not in self.resp_ids:
                self.resp_ids.append(deviceid_resp)
            event = payload[8]
            if event == NotificationType.SWARMIT_NOTIFICATION_STATUS.value:
                status = StatusType(payload[9])
                self.status_data[deviceid_resp] = status
                self.table.add_row(
                    deviceid_resp,
                    f"{"[bold cyan]" if status == StatusType.Running else "[bold green]"}{status.name}",
                )

    def status(self):
        buffer = bytearray()
        buffer += SWARMIT_PREAMBLE
        buffer += int("0", 16).to_bytes(length=8, byteorder="little")
        buffer += int(RequestType.SWARMIT_REQ_EXPERIMENT_STATUS.value).to_bytes(
            length=1, byteorder="little"
        )
        self.serial.write(hdlc_encode(buffer))
        timeout = 0  # ms
        with Live(self.table, refresh_per_second=4) as live:
            while timeout < 10000 and sorted(self.resp_ids) != sorted(DEVICES_IDS):
                timeout += 1
                time.sleep(0.0001)
                live.update(self.table)
            for device_id, status in self.status_data.items():
                if status != StatusType.Off:
                    continue
                self.table.add_row(device_id, f"[bold red]{status.name}")


@click.group()
@click.option(
    "-p",
    "--port",
    default=SERIAL_PORT,
    help=f"Serial port to use to send the bitstream to the gateway. Default: {SERIAL_PORT}.",
)
@click.option(
    "-b",
    "--baudrate",
    default=BAUDRATE,
    help=f"Serial port baudrate. Default: {BAUDRATE}.",
)
@click.option(
    "-d",
    "--devices",
    type=click.Choice(DEVICES_IDS),
    default=DEVICES_IDS,
    multiple=True,
)
@click.pass_context
def main(ctx, port, baudrate, devices):
    ctx.ensure_object(dict)
    ctx.obj["port"] = port
    ctx.obj["baudrate"] = baudrate
    ctx.obj["devices"] = devices


@main.command()
@click.option(
    "-y",
    "--yes",
    is_flag=True,
    help="Start the experiment without prompt.",
)
@click.argument("firmware", type=click.File(mode="rb", lazy=True), required=False)
@click.pass_context
def start(ctx, yes, firmware):
    # Disable logging configure in PyDotBot
    structlog.configure(
        wrapper_class=structlog.make_filtering_bound_logger(logging.CRITICAL),
    )
    start = time.time()
    try:
        experiment = SwarmitStartExperiment(
            ctx.obj["port"],
            ctx.obj["baudrate"],
            firmware,
        )
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        print(f"Error: {exc}")
        return
    if firmware is not None:
        print(f"Image size: {len(experiment.firmware)}B")
        print("")
        if yes is False:
            click.confirm("Do you want to continue?", default=True, abort=True)
        experiment.init()
        ids = experiment.start_ota(list(ctx.obj["devices"]))
        if sorted(ids) != sorted(ctx.obj["devices"]):
            print(
                f"Error: some acknowledgment are missing ({"|".join(sorted(set(ctx.obj["devices"]).difference(set(ids))))}). Aborting."
            )
            return
        try:
            experiment.transfer(list(ctx.obj["devices"]))
        except Exception as exc:
            print(f"Error during transfer of image: {exc}")
            return
    experiment.start(list(ctx.obj["devices"]))
    print(f"Elapsed: {time.time() - start:.3f}s")
    print("Experiment started.")


@main.command()
@click.pass_context
def stop(ctx):
    structlog.configure(
        wrapper_class=structlog.make_filtering_bound_logger(logging.CRITICAL),
    )
    try:
        experiment = SwarmitStopExperiment(
            ctx.obj["port"],
            ctx.obj["baudrate"],
        )
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        print(f"Error: {exc}")
        return
    experiment.stop(list(ctx.obj["devices"]))


@main.command()
@click.pass_context
def monitor(ctx):
    try:
        experiment = SwarmitMonitorExperiment(
            ctx.obj["port"],
            ctx.obj["baudrate"],
            list(ctx.obj["devices"]),
        )
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        print(f"Error: {exc}")
        return
    try:
        experiment.monitor()
    except KeyboardInterrupt:
        print("Stopping monitor.")


@main.command()
@click.pass_context
def status(ctx):
    structlog.configure(
        wrapper_class=structlog.make_filtering_bound_logger(logging.CRITICAL),
    )
    try:
        experiment = SwarmitStatusExperiment(
            ctx.obj["port"],
            ctx.obj["baudrate"],
        )
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        print(f"Error: {exc}")
        return
    experiment.status()


if __name__ == "__main__":
    main(obj={})
