 # SwarmIT

 SwarmIT provides a embedded C port for nRF53 as well as Python based services to
 easily build and deploy a robotic swarm infrastructure testbed.
 ARM TrustZone is used to create a sandboxed user environment on each device
 under test, without requiring a control co-processor attached to it.

## Features

- Experiment management: start, stop, monitor and status check
- Deploy a custom firmware on all or on a subset of robots of a swarm testbed
- Resilient robot state: even when crashed by buggy user code, the robot can be reprogrammed remotely and wirelessly

## Usage

### Embedded C code

SwarmIT embedded C code can be built using
[Segger Embedded Studio (SES)](https://www.segger.com/products/development-tools/embedded-studio/).

To provision a device, follow the following steps:
1. open [netcore.emProject](device/network_core/netcore.emProject)
and [bootloader.emProject](device/bootloader/bootloader.emProject) in SES
2. build and load the netcore application on the nRF53 network core,
3. build and load the bootloader application on the nRF53 application core.

The device is now ready.

### Gateway

The communication between the computer and the swarm devices is performed via a
gateway board connected via USB to the computer.
The gateway board is a Nordic nRF53840DK.

The firmware to run on the gateway can also be compiled and flash using SES.
The SES project to open is located at [gateway.emProject](gateway/gateway.emProject).

After flashing the gateway firmware, LED1 on the DK should blink fast during 1s.

### Python CLI script

The Python CLI script provides commands for flashing, starting and stopping user
code on the device, as well as monitoring and checking the status of devices
in the swarm.

The Python CLI script connects via a virtual COM port to the gateway connected to
the computer.

The Python CLI script is available on PyPI. Install it using:

```
pip install swarmit
```

Default usage:

```
swarmit --help
Usage: swarmit [OPTIONS] COMMAND [ARGS]...

Options:
  -p, --port TEXT         Serial port to use to send the bitstream to the
                          gateway. Default: /dev/ttyACM0.
  -b, --baudrate INTEGER  Serial port baudrate. Default: 1000000.
  -d, --devices TEXT      Subset list of devices to interact with, separated
                          with ,
  --help                  Show this message and exit.

Commands:
  flash
  monitor
  start
  status
  stop
```
