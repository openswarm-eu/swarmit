"""Module containing the swarmit controller class."""

import dataclasses
import time
from binascii import hexlify
from dataclasses import dataclass

import serial
from cryptography.hazmat.primitives import hashes
from dotbot.logger import LOGGER
from dotbot.protocol import Frame, Packet, Payload
from dotbot.serial_interface import SerialInterfaceException, get_default_port
from rich import print
from rich.console import Console
from rich.live import Live
from rich.table import Table
from tqdm import tqdm

from testbed.swarmit.adapter import (
    GatewayAdapterBase,
    MarilibCloudAdapter,
    MarilibEdgeAdapter,
    MQTTAdapter,
    SerialAdapter,
)
from testbed.swarmit.protocol import (
    PayloadMessage,
    PayloadOTAChunkRequest,
    PayloadOTAStartRequest,
    PayloadResetRequest,
    PayloadStartRequest,
    PayloadStatusRequest,
    PayloadStopRequest,
    StatusType,
    SwarmitPayloadType,
    register_parsers,
)

CHUNK_SIZE = 128
COMMAND_TIMEOUT = 5
OTA_CHUNK_MAX_RETRIES_DEFAULT = 5
OTA_CHUNK_TIMEOUT_DEFAULT = 0.5
SERIAL_PORT_DEFAULT = get_default_port()
BROADCAST_ADDRESS = 0xFFFFFFFFFFFFFFFF


@dataclass
class DataChunk:
    """Class that holds data chunks."""

    index: int
    size: int
    data: bytes


@dataclass
class StartOtaData:
    """Class that holds start ota data."""

    chunks: int = 0
    fw_hash: bytes = b""
    addrs: list[int] = dataclasses.field(default_factory=lambda: [])


@dataclass
class Chunk:
    """Class that holds chunk status."""

    index: str = "0"
    size: str = "0B"
    acked: int = 0
    retries: int = 0

    def __repr__(self):
        return f"{dataclasses.asdict(self)}"


@dataclass
class TransferDataStatus:
    """Class that holds transfer data status for a single device."""

    chunks: list[Chunk] = dataclasses.field(default_factory=lambda: [])
    hashes_match: bool = False


@dataclass
class ResetLocation:
    """Class that holds reset location."""

    pos_x: int = 0
    pos_y: int = 0

    def __repr__(self):
        return f"(x={self.pos_x}, y={self.pos_y})"


def print_status(status_data: dict[int, StatusType]) -> None:
    """Print the status of the devices."""
    print()
    print(
        f"{len(status_data)} device{'s' if len(status_data) > 1 else ''} found"
    )
    print()
    status_table = Table()
    status_table.add_column("Device Addr", style="magenta", no_wrap=True)
    status_table.add_column("Status", style="green", justify="center")
    with Live(status_table, refresh_per_second=4) as live:
        live.update(status_table)
        for device_addr, status in sorted(status_data.items()):
            status_table.add_row(
                f"{hexlify(int(device_addr).to_bytes(8, "big")).decode()}",
                f"{'[bold cyan]' if status == StatusType.Running else '[bold green]'}{status.name}",
            )


def print_start_status(
    stopped_data: list[str], not_started: list[str]
) -> None:
    """Print the start status."""
    print("[bold]Start status:[/]")
    status_table = Table()
    status_table.add_column("Device Addr", style="magenta", no_wrap=True)
    status_table.add_column("Status", style="green", justify="center")
    with Live(status_table, refresh_per_second=4) as live:
        live.update(status_table)
        for device_addr in sorted(stopped_data):
            status_table.add_row(
                f"{hexlify(int(device_addr).to_bytes(8, "big")).decode()}",
                "[bold green]:heavy_check_mark:[/]",
            )
        for device_id in sorted(not_started):
            status_table.add_row(f"{device_id}", "[bold red]:x:[/]")


def print_stop_status(stopped_data: list[str], not_stopped: list[str]) -> None:
    """Print the stop status."""
    print("[bold]Stop status:[/]")
    status_table = Table()
    status_table.add_column("Device Addr", style="magenta", no_wrap=True)
    status_table.add_column("Status", style="green", justify="center")
    with Live(status_table, refresh_per_second=4) as live:
        live.update(status_table)
        for device_addr in sorted(stopped_data):
            status_table.add_row(
                f"{hexlify(int(device_addr).to_bytes(8, "big")).decode()}",
                "[bold green]:heavy_check_mark:[/]",
            )
        for device_addr in sorted(not_stopped):
            status_table.add_row(
                f"{hexlify(int(device_addr).to_bytes(8, "big")).decode()}",
                "[bold red]:x:[/]",
            )


def print_transfer_status(
    status: dict[str, TransferDataStatus], start_data: int
) -> None:
    """Print the transfer status."""
    print()
    print("[bold]Transfer status:[/]")
    transfer_status_table = Table()
    transfer_status_table.add_column(
        "Device Addr", style="magenta", no_wrap=True
    )
    transfer_status_table.add_column(
        "Chunks acked", style="green", justify="center"
    )
    transfer_status_table.add_column(
        "Hashes match", style="green", justify="center"
    )
    with Live(transfer_status_table, refresh_per_second=4) as live:
        live.update(transfer_status_table)
        for device_addr, status in sorted(status.items()):
            start_marker, stop_marker = (
                ("[bold green]", "[/]")
                if bool(status.hashes_match) is True
                else ("[bold red]", "[/]")
            )
            transfer_status_table.add_row(
                f"{hexlify(int(device_addr).to_bytes(8, "big")).decode()}",
                f"{len([chunk for chunk in status.chunks if bool(chunk.acked)])}/{start_data.chunks}",
                f"{start_marker}{bool(status.hashes_match)}{stop_marker}",
            )


def wait_for_done(timeout, condition_func):
    """Wait for the condition to be met."""
    while timeout > 0:
        if condition_func():
            return True
        timeout -= 0.01
        time.sleep(0.01)
    return False


@dataclass
class ControllerSettings:
    """Class that holds controller settings."""

    serial_port: str = SERIAL_PORT_DEFAULT
    serial_baudrate: int = 1000000
    mqtt_host: str = "localhost"
    mqtt_port: int = 1883
    mqtt_use_tls: bool = False
    network_id: int = 1
    adapter: str = "serial"  # or "mqtt", "marilib-edge", "marilib-cloud"
    devices: list[str] = dataclasses.field(default_factory=lambda: [])
    verbose: bool = False


class Controller:
    """Class used to control a swarm testbed."""

    def __init__(self, settings: ControllerSettings):
        self.logger = LOGGER.bind(context=__name__)
        self.settings = settings
        self._interface: GatewayAdapterBase = None
        self.status_data: dict[str, StatusType] = {}
        self.started_data: list[str] = []
        self.stopped_data: list[str] = []
        self.chunks: list[DataChunk] = []
        self.start_ota_data: StartOtaData = StartOtaData()
        self.transfer_data: dict[str, TransferDataStatus] = {}
        self._known_devices: dict[str, StatusType] = {}
        register_parsers()
        if self.settings.adapter == "mqtt":
            self._interface = MQTTAdapter(
                self.settings.mqtt_host, self.settings.mqtt_port
            )
        elif self.settings.adapter == "marilib-edge":
            self._interface = MarilibEdgeAdapter(
                self.settings.serial_port, self.settings.serial_baudrate
            )
        elif self.settings.adapter == "marilib-cloud":
            self._interface = MarilibCloudAdapter(
                self.settings.mqtt_host,
                self.settings.mqtt_port,
                self.settings.mqtt_use_tls,
                self.settings.network_id,
            )
        else:
            try:
                self._interface = SerialAdapter(
                    self.settings.serial_port, self.settings.serial_baudrate
                )
            except (
                SerialInterfaceException,
                serial.serialutil.SerialException,
            ) as exc:
                console = Console()
                console.print(f"[bold red]Error:[/] {exc}")
        self._interface.init(self.on_frame_received)

    @property
    def known_devices(self) -> dict[str, StatusType]:
        """Return the known devices."""
        if not self._known_devices:
            self._known_devices = self.status()
        return self._known_devices

    @property
    def running_devices(self) -> list[str]:
        """Return the running devices."""
        return [
            device_addr
            for device_addr, status in self.known_devices.items()
            if (
                (
                    status == StatusType.Running
                    or status == StatusType.Programming
                )
                and (
                    not self.settings.devices
                    or device_addr in self.settings.devices
                )
            )
        ]

    @property
    def resetting_devices(self) -> list[str]:
        """Return the resetting devices."""
        return [
            device_addr
            for device_addr, status in self.known_devices.items()
            if (
                status == StatusType.Resetting
                and (
                    not self.settings.devices
                    or device_addr in self.settings.devices
                )
            )
        ]

    @property
    def ready_devices(self) -> list[str]:
        """Return the ready devices."""
        return [
            device_addr
            for device_addr, status in self.known_devices.items()
            if (
                status == StatusType.Bootloader
                and (
                    not self.settings.devices
                    or device_addr in self.settings.devices
                )
            )
        ]

    @property
    def interface(self) -> GatewayAdapterBase:
        """Return the interface."""
        return self._interface

    def terminate(self):
        """Terminate the controller."""
        self.interface.close()

    def send_payload(self, destination: int, payload: Payload):
        """Send a frame to the devices."""
        self.interface.send_payload(destination, payload)

    def on_frame_received(self, header, packet: Packet):
        """Handle the received frame."""
        if self.settings.verbose:
            print()
            print(Frame(header, packet))
        if packet.payload_type < SwarmitPayloadType.SWARMIT_REQUEST_STATUS:
            return
        device_addr = f"{packet.header.source:08X}"
        if (
            packet.payload_type
            == SwarmitPayloadType.SWARMIT_NOTIFICATION_STATUS
        ):
            self.status_data.update(
                {device_addr: StatusType(packet.payload.status)}
            )
        elif (
            packet.payload_type
            == SwarmitPayloadType.SWARMIT_NOTIFICATION_STARTED
        ):
            if device_addr not in self.started_data:
                self.started_data.append(device_addr)
        elif (
            packet.payload_type
            == SwarmitPayloadType.SWARMIT_NOTIFICATION_STOPPED
        ):
            if device_addr not in self.stopped_data:
                self.stopped_data.append(device_addr)
        elif (
            packet.payload_type
            == SwarmitPayloadType.SWARMIT_NOTIFICATION_OTA_START_ACK
        ):
            if device_addr not in self.start_ota_data.ids:
                self.start_ota_data.ids.append(device_addr)
        elif (
            packet.payload_type
            == SwarmitPayloadType.SWARMIT_NOTIFICATION_OTA_CHUNK_ACK
        ):
            try:
                acked = bool(
                    self.transfer_data[device_addr]
                    .chunks[packet.payload.index]
                    .acked
                )
            except (IndexError, KeyError):
                self.logger.warning(
                    "Chunk index out of range",
                    device_addr=device_addr,
                    chunk_index=packet.payload.index,
                )
                return
            if acked is False:
                self.transfer_data[device_addr].chunks[
                    packet.payload.index
                ].acked = 1
            self.transfer_data[device_addr].hashes_match = (
                packet.payload.hashes_match
            )
        elif packet.payload_type in [
            SwarmitPayloadType.SWARMIT_NOTIFICATION_EVENT_GPIO,
            SwarmitPayloadType.SWARMIT_NOTIFICATION_EVENT_LOG,
        ]:
            if (
                self.settings.devices
                and device_addr not in self.settings.devices
            ):
                return
            logger = self.logger.bind(
                device_addr=device_addr,
                notification=SwarmitPayloadType(packet.payload_type).name,
                timestamp=packet.payload.timestamp,
                data_size=packet.payload.count,
                data=packet.payload.data,
            )
            if (
                packet.payload_type
                == SwarmitPayloadType.SWARMIT_NOTIFICATION_EVENT_GPIO
            ):
                logger.info("GPIO event")
            elif (
                packet.payload_type
                == SwarmitPayloadType.SWARMIT_NOTIFICATION_EVENT_LOG
            ):
                logger.info("LOG event")
        else:
            self.logger.error(
                "Unknown payload type", payload_type=packet.payload_type
            )

    def status(self):
        """Request the status of the testbed."""
        self.status_data: dict[str, StatusType] = {}
        self.send_payload(
            destination=BROADCAST_ADDRESS, payload=PayloadStatusRequest()
        )
        wait_for_done(COMMAND_TIMEOUT, lambda: False)
        return self.status_data

    def _send_start(self, device_addr: int):
        def is_started():
            if device_addr == BROADCAST_ADDRESS:
                return sorted(self.started_data) == sorted(self.ready_devices)
            else:
                return device_addr in self.started_data

        payload = PayloadStartRequest()
        self.send_payload(device_addr, payload)
        wait_for_done(COMMAND_TIMEOUT, is_started)

    def start(self):
        """Start the application."""
        self.started_data = []
        ready_devices = self.ready_devices
        if not self.settings.devices:
            self._send_start(BROADCAST_ADDRESS)
        else:
            for device_addr in self.settings.devices:
                if device_addr not in ready_devices:
                    continue
                self._send_start(device_addr)
        return self.started_data

    def _send_stop(self, device_addr: int):
        stoppable_devices = self.running_devices + self.resetting_devices

        def is_stopped():
            if device_addr == BROADCAST_ADDRESS:
                return sorted(self.stopped_data) == sorted(stoppable_devices)
            else:
                return device_addr in self.stopped_data

        payload = PayloadStopRequest()
        self.send_payload(device_addr, payload)
        wait_for_done(COMMAND_TIMEOUT, is_stopped)

    def stop(self):
        """Stop the application."""
        self.stopped_data = []
        stoppable_devices = self.running_devices + self.resetting_devices
        if not self.settings.devices:
            self._send_stop(BROADCAST_ADDRESS)
        else:
            for device_addr in self.settings.devices:
                if device_addr not in stoppable_devices:
                    continue
                self._send_stop(device_addr)
        return self.stopped_data

    def _send_reset(self, device_addr: int, location: ResetLocation):
        payload = PayloadResetRequest(
            pos_x=location.pos_x,
            pos_y=location.pos_y,
        )
        self.send_payload(device_addr, payload)

    def reset(self, locations: dict[str, ResetLocation]):
        """Reset the application."""
        ready_devices = self.ready_devices
        for device_addr in self.settings.devices:
            if device_addr not in ready_devices:
                continue
            print(
                f"Resetting device {device_addr} with location {locations[device_addr]}"
            )
            self._send_reset(device_addr, locations[device_addr])

    def monitor(self):
        """Monitor the testbed."""
        self.logger.info("Monitoring testbed")
        while True:
            time.sleep(0.01)

    def _send_message(self, device_addr: int, message: str):
        payload = PayloadMessage(
            count=len(message),
            message=message.encode(),
        )
        self.send_payload(device_addr, payload)

    def send_message(self, message):
        """Send a message to the devices."""
        running_devices = self.running_devices
        if not self.settings.devices:
            self._send_message(BROADCAST_ADDRESS, message)
        else:
            for addr in self.settings.devices:
                if addr not in running_devices:
                    continue
                self._send_message(addr, message)

    def _send_start_ota(self, device_addr: int, firmware: bytes):
        def is_start_ota_acknowledged():
            if device_addr == BROADCAST_ADDRESS:
                return sorted(self.start_ota_data.ids) == sorted(
                    self.ready_devices
                )
            else:
                return device_addr in self.start_ota_data.ids

        payload = PayloadOTAStartRequest(
            fw_length=len(firmware),
            fw_chunk_count=len(self.chunks),
            fw_hash=self.fw_hash,
        )
        self.send_payload(device_addr, payload)
        wait_for_done(COMMAND_TIMEOUT, is_start_ota_acknowledged)

    def start_ota(self, firmware) -> StartOtaData:
        """Start the OTA process."""
        self.start_ota_data = StartOtaData()
        self.chunks = []
        digest = hashes.Hash(hashes.SHA256())
        chunks_count = int(len(firmware) / CHUNK_SIZE) + int(
            len(firmware) % CHUNK_SIZE != 0
        )
        for chunk_idx in range(chunks_count):
            if chunk_idx == chunks_count - 1:
                chunk_size = len(firmware) % CHUNK_SIZE
            else:
                chunk_size = CHUNK_SIZE
            data = firmware[
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
        self.fw_hash = digest.finalize()
        self.start_ota_data.fw_hash = self.fw_hash
        self.start_ota_data.chunks = len(self.chunks)
        if not self.settings.devices:
            print("Broadcast start ota notification...")
            self._send_start_ota(BROADCAST_ADDRESS, firmware)
        else:
            for addr in self.settings.devices:
                print(f"Sending start ota notification to {addr}...")
                self._send_start_ota(addr, firmware)
        return self.start_ota_data

    def send_chunk(
        self, chunk: DataChunk, device_addr: int, timeout: float, retries: int
    ):
        def is_chunk_acknowledged():
            if device_addr == BROADCAST_ADDRESS:
                return sorted(self.transfer_data.keys()) == sorted(
                    self.ready_devices
                ) and all(
                    [
                        status.chunks[chunk.index].acked
                        for status in self.transfer_data.values()
                    ]
                )
            else:
                return (
                    device_addr in self.transfer_data.keys()
                    and self.transfer_data[device_addr]
                    .chunks[chunk.index]
                    .acked
                )

        send_time = time.time()
        send = True
        retries_count = 0
        while not is_chunk_acknowledged() and retries_count <= retries:
            if send is True:
                payload = PayloadOTAChunkRequest(
                    index=chunk.index,
                    count=chunk.size,
                    chunk=chunk.data,
                )
                self.send_payload(payload)
                if device_addr == BROADCAST_ADDRESS:
                    for addr in self.ready_devices:
                        self.transfer_data[addr].chunks[
                            chunk.index
                        ].retries = retries_count
                else:
                    self.transfer_data[device_addr].chunks[
                        chunk.index
                    ].retries = retries_count
                send_time = time.time()
                retries_count += 1
            time.sleep(0.001)
            send = time.time() - send_time > timeout

    def transfer(
        self,
        firmware,
        timeout=OTA_CHUNK_TIMEOUT_DEFAULT,
        retries=OTA_CHUNK_MAX_RETRIES_DEFAULT,
    ) -> dict[str, TransferDataStatus]:
        """Transfer the firmware to the devices."""
        data_size = len(firmware)
        progress = tqdm(
            range(0, data_size),
            unit="B",
            unit_scale=False,
            colour="green",
            ncols=100,
        )
        progress.set_description(
            f"Loading firmware ({int(data_size / 1024)}kB)"
        )
        self.transfer_data = {}
        devices = self.settings.devices
        if not devices:
            devices = self.ready_devices
        for device_addr in devices:
            self.transfer_data[device_addr] = TransferDataStatus()
            self.transfer_data[device_addr].chunks = [
                Chunk(index=f"{i:03d}", size=f"{self.chunks[i].size:03d}B")
                for i in range(len(self.chunks))
            ]
        for chunk in self.chunks:
            if not self.settings.devices:
                self.send_chunk(chunk, device_addr, timeout, retries)
            else:
                for addr in self.settings.devices:
                    self.send_chunk(chunk, addr, timeout, retries)
            progress.update(chunk.size)
        progress.close()
        return self.transfer_data
