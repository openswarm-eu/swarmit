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

from dotbot.logger import LOGGER
from dotbot.hdlc import hdlc_encode, HDLCHandler, HDLCState
from dotbot.protocol import PROTOCOL_VERSION
from dotbot.serial_interface import SerialInterface, SerialInterfaceException


SERIAL_PORT = "/dev/ttyACM0"
BAUDRATE = 1000000
CHUNK_SIZE = 128
SWARMIT_PREAMBLE = bytes([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07])

#DK
# 0x24374c76b5cf8604,

DEVICES_IDS = [
    0x61d2bc40c9864d0a, #DB_old
    # 0x7edc9c15171b71d5, #DB1
    # 0x298708dfd8e95c9b, #DB2
    # 0x6761d5b474d7becd, #DB3
    # 0xb9fa5113b820a2df, #DB4
    # 0xa2b55971529aa02c, #DB5
    # 0xde297dd2e6d83274, #DB6
    # 0xeb4dbfb6ad0f512c, #DB8
    # 0x9903ef26257feb31, #DB10
    # 0x0809c4bdd6f5e687, #DB11
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
    SWARMIT_REQ_OTA_START = 3
    SWARMIT_REQ_OTA_CHUNK = 4


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
        self.last_deviceid_ack = None
        self.chunks = []
        self.fw_hash = None
        self.device_id = None
        # Just write a single byte to fake a DotBot gateway handshake
        self.serial.write(int(PROTOCOL_VERSION).to_bytes(length=1))

    def on_byte_received(self, byte):
        self.hdlc_handler.handle_byte(byte)
        if self.hdlc_handler.state == HDLCState.READY:
            payload = self.hdlc_handler.payload
            if not payload:
                return
            self.last_deviceid_ack = int.from_bytes(payload[0:8], byteorder="little")
            if payload[8] == NotificationType.SWARMIT_NOTIFICATION_OTA_START_ACK.value:
                self.start_ack_received = True
            elif payload[8] == NotificationType.SWARMIT_NOTIFICATION_OTA_CHUNK_ACK.value:
                self.last_acked_index = int.from_bytes(payload[9:14], byteorder="little")

    def init(self):
        digest = hashes.Hash(hashes.SHA256())
        chunks_count = int(len(self.firmware) / CHUNK_SIZE) + int(len(self.firmware) % CHUNK_SIZE != 0)
        for chunk_idx in range(chunks_count):
            if chunk_idx == chunks_count - 1:
                chunk_size = len(self.firmware) % CHUNK_SIZE
            else:
                chunk_size = CHUNK_SIZE
            data = self.firmware[chunk_idx * CHUNK_SIZE : chunk_idx * CHUNK_SIZE + chunk_size]
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

    def start_ota(self):
        buffer = bytearray()
        buffer += SWARMIT_PREAMBLE
        buffer += self.device_id.to_bytes(length=8, byteorder="little")
        buffer += int(RequestType.SWARMIT_REQ_OTA_START.value).to_bytes(
            length=1, byteorder="little"
        )
        buffer += len(self.firmware).to_bytes(length=4, byteorder="little")
        buffer += self.fw_hash
        print("Sending start ota notification...")
        self.serial.write(hdlc_encode(buffer))
        timeout = 0  # ms
        self.last_deviceid_ack = None
        while self.start_ack_received is False and timeout < 10000 and self.last_deviceid_ack != self.device_id:
            timeout += 1
            time.sleep(0.0001)
        return self.start_ack_received is True

    def send_chunk(self, chunk):
        send_time = time.time()
        send = True
        tries = 0
        self.last_deviceid_ack = None
        while tries < 3:
            if self.last_acked_index == chunk.index and self.last_deviceid_ack == self.device_id:
                break
            if send is True:
                bcast_id = 0
                buffer = bytearray()
                buffer += SWARMIT_PREAMBLE
                buffer += self.device_id.to_bytes(length=8, byteorder="little")
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

    def transfer(self):
        if self.device_id is None:
            raise Exception("Device ID not set.")
        data_size = len(self.firmware)
        progress = tqdm(range(0, data_size), unit="B", unit_scale=False, colour="green", ncols=100)
        progress.set_description(f"Loading firmware ({int(data_size / 1024)}kB)")
        for chunk in self.chunks:
            self.send_chunk(chunk)
            progress.update(chunk.size)
        progress.close()

    def start(self):
        bcast_id = 0
        buffer = bytearray()
        buffer += SWARMIT_PREAMBLE
        buffer += bcast_id.to_bytes(length=8, byteorder="little")
        buffer += int(RequestType.SWARMIT_REQ_EXPERIMENT_START.value).to_bytes(
            length=1, byteorder="little"
        )
        self.serial.write(hdlc_encode(buffer))


class SwarmitStopExperiment:
    """Class used to stop an experiment."""

    def __init__(self, port, baudrate):
        self.serial = SerialInterface(port, baudrate, lambda x: None)
        self.hdlc_handler = HDLCHandler()
        # Just write a single byte to fake a DotBot gateway handshake
        self.serial.write(int(PROTOCOL_VERSION).to_bytes(length=1))

    def stop(self):
        bcast_id = 0
        buffer = bytearray()
        buffer += SWARMIT_PREAMBLE
        buffer += bcast_id.to_bytes(length=8, byteorder="little")
        buffer += int(RequestType.SWARMIT_REQ_EXPERIMENT_STOP.value).to_bytes(
            length=1, byteorder="little"
        )
        self.serial.write(hdlc_encode(buffer))


class SwarmitMonitorExperiment:
    """Class used to monitor an experiment."""

    def __init__(self, port, baudrate):
        self.logger = LOGGER.bind(context=__name__)
        self.hdlc_handler = HDLCHandler()
        self.serial = SerialInterface(port, baudrate, self.on_byte_received)
        self.last_deviceid_notification = None
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
            event = payload[8]
            timestamp = int.from_bytes(payload[9:13], byteorder="little")
            data_size = int(payload[13])
            data = payload[14:data_size + 14]
            logger = self.logger.bind(deviceid=hex(deviceid), notification=event, time=timestamp, data_size=data_size, data=data)
            if event == NotificationType.SWARMIT_NOTIFICATION_EVENT_GPIO.value:
                logger.info(f"GPIO event")
            elif event == NotificationType.SWARMIT_NOTIFICATION_EVENT_LOG.value:
                logger.info(f"LOG event")

    def monitor(self):
        while True:
            time.sleep(0.01)


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
    multiple=True
)
@click.pass_context
def main(ctx, port, baudrate, devices):
    ctx.ensure_object(dict)
    ctx.obj['port'] = port
    ctx.obj['baudrate'] = baudrate
    ctx.obj['devices'] = devices


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
            ctx.obj['port'],
            ctx.obj['baudrate'],
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
        ret = experiment.init()
        for device_id in ctx.obj["devices"]:
            print(f"Preparing device {hex(device_id)}")
            experiment.device_id = device_id
            ret = experiment.start_ota()
            if ret is False:
                print(f"Error: No start acknowledgment received from {hex(device_id)}. Aborting.")
                return
            try:
                experiment.transfer()
            except Exception as exc:
                print(f"Error during transfering image to {hex(device_id)}: {exc}")
                return
    experiment.start()
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
            ctx.obj['port'],
            ctx.obj['baudrate'],
        )
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        print(f"Error: {exc}")
        return
    experiment.stop()


@main.command()
@click.pass_context
def monitor(ctx):
    try:
        experiment = SwarmitMonitorExperiment(
            ctx.obj['port'],
            ctx.obj['baudrate'],
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


if __name__ == '__main__':
    main(obj={})
