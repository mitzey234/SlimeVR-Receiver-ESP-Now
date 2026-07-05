//This class provides a quick means of getting the serial port of the dongle
const { SerialPort } = require('serialport');

const VID = '1209';
const PID = '7690';

/**
 * @returns { Promise<SerialPort | null> }
 */
async function findAndOpenESP32() {
    const ports = await SerialPort.list();
    const device = ports.find(
        port =>
            port.vendorId && port.productId &&
            port.vendorId.toLowerCase() === VID &&
            port.productId.toLowerCase() === PID
    );
    if (!device) {
        console.error('No ESP32 device with VID 0x1209 and PID 0x7690 found.');
        return;
    }

    console.log('Found device:', device.path);

    let prom = {resolve: null, reject: null};
    let promise = new Promise((resolve, reject) => {
        prom.resolve = resolve;
        prom.reject = reject;
    });

    const port = new SerialPort({ path: device.path, baudRate: 115200 });

    port.on('open', prom.resolve.bind(null, port));

    port.on('error', (e) => prom.reject.bind(null, e)());

    return promise;
}

module.exports = findAndOpenESP32;