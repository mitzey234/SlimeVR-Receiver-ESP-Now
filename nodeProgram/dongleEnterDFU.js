const { SerialPort } = require('serialport');

const VID = '1209';
const PID = '7690';

async function setSignals(port, signals, label) {
    return new Promise((resolve, reject) => {
        port.set(signals, err => {
            if (err) {
                console.error(`Error setting DTR/RTS (${label}):`, err);
                reject(err);
            } else {
                resolve();
            }
        });
    });
}

function wait(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

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

    const port = new SerialPort({ path: device.path, baudRate: 115200 });

    port.on('open', async () => {
        console.log('Serial port opened.');
        try {
            // esptool-style bootloader sequence (inverted logic)
            await setSignals(port, { dtr: false, rts: true }, 'step 1');
            await wait(100);
            await setSignals(port, { dtr: true, rts: true }, 'step 2');
            await wait(100);
            await setSignals(port, { dtr: true, rts: false }, 'step 3');
            await wait(100);
            await setSignals(port, { dtr: false, rts: false }, 'step 4');
            console.log('ESP32 should now be in bootloader mode.');
            process.exit(0);
        } catch (err) {
            // Error already logged in setSignals
        }
    });

    port.on('error', err => {
        console.error('Serial port error:', err);
        process.exit(1);
    });
}

findAndOpenESP32();