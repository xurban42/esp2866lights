var request = require('request');
const WebSocket = require('ws-reconnect');

var Service, Characteristic;

module.exports = function(homebridge) {
    Service = homebridge.hap.Service;
    Characteristic = homebridge.hap.Characteristic;
    homebridge.registerAccessory('homebridge-esp2866-websocket-lights', 'WebSocketLights', WebSocketLights);
    console.log('Loading WebSocketLights accessories...');
};

function WebSocketLights(log, config) {
    this.log = log;

    this.name = config.ip;

    this.pin = String(config.pin).toLowerCase();

    //100 means fully open
    this.state = undefined;

    this.ws = new WebSocket(this.name + ':81', {
        retryCount: 3, // default is 2
        reconnectInterval: 30 // default is 5
    });

    this.ws.start();

    this.ws.on('connect', function open() {
        this.log('Connected to ' + 'ws://' + this.name + ':81/')
        this.ws.socket.send(`(update-${this.pin})`);
    }.bind(this));

    this.ws.on('reconnect', function open() {
        this.log('Reconnected to ' + 'ws://' + this.name + ':81/')
    }.bind(this));

    this.ws.on("destroyed",function(){
        console.log("Destroyed");
    });

    this.init_service();
}

WebSocketLights.prototype.callBack = function(value) {
    //function that gets called by the registered ws listener
    //console.log("Got new state for blind " + value);
    this.state = parseInt(value);
    this.log('set: ', this.state)

    //also make sure this change is directly communicated to HomeKit
    this.setFromApp = true;
    this.service.getCharacteristic(Characteristic.On)
        .setValue(this.state > 0);

    this.service.getCharacteristic(Characteristic.Brightness)
        .setValue(this.state, function() {
            this.setFromApp = false;
        }.bind(this));
};

WebSocketLights.prototype.init_service = function() {
    this.service = new Service.Lightbulb(this.name);

    this.ws.on('message', function incoming(data) {  
        var payload = JSON.parse(data);

        if (payload.lightState[this.pin] !== undefined) {
            this.log('Recieved:', data)
            
            if (this.setInitialState === undefined) {
                this.setInitialState = true;
                this.log("initialized");
                    
                this.service.getCharacteristic(Characteristic.On)
                    .on('set', this.setLightPowerState.bind(this))
                    .on('get', this.getLightPowerState.bind(this))

                this.service.getCharacteristic(Characteristic.Brightness)
                    .on('set', this.setLightState.bind(this))
                    .on('get', this.getLightState.bind(this))
            }

            this.callBack(payload.lightState[this.pin]);
        }
    }.bind(this));
};

WebSocketLights.prototype.getLightState = function(callback) {
    callback(undefined, this.state);
};

WebSocketLights.prototype.getLightPowerState = function(callback) {
    callback(undefined, this.state > 0);
};

WebSocketLights.prototype.setLightState = function(value, callback) {

    //sending new state (pct closed) to app
    //added some logic to prevent a loop when the change because of external event captured by callback
    var self = this;
    
    if (this.setFromApp) {
        callback();
        return;
    }

    if(value === undefined) {
        //happens at initial load
        callback();
        return;
    }

    this.log("[Light Dimmer] iOS - send brightness message to " + this.name + ": " + value);
    
    var command = value; //Light expects a value between 0 and 100
    this.ws.socket.send(`(${this.pin})` + command);
    callback();
};

WebSocketLights.prototype.setLightPowerState = function(value, callback) {
    //sending new state (pct closed) to app
    //added some logic to prevent a loop when the change because of external event captured by callback
    var self = this;
    
    if (this.setFromApp) {
        callback();
        return;
    }

    if(value === undefined) {
        //happens at initial load
        callback();
        return;
    }

    this.log("[Light Dimmer] iOS - send on/off message to " + this.name + ": " + value);
    var command = (value == true) ? `(on-${this.pin})` : `(off-${this.pin})`;
    this.ws.socket.send(command);
    callback();
}

WebSocketLights.prototype.getServices = function() {
    return [this.service];
};