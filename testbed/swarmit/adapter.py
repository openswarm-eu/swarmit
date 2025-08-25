"""Module containing classes for interfacing with the DotBot gateway."""

import base64
import time
from abc import ABC, abstractmethod

import paho.mqtt.client as mqtt
from dotbot.hdlc import HDLCHandler, HDLCState, hdlc_encode
from dotbot.protocol import (
    Frame,
    Header,
    Packet,
    Payload,
    ProtocolPayloadParserException,
)
from dotbot.serial_interface import SerialInterface
from marilib.communication_adapter import MQTTAdapter as MarilibMQTTAdapter
from marilib.communication_adapter import SerialAdapter as MarilibSerialAdapter
from marilib.mari_protocol import MARI_BROADCAST_ADDRESS
from marilib.mari_protocol import Frame as MariFrame
from marilib.marilib_cloud import MarilibCloud
from marilib.marilib_edge import MarilibEdge
from marilib.model import EdgeEvent, MariNode
from rich import print


class GatewayAdapterBase(ABC):
    """Base class for interface adapters."""

    @abstractmethod
    def init(self, on_frame_received: callable):
        """Initialize the interface."""

    @abstractmethod
    def close(self):
        """Close the interface."""

    @abstractmethod
    def send_payload(self, payload: Payload):
        """Send payload to the interface."""


class SerialAdapter(GatewayAdapterBase):
    """Class used to interface with the serial port."""

    def __init__(self, port: str, baudrate: int):
        self.port = port
        self.baudrate = baudrate
        self.hdlc_handler = HDLCHandler()

    def on_byte_received(self, byte: bytes):
        self.hdlc_handler.handle_byte(byte)
        if self.hdlc_handler.state == HDLCState.READY:
            try:
                data = self.hdlc_handler.payload
                try:
                    frame = Frame().from_bytes(data)
                except (ValueError, ProtocolPayloadParserException) as exc:
                    print(f"[red]Error parsing frame: {exc}[/]")
                    return
            except:
                return
            self.on_frame_received(frame.header, frame.packet)

    def init(self, on_frame_received: callable):
        self.on_frame_received = on_frame_received
        self.serial = SerialInterface(
            self.port, self.baudrate, self.on_byte_received
        )
        print("[yellow]Connected to gateway[/]")
        self.serial.serial.flush()
        self.serial.write(hdlc_encode(b"\x01\xff"))

    def close(self):
        print("[yellow]Disconnect from gateway...[/]")
        self.serial.write(hdlc_encode(b"\x01\xfe"))
        self.serial.stop()

    def send_payload(self, payload: Payload):
        frame = Frame(header=Header(), packet=Packet().from_payload(payload))
        self.serial.write(hdlc_encode(frame.to_bytes()))
        self.serial.serial.flush()


class MarilibEdgeAdapter(GatewayAdapterBase):
    """Class used to interface with Marilib."""

    def on_event(self, event: EdgeEvent, event_data: MariNode | MariFrame):
        if event == EdgeEvent.NODE_JOINED:
            print("[green]Node joined:[/]", event_data)
        elif event == EdgeEvent.NODE_LEFT:
            print("[orange]Node left:[/]", event_data)
        elif event == EdgeEvent.NODE_DATA:
            try:
                packet = Packet().from_bytes(event_data.payload)
            except (ValueError, ProtocolPayloadParserException) as exc:
                print(f"[red]Error parsing packet: {exc}[/]")
                return
            self.on_frame_received(event_data.header, packet)

    def __init__(self, port: str, baudrate: int):
        self.mari = MarilibEdge(
            self.on_event, MarilibSerialAdapter(port, baudrate)
        )

    def _busy_wait(self, timeout: int):
        """Wait for the condition to be met."""
        while timeout > 0:
            self.mari.update()
            timeout -= 0.1
            time.sleep(0.1)

    def init(self, on_frame_received: callable):
        self.on_frame_received = on_frame_received
        self._busy_wait(3)
        print("[yellow]Mari nodes available:[/]")
        print(self.mari.nodes)

    def close(self):
        pass

    def send_payload(self, payload: Payload):
        self.mari.send_frame(
            dst=MARI_BROADCAST_ADDRESS,
            payload=Packet().from_payload(payload).to_bytes(),
        )


class MarilibCloudAdapter(GatewayAdapterBase):
    """Class used to interface with Marilib."""

    def on_event(self, event: EdgeEvent, event_data: MariNode | MariFrame):
        if event == EdgeEvent.NODE_JOINED:
            print("[green]Node joined:[/]", event_data)
        elif event == EdgeEvent.NODE_LEFT:
            print("[orange]Node left:[/]", event_data)
        elif event == EdgeEvent.NODE_DATA:
            try:
                packet = Packet().from_bytes(event_data.payload)
            except (ValueError, ProtocolPayloadParserException) as exc:
                print(f"[red]Error parsing packet: {exc}[/]")
                return
            self.on_frame_received(event_data.header, packet)

    def __init__(self, host: str, port: int, use_tls: bool, network_id: int):
        self.mari = MarilibCloud(
            self.on_event,
            MarilibMQTTAdapter(host, port, use_tls=use_tls, is_edge=False),
            network_id,
        )

    def _busy_wait(self, timeout):
        """Wait for the condition to be met."""
        while timeout > 0:
            self.mari.update()
            timeout -= 0.1
            time.sleep(0.1)

    def init(self, on_frame_received: callable):
        self.on_frame_received = on_frame_received
        self._busy_wait(3)
        print("[yellow]Mari nodes available:[/]")
        print(self.mari.nodes)

    def close(self):
        pass

    def send_payload(self, payload: Payload):
        self.mari.send_frame(
            dst=MARI_BROADCAST_ADDRESS,
            payload=Packet().from_payload(payload).to_bytes(),
        )


class MQTTAdapter(GatewayAdapterBase):
    """Class used to interface with MQTT."""

    def __init__(self, host: str, port: int, use_tls: bool):
        self.host = host
        self.port = port
        self.use_tls = use_tls
        self.client = None

    def on_message(self, client, userdata, message):
        try:
            data = base64.b64decode(message.payload)
            try:
                frame = Frame().from_bytes(data)
            except (ValueError, ProtocolPayloadParserException) as exc:
                print(f"[red]Error parsing frame: {exc}[/]")
                return
        except Exception as e:
            # print the error and a stacktrace
            print(f"[red]Error decoding MQTT message: {e}[/]")
            print(f"[red]Message: {message.payload}[/]")
            return
        self.on_frame_received(frame.header, frame.packet)

    def on_log(self, client, userdata, paho_log_level, messages):
        print(messages)

    def on_connect(self, client, userdata, flags, reason_code, properties):
        self.client.subscribe("/pydotbot/edge_to_controller")

    def init(self, on_frame_received: callable):
        self.on_frame_received = on_frame_received
        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            protocol=mqtt.MQTTProtocolVersion.MQTTv5,
        )
        if self.use_tls:
            self.client.tls_set_context(context=None)
        # self.client.on_log = self.on_log
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.connect(self.host, self.port, 60)
        self.client.loop_start()

    def close(self):
        self.client.disconnect()
        self.client.loop_stop()

    def send_payload(self, payload: Payload):
        frame = Frame(header=Header(), packet=Packet().from_payload(payload))
        self.client.publish(
            "/pydotbot/controller_to_edge",
            base64.b64encode(frame.to_bytes()).decode(),
        )
