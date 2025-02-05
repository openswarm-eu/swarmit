#!/usr/bin/env python

import dataclasses
import logging
import time
from dataclasses import dataclass
from enum import Enum, IntEnum

import click
import serial
import structlog
from cryptography.hazmat.primitives import hashes
from dotbot.hdlc import HDLCHandler, HDLCState, hdlc_encode
from dotbot.logger import LOGGER
from dotbot.protocol import (
    PROTOCOL_VERSION,
    Frame,
    Header,
    Packet,
    PacketFieldMetadata,
    register_parser,
)
from dotbot.serial_interface import (
    SerialInterface,
    SerialInterfaceException,
    get_default_port,
)
from rich.console import Console
from rich.live import Live
from rich.table import Table
from tqdm import tqdm

SERIAL_PORT = get_default_port()
BAUDRATE = 1000000
CHUNK_SIZE = 128


class StatusType(Enum):
    """Types of device status."""

    Ready = 0
    Running = 1
    Off = 2


class SwarmitPayloadType(IntEnum):
    """Types of DotBot payload types."""

    SWARMIT_REQUEST_STATUS = 0x80
    SWARMIT_REQUEST_START = 0x81
    SWARMIT_REQUEST_STOP = 0x82
    SWARMIT_REQUEST_OTA_START = 0x83
    SWARMIT_REQUEST_OTA_CHUNK = 0x84
    SWARMIT_NOTIFICATION_STATUS = 0x85
    SWARMIT_NOTIFICATION_OTA_START_ACK = 0x86
    SWARMIT_NOTIFICATION_OTA_CHUNK_ACK = 0x87
    SWARMIT_NOTIFICATION_EVENT_GPIO = 0x88
    SWARMIT_NOTIFICATION_EVENT_LOG = 0x89
    SWARMIT_MESSAGE = 0x8A


@dataclass
class PayloadExperimentRequest(Packet):
    """Dataclass that holds an experiment request packet (start/stop/status)."""

    metadata: list[PacketFieldMetadata] = dataclasses.field(
        default_factory=lambda: [
            PacketFieldMetadata(name="device_id", disp="id", length=8),
        ]
    )

    device_id: int = 0x0000000000000000


@dataclass
class PayloadExperimentStatusRequest(PayloadExperimentRequest):
    """Dataclass that holds an experiment status request packet."""


@dataclass
class PayloadExperimentStartRequest(PayloadExperimentRequest):
    """Dataclass that holds an experiment start request packet."""


@dataclass
class PayloadExperimentStopRequest(PayloadExperimentRequest):
    """Dataclass that holds an experiment stop request packet."""


@dataclass
class PayloadOTAStartRequest(Packet):
    """Dataclass that holds an OTA start packet."""

    metadata: list[PacketFieldMetadata] = dataclasses.field(
        default_factory=lambda: [
            PacketFieldMetadata(name="device_id", disp="id", length=8),
            PacketFieldMetadata(name="fw_length", disp="len.", length=4),
            PacketFieldMetadata(name="fw_hash", disp="hash.", type_=bytes, length=32),
        ]
    )

    device_id: int = 0x0000000000000000
    fw_length: int = 0
    fw_hash: bytes = dataclasses.field(default_factory=lambda: bytearray)


@dataclass
class PayloadOTAChunkRequest(Packet):
    """Dataclass that holds an OTA chunk packet."""

    metadata: list[PacketFieldMetadata] = dataclasses.field(
        default_factory=lambda: [
            PacketFieldMetadata(name="device_id", disp="id", length=8),
            PacketFieldMetadata(name="index", disp="idx", length=4),
            PacketFieldMetadata(name="count", disp="size"),
            PacketFieldMetadata(name="chunk", type_=bytes, length=0),
        ]
    )

    device_id: int = 0x0000000000000000
    index: int = 0
    count: int = 0
    chunk: bytes = dataclasses.field(default_factory=lambda: bytearray)


@dataclass
class PayloadExperimentStatusNotification(Packet):
    """Dataclass that holds an experiment status notification packet."""

    metadata: list[PacketFieldMetadata] = dataclasses.field(
        default_factory=lambda: [
            PacketFieldMetadata(name="device_id", disp="id", length=8),
            PacketFieldMetadata(name="status", disp="st."),
        ]
    )

    device_id: int = 0x0000000000000000
    status: StatusType = StatusType.Ready


@dataclass
class PayloadExperimentOTAStartAckNotification(Packet):
    """Dataclass that holds an experiment OTA start ACK notification packet."""

    metadata: list[PacketFieldMetadata] = dataclasses.field(
        default_factory=lambda: [
            PacketFieldMetadata(name="device_id", disp="id", length=8),
        ]
    )

    device_id: int = 0x0000000000000000


@dataclass
class PayloadExperimentOTAChunkAckNotification(Packet):
    """Dataclass that holds an experiment OTA chunk ACK notification packet."""

    metadata: list[PacketFieldMetadata] = dataclasses.field(
        default_factory=lambda: [
            PacketFieldMetadata(name="device_id", disp="id", length=8),
            PacketFieldMetadata(name="index", disp="idx"),
        ]
    )

    device_id: int = 0x0000000000000000
    index: int = 0


@dataclass
class PayloadExperimentEventNotification(Packet):
    """Dataclass that holds an event notification packet."""

    metadata: list[PacketFieldMetadata] = dataclasses.field(
        default_factory=lambda: [
            PacketFieldMetadata(name="device_id", disp="id", length=8),
            PacketFieldMetadata(name="timestamp", disp="ts", length=4),
            PacketFieldMetadata(name="count", disp="len."),
            PacketFieldMetadata(name="data", disp="data", type_=bytes, length=0),
        ]
    )

    device_id: int = 0x0000000000000000
    timestamp: int = 0
    count: int = 0
    data: bytes = dataclasses.field(default_factory=lambda: bytearray)


@dataclass
class PayloadExperimentMessage(Packet):
    """Dataclass that holds a message packet."""

    metadata: list[PacketFieldMetadata] = dataclasses.field(
        default_factory=lambda: [
            PacketFieldMetadata(name="device_id", disp="id", length=8),
            PacketFieldMetadata(name="count", disp="len."),
            PacketFieldMetadata(name="message", disp="msg", type_=bytes, length=0),
        ]
    )

    device_id: int = 0x0000000000000000
    count: int = 0
    message: bytes = dataclasses.field(default_factory=lambda: bytearray)


# Register all swarmit specific parsers at module level
register_parser(
    SwarmitPayloadType.SWARMIT_REQUEST_STATUS, PayloadExperimentStatusRequest
)
register_parser(SwarmitPayloadType.SWARMIT_REQUEST_START, PayloadExperimentStartRequest)
register_parser(SwarmitPayloadType.SWARMIT_REQUEST_STOP, PayloadExperimentStopRequest)
register_parser(SwarmitPayloadType.SWARMIT_REQUEST_OTA_START, PayloadOTAStartRequest)
register_parser(SwarmitPayloadType.SWARMIT_REQUEST_OTA_CHUNK, PayloadOTAChunkRequest)
register_parser(
    SwarmitPayloadType.SWARMIT_NOTIFICATION_STATUS, PayloadExperimentStatusNotification
)
register_parser(
    SwarmitPayloadType.SWARMIT_NOTIFICATION_OTA_START_ACK,
    PayloadExperimentOTAStartAckNotification,
)
register_parser(
    SwarmitPayloadType.SWARMIT_NOTIFICATION_OTA_CHUNK_ACK,
    PayloadExperimentOTAChunkAckNotification,
)
register_parser(
    SwarmitPayloadType.SWARMIT_NOTIFICATION_EVENT_LOG,
    PayloadExperimentEventNotification,
)
register_parser(SwarmitPayloadType.SWARMIT_MESSAGE, PayloadExperimentMessage)


@dataclass
class DataChunk:
    """Class that holds data chunks."""

    index: int
    size: int
    data: bytes


class SwarmitFlash:
    """Class used to flash a firmware."""

    def __init__(self, port, baudrate, firmware, known_devices):
        self.serial = SerialInterface(port, baudrate, self.on_byte_received)
        self.hdlc_handler = HDLCHandler()
        self.start_ack_received = False
        self.firmware = bytearray(firmware.read()) if firmware is not None else None
        self.known_devices = known_devices
        self.last_acked_index = -1
        self.chunks = []
        self.fw_hash = None
        self.acked_ids: list[str] = []

    def on_byte_received(self, byte):
        self.hdlc_handler.handle_byte(byte)
        if self.hdlc_handler.state == HDLCState.READY:
            payload = self.hdlc_handler.payload
            if not payload:
                return
            frame = Frame().from_bytes(payload)
            if frame.payload_type not in [
                SwarmitPayloadType.SWARMIT_NOTIFICATION_OTA_START_ACK,
                SwarmitPayloadType.SWARMIT_NOTIFICATION_OTA_CHUNK_ACK,
            ]:
                return
            device_id = f"{frame.payload.device_id:08X}"
            if device_id not in self.acked_ids:
                self.acked_ids.append(device_id)
            if (
                frame.payload_type
                == SwarmitPayloadType.SWARMIT_NOTIFICATION_OTA_START_ACK
            ):
                self.start_ack_received = True
            elif (
                frame.payload_type
                == SwarmitPayloadType.SWARMIT_NOTIFICATION_OTA_CHUNK_ACK
            ):
                self.last_acked_index = frame.payload.index

    def _send_start_ota(self, device_id: str):
        payload = PayloadOTAStartRequest(
            device_id=int(device_id), fw_length=len(self.firmware), fw_hash=self.fw_hash
        )
        frame = Frame(header=Header(), payload=payload)
        self.serial.write(hdlc_encode(frame.to_bytes()))

    def init(self, device_ids: list[str]):
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
        if not device_ids:
            print("Broadcast start ota notification...")
            self._send_start_ota("0")
            self.acked_ids = []
            timeout = 0  # ms
            while timeout < 10000 and sorted(self.acked_ids) != sorted(
                self.known_devices
            ):
                timeout += 1
                time.sleep(0.0001)
        else:
            self.acked_ids = []
            for device_id in device_ids:
                print(f"Sending start ota notification to {device_id}...")
                self._send_start_ota(device_id)
                timeout = 0  # ms
                while timeout < 10000 and device_id not in self.acked_ids:
                    timeout += 1
                    time.sleep(0.0001)
        return self.acked_ids

    def send_chunk(self, chunk, device_id: str):
        send_time = time.time()
        send = True
        tries = 0

        def is_chunk_acknowledged():
            if device_id == "0":
                return self.last_acked_index == chunk.index and sorted(
                    self.acked_ids
                ) == sorted(self.known_devices)
            else:
                return (
                    self.last_acked_index == chunk.index and device_id in self.acked_ids
                )

        self.acked_ids = []
        while tries < 3:
            if is_chunk_acknowledged():
                break
            if send is True:
                payload = PayloadOTAChunkRequest(
                    device_id=device_id,
                    index=chunk.index,
                    count=chunk.size,
                    chunk=chunk.data,
                )
                frame = Frame(header=Header(), payload=payload)
                self.serial.write(hdlc_encode(frame.to_bytes()))
                send_time = time.time()
                tries += 1
            time.sleep(0.001)
            send = time.time() - send_time > 0.1
        else:
            raise Exception(f"chunk #{chunk.index} not acknowledged. Aborting.")
        self.last_acked_index = -1
        self.last_deviceid_ack = None

    def transfer(self, device_ids: list[str]):
        data_size = len(self.firmware)
        progress = tqdm(
            range(0, data_size), unit="B", unit_scale=False, colour="green", ncols=100
        )
        progress.set_description(f"Loading firmware ({int(data_size / 1024)}kB)")
        for chunk in self.chunks:
            if not device_ids:
                self.send_chunk(chunk, "0")
            else:
                for device_id in device_ids:
                    self.send_chunk(chunk, device_id)
            progress.update(chunk.size)
        progress.close()


class SwarmitStart:
    """Class used to start an experiment."""

    def __init__(self, port, baudrate, known_devices):
        self.serial = SerialInterface(port, baudrate, lambda x: None)
        self.known_devices = known_devices
        self.hdlc_handler = HDLCHandler()
        # Just write a single byte to fake a DotBot gateway handshake
        self.serial.write(int(PROTOCOL_VERSION).to_bytes(length=1))

    def _send_start(self, device_id):
        payload = PayloadExperimentStartRequest(device_id=device_id)
        frame = Frame(header=Header(), payload=payload)
        self.serial.write(hdlc_encode(frame.to_bytes()))

    def start(self, device_ids):
        if not device_ids:
            self._send_start("0")
        else:
            for device_id in device_ids:
                if device_id not in self.known_devices:
                    continue
                self._send_start(device_id)


class SwarmitStop:
    """Class used to stop an experiment."""

    def __init__(self, port, baudrate, known_devices):
        self.serial = SerialInterface(port, baudrate, lambda x: None)
        self.known_devices = known_devices
        self.hdlc_handler = HDLCHandler()
        # Just write a single byte to fake a DotBot gateway handshake
        self.serial.write(int(PROTOCOL_VERSION).to_bytes(length=1))

    def _send_stop(self, device_id):
        payload = PayloadExperimentStopRequest(device_id=device_id)
        frame = Frame(header=Header(), payload=payload)
        self.serial.write(hdlc_encode(frame.to_bytes()))

    def stop(self, device_ids):
        if not device_ids:
            self._send_stop("0")
        else:
            for device_id in device_ids:
                if device_id not in self.known_devices:
                    continue
                self._send_stop(device_id)


class SwarmitMonitor:
    """Class used to monitor an experiment."""

    def __init__(self, port, baudrate, device_ids: list[str]):
        self.logger = LOGGER.bind(context=__name__)
        self.hdlc_handler = HDLCHandler()
        self.serial = SerialInterface(port, baudrate, self.on_byte_received)
        self.last_deviceid_notification = None
        self.device_ids: list[str] = device_ids
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
            frame = Frame().from_bytes(payload)
            if frame.payload_type not in [
                SwarmitPayloadType.SWARMIT_NOTIFICATION_EVENT_GPIO,
                SwarmitPayloadType.SWARMIT_NOTIFICATION_EVENT_LOG,
            ]:
                return
            deviceid = f"{frame.payload.device_id:08X}"
            if self.device_ids and deviceid not in self.device_ids:
                return
            logger = self.logger.bind(
                deviceid=deviceid,
                notification=frame.payload_type.name,
                time=frame.payload.timestamp,
                data_size=frame.payload.count,
                data=frame.payload.data,
            )
            if frame.payload_type == SwarmitPayloadType.SWARMIT_NOTIFICATION_EVENT_GPIO:
                logger.info(f"GPIO event")
            elif (
                frame.payload_type == SwarmitPayloadType.SWARMIT_NOTIFICATION_EVENT_LOG
            ):
                logger.info(f"LOG event")

    def monitor(self):
        while True:
            time.sleep(0.01)


class SwarmitStatus:
    """Class used to get the status of experiment."""

    def __init__(self, port, baudrate):
        self.logger = LOGGER.bind(context=__name__)
        self.hdlc_handler = HDLCHandler()
        self.last_deviceid_notification = None
        self.status_data = {}
        self.resp_ids: list[str] = []
        self.table = Table()
        self.table.add_column("Device ID", style="magenta", no_wrap=True)
        self.table.add_column("Status", style="green")
        self.serial = SerialInterface(port, baudrate, self.on_byte_received)
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
            frame = Frame().from_bytes(payload)
            if frame.payload_type != SwarmitPayloadType.SWARMIT_NOTIFICATION_STATUS:
                return
            device_id = f"{frame.payload.device_id:08X}"
            if device_id not in self.resp_ids:
                self.resp_ids.append(device_id)
            status = StatusType(frame.payload.status)
            self.status_data[device_id] = status
            self.table.add_row(
                f"0x{device_id}",
                f'{"[bold cyan]" if status == StatusType.Running else "[bold green]"}{status.name}',
            )

    def status(self, display=True):
        payload = PayloadExperimentStatusRequest(device_id=0)
        frame = Frame(header=Header(), payload=payload)
        self.serial.write(hdlc_encode(frame.to_bytes()))
        timeout = 0  # ms
        while timeout < 2000:
            timeout += 1
            time.sleep(0.0001)
        if self.status_data and display is True:
            with Live(self.table, refresh_per_second=4) as live:
                live.update(self.table)
                for device_id, status in self.status_data.items():
                    if status != StatusType.Off:
                        continue
                    self.table.add_row(device_id, f"[bold red]{status.name}")
        return self.status_data


class SwarmitMessage:
    """Class used to send a message to devices running a non-secure image."""

    def __init__(self, port, baudrate, device_ids):
        self.logger = LOGGER.bind(context=__name__)
        self.hdlc_handler = HDLCHandler()
        self.serial = SerialInterface(port, baudrate, lambda x: None)
        self.last_deviceid_notification = None
        self.device_ids = device_ids
        # Just write a single byte to fake a DotBot gateway handshake
        self.serial.write(int(PROTOCOL_VERSION).to_bytes(length=1))

    def _send_message(self, device_id, message):
        payload = PayloadExperimentMessage(
            device_id=device_id,
            count=len(message),
            message=message.encode(),
        )
        frame = Frame(header=Header(), payload=payload)
        self.serial.write(hdlc_encode(frame.to_bytes()))

    def send(self, message):
        if not self.device_ids:
            self._send_message("0", message)
        else:
            for device_id in self.device_ids:
                if device_id not in self.known_devices:
                    continue
                self._send_message(device_id, message)


def swarmit_flash(port, baudrate, firmware, yes, devices, ready_devices):
    try:
        experiment = SwarmitFlash(port, baudrate, firmware, ready_devices)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print("[bold red]Error:[/] {exc}")
        return False

    start_time = time.time()
    print(f"Image size: {len(experiment.firmware)}B")
    print("")
    if yes is False:
        click.confirm("Do you want to continue?", default=True, abort=True)
    ids = experiment.init(devices)
    if (devices and sorted(ids) != sorted(devices)) or (
        not devices and sorted(ids) != sorted(ready_devices)
    ):
        console = Console()
        console.print(
            "[bold red]Error:[/] some acknowledgment are missing "
            f'({", ".join(sorted(set(ready_devices).difference(set(ids))))}). '
            "Aborting."
        )
        return False
    try:
        experiment.transfer(devices)
    except Exception as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] transfer of image failed: {exc}")
        return False
    print(f"Elapsed: {time.time() - start_time:.3f}s")
    return True


def swarmit_start(port, baudrate, devices, ready_devices):
    try:
        experiment = SwarmitStart(port, baudrate, ready_devices)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] {exc}")
        return
    experiment.start(devices)
    print("Experiment started.")


def swarmit_stop(port, baudrate, devices, running_devices):
    try:
        experiment = SwarmitStop(port, baudrate, running_devices)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] {exc}")
        return
    experiment.stop(devices)
    print("Experiment stopped.")


def swarmit_monitor(port, baudrate, devices):
    try:
        experiment = SwarmitMonitor(port, baudrate, devices)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] {exc}")
        return
    try:
        experiment.monitor()
    except KeyboardInterrupt:
        print("Stopping monitor.")


def swarmit_status(port, baudrate, display=True):
    try:
        experiment = SwarmitStatus(port, baudrate)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] {exc}")
        return {}
    result = experiment.status(display)
    experiment.serial.stop()
    return result


def swarmit_message(port, baudrate, devices, message):
    try:
        experiment = SwarmitMessage(port, baudrate, devices)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] {exc}")
        return {}
    experiment.send(message)
    print(f"Message '{message}' sent.")


@click.group(context_settings=dict(help_option_names=["-h", "--help"]))
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
    type=str,
    default="",
    help=f"Subset list of devices to interact with, separated with ,",
)
@click.pass_context
def main(ctx, port, baudrate, devices):
    if ctx.invoked_subcommand != "monitor":
        # Disable logging if not monitoring
        structlog.configure(
            wrapper_class=structlog.make_filtering_bound_logger(logging.CRITICAL),
        )
    ctx.ensure_object(dict)
    ctx.obj["port"] = port
    ctx.obj["baudrate"] = baudrate
    ctx.obj["devices"] = [e for e in devices.split(",") if e]


@main.command()
@click.pass_context
def start(ctx):
    known_devices = swarmit_status(ctx.obj["port"], ctx.obj["baudrate"], display=False)
    ready_devices = sorted(
        [
            device
            for device, status in known_devices.items()
            if status == StatusType.Ready
        ]
    )
    swarmit_start(
        ctx.obj["port"], ctx.obj["baudrate"], list(ctx.obj["devices"]), ready_devices
    )


@main.command()
@click.pass_context
def stop(ctx):
    known_devices = swarmit_status(ctx.obj["port"], ctx.obj["baudrate"], display=False)
    running_devices = sorted(
        [
            device
            for device, status in known_devices.items()
            if status == StatusType.Running
        ]
    )
    swarmit_stop(
        ctx.obj["port"], ctx.obj["baudrate"], list(ctx.obj["devices"]), running_devices
    )


@main.command()
@click.option(
    "-y",
    "--yes",
    is_flag=True,
    help="Flash the firmware without prompt.",
)
@click.option(
    "-s",
    "--start",
    is_flag=True,
    help="Start the firmware once flashed.",
)
@click.argument("firmware", type=click.File(mode="rb", lazy=True), required=False)
@click.pass_context
def flash(ctx, yes, start, firmware):
    console = Console()
    if firmware is None:
        console.print("[bold red]Error:[/] Missing firmware file. Exiting.")
        ctx.exit()
    known_devices = swarmit_status(ctx.obj["port"], ctx.obj["baudrate"], display=False)
    ready_devices = sorted(
        [
            device
            for device, status in known_devices.items()
            if status == StatusType.Ready
        ]
    )
    if not ready_devices:
        return
    if (
        swarmit_flash(
            ctx.obj["port"],
            ctx.obj["baudrate"],
            firmware,
            yes,
            list(ctx.obj["devices"]),
            ready_devices,
        )
        is False
    ):
        return
    if start is True:
        swarmit_start(
            ctx.obj["port"],
            ctx.obj["baudrate"],
            list(ctx.obj["devices"]),
            ready_devices,
        )


@main.command()
@click.pass_context
def monitor(ctx):
    swarmit_monitor(ctx.obj["port"], ctx.obj["baudrate"], ctx.obj["devices"])


@main.command()
@click.pass_context
def status(ctx):
    if not swarmit_status(ctx.obj["port"], ctx.obj["baudrate"]):
        click.echo("No devices found.")


@main.command()
@click.argument("message", type=str, required=True)
@click.pass_context
def message(ctx, message):
    swarmit_message(ctx.obj["port"], ctx.obj["baudrate"], ctx.obj["devices"], message)


if __name__ == "__main__":
    main(obj={})
