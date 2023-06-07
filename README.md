# nRF Connect SDK: sdk-nrf
This repository contains the core of nRF Connect SDK, including subsystems,
libraries, samples and applications.
It is also the SDK's west manifest repository, containing the nRF Connect SDK
manifest (west.yml).

## Documentation
Official documentation at:

- Latest: http://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest
- All versions: http://developer.nordicsemi.com/nRF_Connect_SDK/doc/

## Purpose
This repository exists to build the [IEEE 802.15.4 sniffer sample](./samples/peripheral/802154_sniffer). Some modifications necessary for a custom application were made, and if any further changes need to be made, that sample should be updated.

To build the application, the [nRF Connect for VS Code Extension Pack](https://marketplace.visualstudio.com/items?itemName=nordic-semiconductor.nrf-connect-extension-pack) is needed as well as [Visual Studio Code](https://code.visualstudio.com/) obviously.

In the extension menu, choose the `Open an existing application` option and select the 802154_sniffer folder. A build configuration for the nRF52840 USB dongle should already be available.

For more details on building applications, refer to the [documentation](https://nrfconnect.github.io/vscode-nrf-connect/get_started/build_app_ncs.html).

Since firmware changes should be minimal, the latest .hex file for the nRF52840 USB dongle will be provided as a [release](https://github.com/AlexStanglEmerson/sdk-nrf/releases).
