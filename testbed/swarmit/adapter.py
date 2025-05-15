"""Module containing classes for interfacing with the DotBot gateway."""

import base64
from abc import ABC, abstractmethod

import paho.mqtt.client as mqtt
from dotbot.hdlc import HDLCHandler, HDLCState, hdlc_encode
from dotbot.serial_interface import SerialInterface
from rich import print


class GatewayAdapterBase(ABC):
    """Base class for interface adapters."""

    @abstractmethod
    def init(self, on_data_received: callable):
        """Initialize the interface."""

    @abstractmethod
    def close(self):
        """Close the interface."""

    @abstractmethod
    def send_data(self, data):
        """Send data to the interface."""


class SerialAdapter(GatewayAdapterBase):
    """Class used to interface with the serial port."""

    def __init__(self, port, baudrate):
        self.port = port
        self.baudrate = baudrate
        self.hdlc_handler = HDLCHandler()

    def on_byte_received(self, byte):
        self.hdlc_handler.handle_byte(byte)
        if self.hdlc_handler.state == HDLCState.READY:
            try:
                payload = self.hdlc_handler.payload
                self.on_data_received(payload)
            except:
                pass

    def init(self, on_data_received: callable):
        self.serial = SerialInterface(
            self.port, self.baudrate, self.on_byte_received
        )
        self.on_data_received = on_data_received
        print("[yellow]Connecting to gateway...[/]")
        self.serial.serial.flush()
        self.serial.write(hdlc_encode(b"\x01\xff"))

    def close(self):
        print("[yellow]Disconnect from gateway...[/]")
        self.serial.write(hdlc_encode(b"\x01\xfe"))
        self.serial.stop()

    def send_data(self, data):
        self.serial.write(hdlc_encode(data))


class MQTTAdapter(GatewayAdapterBase):
    """Class used to interface with MQTT."""

    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.client = None

    def on_message(self, client, userdata, message):
        try:
            self.on_data_received(base64.b64decode(message.payload))
        except Exception as e:
            # print the error and a stacktrace
            print(f"[red]Error decoding MQTT message: {e}[/]")
            print(f"[red]Message: {message.payload}[/]")

    def on_log(self, client, userdata, paho_log_level, messages):
        print(messages)

    def on_connect(self, client, userdata, flags, reason_code, properties):
        self.client.subscribe("/pydotbot/edge_to_controller")

    def init(self, on_data_received: callable):
        self.on_data_received = on_data_received
        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            protocol=mqtt.MQTTProtocolVersion.MQTTv5,
        )
        self.client.tls_set_context(context=None)
        # self.client.on_log = self.on_log
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.connect(self.host, self.port, 60)
        self.client.loop_start()

    def close(self):
        self.client.disconnect()
        self.client.loop_stop()

    def send_data(self, data):
        self.client.publish(
            "/pydotbot/controller_to_edge",
            base64.b64encode(data).decode(),
        )
