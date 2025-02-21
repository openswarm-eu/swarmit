#!/usr/bin/env python

import logging

import click
import serial
import structlog
from dotbot.serial_interface import SerialInterfaceException, get_default_port
from rich import print
from rich.console import Console

from testbed.swarmit.controller import (
    Controller,
    ControllerSettings,
    ResetLocation,
)

SERIAL_PORT_DEFAULT = get_default_port()
BAUDRATE_DEFAULT = 1000000


@click.group(context_settings=dict(help_option_names=["-h", "--help"]))
@click.option(
    "-p",
    "--port",
    default=SERIAL_PORT_DEFAULT,
    help=f"Serial port to use to send the bitstream to the gateway. Default: {SERIAL_PORT_DEFAULT}.",
)
@click.option(
    "-b",
    "--baudrate",
    default=BAUDRATE_DEFAULT,
    help=f"Serial port baudrate. Default: {BAUDRATE_DEFAULT}.",
)
@click.option(
    "-e",
    "--edge",
    is_flag=True,
    default=False,
    help="Use MQTT adapter to communicate with an edge gateway.",
)
@click.option(
    "-d",
    "--devices",
    type=str,
    default="",
    help="Subset list of devices to interact with, separated with ,",
)
@click.pass_context
def main(ctx, port, baudrate, edge, devices):
    if ctx.invoked_subcommand != "monitor":
        # Disable logging if not monitoring
        structlog.configure(
            wrapper_class=structlog.make_filtering_bound_logger(
                logging.CRITICAL
            ),
        )
    ctx.ensure_object(dict)
    ctx.obj["port"] = port
    ctx.obj["baudrate"] = baudrate
    ctx.obj["edge"] = edge
    ctx.obj["devices"] = [e for e in devices.split(",") if e]


@main.command()
@click.pass_context
def start(ctx):
    """Start the user application."""
    try:
        settings = ControllerSettings(
            serial_port=ctx.obj["port"],
            serial_baudrate=ctx.obj["baudrate"],
            mqtt_host="argus.paris.inria.fr",
            mqtt_port=8883,
            edge=ctx.obj["edge"],
            devices=list(ctx.obj["devices"]),
        )
        controller = Controller(settings)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] {exc}")
        return
    if controller.ready_devices:
        started = controller.start()
        if started and sorted(started) == sorted(controller.ready_devices):
            print(
                "Application started with success on "
                f"[[bold cyan]{', '.join(sorted(started))}[/bold cyan]]"
            )
        else:
            print(
                f"Started: [{', '.join(sorted(started))}]\n"
                f"Not started: [{', '.join(sorted(set(controller.ready_devices).difference(set(started))))}]"
            )
    else:
        print("No device to start")
    controller.terminate()


@main.command()
@click.pass_context
def stop(ctx):
    """Stop the user application."""
    try:
        settings = ControllerSettings(
            serial_port=ctx.obj["port"],
            serial_baudrate=ctx.obj["baudrate"],
            mqtt_host="argus.paris.inria.fr",
            mqtt_port=8883,
            edge=ctx.obj["edge"],
            devices=list(ctx.obj["devices"]),
        )
        controller = Controller(settings)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] {exc}")
        return
    if controller.running_devices or controller.resetting_devices:
        stopped = controller.stop()
        if stopped and sorted(stopped) == sorted(
            controller.running_devices + controller.resetting_devices
        ):
            print(
                "Application stopped with success on "
                f"[[bold cyan]{', '.join(sorted(stopped))}[/bold cyan]]"
            )
        else:
            print(
                f"Stopped: [{', '.join(sorted(stopped))}]\n"
                f"Not stopped: [{', '.join(sorted(set(controller.running_devices + controller.resetting_devices).difference(set(stopped))))}]"
            )
    else:
        print("No device to stop")
    controller.terminate()


@main.command()
@click.argument(
    "locations",
    type=str,
)
@click.pass_context
def reset(ctx, locations):
    """Reset robots locations.

    Locations are provided as '<device_id>:<x>,<y>-<device_id>:<x>,<y>|...'
    """
    devices = ctx.obj["devices"]
    if not devices:
        print("No devices selected.")
        return
    locations = {
        location.split(':')[0]: ResetLocation(
            pos_x=int(location.split(':')[1].split(',')[0]),
            pos_y=int(location.split(':')[1].split(',')[1]),
        )
        for location in locations.split("-")
    }
    if sorted(devices) and sorted(locations.keys()) != sorted(devices):
        print("Selected devices and reset locations do not match.")
        return
    try:
        settings = ControllerSettings(
            serial_port=ctx.obj["port"],
            serial_baudrate=ctx.obj["baudrate"],
            mqtt_host="argus.paris.inria.fr",
            mqtt_port=8883,
            edge=ctx.obj["edge"],
            devices=list(ctx.obj["devices"]),
        )
        controller = Controller(settings)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] {exc}")
        return
    if not controller.ready_devices:
        print("No device to reset.")
        return
    controller.reset(locations)
    controller.terminate()


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
@click.argument("firmware", type=click.File(mode="rb"), required=False)
@click.pass_context
def flash(ctx, yes, start, firmware):
    """Flash a firmware to the robots."""
    console = Console()
    if firmware is None:
        console.print("[bold red]Error:[/] Missing firmware file. Exiting.")
        ctx.exit()

    fw = bytearray(firmware.read())
    settings = ControllerSettings(
        serial_port=ctx.obj["port"],
        serial_baudrate=ctx.obj["baudrate"],
        mqtt_host="argus.paris.inria.fr",
        mqtt_port=8883,
        edge=ctx.obj["edge"],
        devices=ctx.obj["devices"],
    )
    controller = Controller(settings)
    if not controller.ready_devices:
        console.print("[bold red]Error:[/] No ready devices found. Exiting.")
        controller.terminate()
        return
    print(
        f"Devices to flash ([bold white]{len(controller.ready_devices)}"
        f"[/bold white]): [[bold cyan]{', '.join(controller.ready_devices)}[/bold cyan]]"
    )
    print(f"Image size: [bold cyan]{len(fw)}B[/bold cyan]")
    print("")
    if yes is False:
        click.confirm("Do you want to continue?", default=True, abort=True)

    devices = controller.settings.devices
    ids = controller.start_ota(fw)
    if (devices and sorted(ids) != sorted(devices)) or (
        not devices and sorted(ids) != sorted(controller.ready_devices)
    ):
        console = Console()
        console.print(
            "[bold red]Error:[/] some acknowledgments are missing "
            f'({", ".join(sorted(set(controller.ready_devices).difference(set(ids))))}). '
            "Aborting."
        )
        raise click.Abort()
    try:
        controller.transfer(fw)
    except Exception as exc:
        controller.terminate()
        console = Console()
        console.print(f"[bold red]Error:[/] transfer of image failed: {exc}")
        raise click.Abort()

    if start is True:
        controller.start()
        print("Application started.")
    controller.terminate()


@main.command()
@click.pass_context
def monitor(ctx):
    """Monitor running applications."""
    try:
        settings = ControllerSettings(
            serial_port=ctx.obj["port"],
            serial_baudrate=ctx.obj["baudrate"],
            mqtt_host="argus.paris.inria.fr",
            mqtt_port=8883,
            edge=ctx.obj["edge"],
            devices=ctx.obj["devices"],
        )
        controller = Controller(settings)
    except (
        SerialInterfaceException,
        serial.serialutil.SerialException,
    ) as exc:
        console = Console()
        console.print(f"[bold red]Error:[/] {exc}")
        return {}
    try:
        controller.monitor()
    except KeyboardInterrupt:
        print("Stopping monitor.")
    finally:
        controller.terminate()


@main.command()
@click.pass_context
def status(ctx):
    """Print current status of the robots."""
    settings = ControllerSettings(
        serial_port=ctx.obj["port"],
        serial_baudrate=ctx.obj["baudrate"],
        mqtt_host="argus.paris.inria.fr",
        mqtt_port=8883,
        edge=ctx.obj["edge"],
        devices=ctx.obj["devices"],
    )
    controller = Controller(settings)
    if not controller.status():
        click.echo("No devices found.")
    controller.terminate()


@main.command()
@click.argument("message", type=str, required=True)
@click.pass_context
def message(ctx, message):
    """Send a custom text message to the robots."""
    settings = ControllerSettings(
        serial_port=ctx.obj["port"],
        serial_baudrate=ctx.obj["baudrate"],
        mqtt_host="argus.paris.inria.fr",
        mqtt_port=8883,
        edge=ctx.obj["edge"],
        devices=ctx.obj["devices"],
    )
    controller = Controller(settings)
    controller.send_message(message)
    controller.terminate()


if __name__ == "__main__":
    main(obj={})
