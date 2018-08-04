import * as Bonjour from 'bonjour';
import * as inflection from 'inflection';
import * as mdnsResolver from 'mdns-resolver';
import * as WebSocket from '@oznu/ws-connect';

let Accessory, Service, Characteristic, UUIDGen;

export = (homebridge) => {
  Accessory = homebridge.platformAccessory;
  Service = homebridge.hap.Service;
  Characteristic = homebridge.hap.Characteristic;
  UUIDGen = homebridge.hap.uuid;

  homebridge.registerPlatform('homebridge-esp-pir', 'homebridge-esp-pir', PirPlatform, true);
};

class PirPlatform {
  api: any;
  log: any;
  config: any;
  accessories: any;

  constructor(log, config, api) {
    this.api = api;
    this.log = log;
    this.config = config;
    this.accessories = {};

    const bonjour = Bonjour();
    const browser = bonjour.find({ type: 'oznu-platform' });

    browser.on('up', this.foundAccessory.bind(this));

    // Check bonjour again 5 seconds after launch
    setTimeout(() => {
      browser.update();
    }, 5000);

    // Check bonjour every 60 seconds
    setInterval(() => {
      browser.update();
    }, 60000);
  }

  // Called when a device is found
  async foundAccessory(service) {
    if (service.txt.type && service.txt.type === 'pir') {
      const UUID = UUIDGen.generate(service.txt.mac);
      const host = await mdnsResolver.resolve4(service.host);
      const accessoryConfig = {
        host: host,
        port: service.port,
        name: service.name,
        serial: service.txt.mac,
        noMotionDelay: this.config.noMotionDelay
      };

      if (!this.accessories[UUID]) {
        // New Accessory
        this.log(`Found new PIR Sensor at ${service.host}:${service.port} [${service.txt.mac}]`);
        this.accessories[UUID] = new Accessory(service.txt.mac.replace(/:/g, ''), UUID);
        this.startAccessory(this.accessories[UUID], accessoryConfig);
        this.api.registerPlatformAccessories('homebridge-esp-pir', 'homebridge-esp-pir', [this.accessories[UUID]]);
      } else {
        // Existing Accessory
        this.log(`Found existing PIR Sensor at ${service.host}:${service.port} [${service.txt.mac}]`);
        this.startAccessory(this.accessories[UUID], accessoryConfig);
      }
    }
  }

  // Called when a cached accessory is loaded
  configureAccessory(accessory) {
    this.accessories[accessory.UUID] = accessory;
  }

  // Start accessory service
  async startAccessory(accessory, config) {
    const device = new PirPlatformAccessory(this.log, accessory, config);

    // Thermostat Accessory Information
    accessory.getService(Service.AccessoryInformation)
      .setCharacteristic(Characteristic.Manufacturer, 'oznu-platform')
      .setCharacteristic(Characteristic.Model, 'homebridge-esp-pir')
      .setCharacteristic(Characteristic.SerialNumber, config.serial);

    // Thermostat Characteristic Handlers
    accessory.getService(Service.MotionSensor)
      .getCharacteristic(Characteristic.MotionDetected)
      .on('get', device.getMotionDetected.bind(device));

    // Update reachability
    accessory.updateReachability(true);
  }
}

class PirPlatformAccessory {
  accessory: any;
  config: any;
  name: any;
  log: any;
  service: any;
  pir: any;
  motion = false;

  constructor(log, accessory, config) {
    this.accessory = accessory;
    this.config = config;
    this.name = `${inflection.titleize(this.config.name.replace(/-/g, ' '))}`;
    this.log = (msg) => log(`[${this.name}] ${msg}`);

    // Setup Base Service
    this.service = accessory.getService(Service.MotionSensor) ?
      accessory.getService(Service.MotionSensor) : accessory.addService(Service.MotionSensor, this.name);

    // Setup WebSocket
    this.pir = new WebSocket(`ws://${this.config.host}:${this.config.port}`, {
      options: {
        handshakeTimeout: 2000
      }
    });

    // Setup WebSocket Handlers
    this.pir.on('websocket-status', this.log);
    this.pir.on('json', this.parseCurrentState.bind(this));

    this.pir.on('open', () => {
      this.pir.sendJson({
        noMotionDelay: config.noMotionDelay || 30000
      });
    });
  }

  parseCurrentState(res) {
    this.motion = res.motion;
    this.service.updateCharacteristic(Characteristic.MotionDetected, this.motion);
    this.log(`Motion: ${this.motion}`);
  }

  getMotionDetected(callback) {
    this.log('Called getMotionDetected', this.motion);
    callback(null, this.motion);
  }
}
