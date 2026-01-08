// Print help if -h or --help is present
if (process.argv.includes('-h') || process.argv.includes('--help')) {
        console.log(`Usage: node bulkOTAUpdate.js [--KEY=value ...]

Options (can be set in config.json or as CLI args):
    --PORT=number           UDP port to listen on
    --HOST=ip               Host IP to bind
    --OTAPORT=number        OTA port (default 3232)
    --OTAPASSWORD=string    OTA password
    --FILE=path             Path to firmware .bin file
    --SSID=string           WiFi SSID
    --PASSWORD=string       WiFi password

If config.json does not exist, an example will be generated.
CLI args override config.json values.
`);
    process.exit(0);
}

const EspOTA = require('esp-ota');
const crypto = require('crypto');
const dgram = require('dgram');
const { ReadlineParser } = require('@serialport/parser-readline')
const findAndOpenESP32 = require('./dongle');
const fs = require('fs');

const path = require('path');
const CONFIG_PATH = path.join(__dirname, 'config.json');
let config = {};
let configRequired = ["PORT", "HOST", "OTAPORT", "OTAPASSWORD", "FILE", "SSID", "PASSWORD"];

// Auto-generate example config if missing
if (!fs.existsSync(CONFIG_PATH)) {
        fs.writeFileSync(CONFIG_PATH, JSON.stringify({
            PORT: 9000,
            HOST: "192.168.1.2",
            OTAPORT: 3232,
            OTAPASSWORD: "SlimeVR-OTA",
            FILE: "./firmware.bin",
            SSID: "wifi-ssid",
            PASSWORD: "wifi-password"
        }, null, 2));
    console.error("Config file not found! Example config generated at config.json edit as needed.");
    process.exit(1);
}

// Load config from JSON file
try {
    config = JSON.parse(fs.readFileSync(CONFIG_PATH, 'utf8'));
} catch (e) {
    console.error("Error parsing config file:", e);
    process.exit(1);
}

// Override config with CLI args (format: --KEY=value)
for (let arg of process.argv.slice(2)) {
    if (arg.startsWith('--')) {
        let [key, ...rest] = arg.slice(2).split('=');
        if (key && rest.length > 0) config[key] = rest.join('=');
    }
}

// Check for missing required config values
let missing = configRequired.filter(k => !config[k]);
if (missing.length > 0) {
    console.error("Missing required config values:", missing.join(", "));
    process.exit(1);
}

const PORT = parseInt(config.PORT);
const HOST = config.HOST;
const OTAPORT = parseInt(config.OTAPORT);
const OTAPASSWORD = config.OTAPASSWORD;
const FILE = config.FILE;
const SSID = config.SSID;
const PASSWORD = config.PASSWORD;

//Check file
if (!fs.existsSync(FILE)) {
    console.error("Firmware file does not exist: ", FILE);
    process.exit(1);
}

let auth = crypto.randomBytes(16);
let verification = Buffer.concat([Buffer.from("OTAREQUEST"), auth]).toString("hex");
console.log("Generated auth token: ", auth.toString('hex'));

class Handler {
    /** @type Map<string, EspOTA> */
    trackers = new Map();

    /** @type dgram.Socket */
    server;
    
    isListening = false;

    err = null;

    hooks = [];

    constructor() {
        this.interval = setInterval(this.update.bind(this), 500);

        this.server = dgram.createSocket('udp4');
        this.server.on('listening', this.handleListening.bind(this));
        this.server.on('message', this.handleMessage.bind(this));
        this.server.on('error', this.handleServerError.bind(this));
        this.server.bind(PORT, HOST);
    }

    hook () {
        if (this.isListening) return Promise.resolve();
        if (this.err) return Promise.reject(this.err);
        let prom = {resolve: null, reject: null};
        prom.promise = new Promise((resolve, reject) => {
            prom.resolve = resolve;
            prom.reject = reject;
        });
        this.hooks.push(prom);
        return prom.promise;
    }

    handleListening () {
        this.isListening = true;
        const address = this.server.address();
        console.log(`UDP server listening on ${address.address}:${address.port}`);
        for (let hook of this.hooks) hook.resolve();
        this.hooks = [];
    }

    handleMessage (msg, rinfo) {
        console.log(`Received message from ${rinfo.address}:${rinfo.port}`);
        if (msg.toString("hex") == verification) {
            // Tracker said hello with correct verification token
            this.gotTracker(rinfo.address);
        }
    }

    handleServerError (err) {
        for (let hook of this.hooks) hook.reject(err);
        this.hooks = [];
        this.err = err;
        console.error('UDP server error:', err);
        this.server.close();
    }

    gotTracker (ip) {
        console.log(`Tracker ${ip} requested OTA update`);
        if (!this.trackers.has(ip)) {
            let ota = new EspOTA();
            this.trackers.set(ip, ota);
            ota.setPassword(OTAPASSWORD);
            ota.on('state', this.handleState.bind(this, ip));
            ota.on('progress', this.handleProgress.bind(this, ip));
            ota.uploadFirmware(FILE, ip, OTAPORT).then(this.handleComplete.bind(this, ip)).catch((error) => this.handleError(ip, error));
            return ota;
        }
    }
    
    handleProgress (ip, data) {
        let tracker = this.trackers.get(ip);
        tracker.progress = Math.round(data.sent / data.total * 100);
    }

    handleError (ip, error) {
        console.error(`Tracker ${ip} OTA error: `, error);
        this.trackers.delete(ip);
    }

    handleComplete (ip) {
        console.log(`Tracker ${ip} OTA complete.`);
        this.trackers.delete(ip);
    }

    handleState (ip, state) {
        console.log(`Tracker ${ip} state: `, state);
    }

    update () {
        if (this.trackers.size > 0) console.log(Array.from(this.trackers.values()).map(d => d.progress + "%").join(" "))
    }
}

class DongleHandler {
    started = false;

    parser = new ReadlineParser();

    constructor(port) {
        this.port = port;
        this.parser.once('data', this.online.bind(this));

        port.on('close', this.onClose.bind(this));
        port.pipe(this.parser);
        let str = 'startotaupdate ' + auth.toString("hex") + ' ' + PORT + ' ' + HOST + ' ' + SSID + '\t' + PASSWORD;
        console.log("Sending to dongle:", str);
        port.write(str + '\n', "utf8", (err) => {
            if (err) {
                console.error("Error writing to dongle port: ", err);
                return;
            }
        });
    }

    online (data) {
        if (data.indexOf("OTAUPDATESTARTED") !== -1) {
            console.log("Dongle Entered OTA mode");
            this.started = true;
            this.port.close();
        } else {
            if (data.indexOf("Unknown command") !== -1) {
                console.error("Dongle does not support OTA update command, please update the dongle firmware");
                process.exit(1);
            } else {
                console.log("Dongle ERR: ", data);
            }
            this.parser.once('data', this.online.bind(this));
        }
    }

    onClose () {
        if (!this.started) {
            console.error("Port closed before OTA could start, check the dongle");
            process.exit(1);
        }
    }
}

async function start () {
    let port;
    try {
        port = await findAndOpenESP32();
    } catch (e) {
        console.error("Error opening ESP32 dongle: ", e);
        return;
    }

    if (port == null) {
        console.error("Could not find a valid dongle, make sure your dongle is connected correctly and isnt loaded into the bootloader");
        return;
    }

    let handler = new Handler();
    try {
        await handler.hook();
    } catch (err) {
        console.error("Error starting Handler: ", err);
        return;
    }

    console.log("Handler putting dongle into OTA mode");
    let dongle = new DongleHandler(port);
}

start();

