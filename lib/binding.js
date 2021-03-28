"use strict";

const _spi = require('bindings')('_spi.node');

// Consistence with the C++ part
var MODE = {
    MODE_0: _spi.SPI_MODE_0, 
    MODE_1: _spi.SPI_MODE_1,
    MODE_2: _spi.SPI_MODE_2,
    MODE_3: _spi.SPI_MODE_3
};

var CS = {
    none: _spi.SPI_NO_CS,
    high: _spi.SPI_CS_HIGH,
    low:  _spi.SPI_CS_LOW
};

var ORDER = {
    msb:  _spi.SPI_MSB == 1,
    lsb:  _spi.SPI_LSB == 1
};

var DRIVER = {
    SPIDEV: _spi.DRIVER_SPIDEV,
    BCM2835: _spi.DRIVER_BCM2835
};

function isFunction(object) {
    return object && typeof object == 'function';
}

var Spi = function(device, options, callback) {
    this._spi = new _spi.Spi();

    options = options || {}; // Default to an empty object

    for(var attrname in options) {
	var value = options[attrname];
	if (attrname in this._spi) {
	    this._spi[attrname](value);
	}
	else
	    console.log("Unknown option: " + attrname + "=" + value);
    }

    this.device = device;

    isFunction(callback) && callback(this); // TODO: Update once open is async;
}

Spi.prototype.open = function() {
    return this._spi.open(this.device);
}

Spi.prototype.close = function() {
    return this._spi.close();
}

Spi.prototype.write = function(buf, callback) {
    this._spi.transfer(buf);

    isFunction(callback) && callback(this, buf);
}

/**
 * Write a buffer without monitoring the Rdy pin. This should only be used while
 * transfering bitmap data to the screen (only the bitmap data, not the command header at
 * the start of the transfer)
 */
Spi.prototype.writeDMA = function(buf, callback) {
    this._spi.dmaTransfer(buf);

    isFunction(callback) && callback(this, buf);
}


Spi.prototype.read = function(buf, callback) {
    this._spi.transfer(new Buffer(buf.length), buf);

    isFunction(callback) && callback(this, buf);
}

Spi.prototype.transfer = function(txbuf, rxbuf, callback) {
    // tx and rx buffers need to be the same size
    this._spi.transfer(txbuf, rxbuf);

    isFunction(callback) && callback(this, rxbuf);
}

Spi.prototype.mode = function(mode) {
    if (typeof(mode) != 'undefined') {
        this._spi['mode'](mode);
	    return this._spi;
	}  else
        return this._spi['mode']();
}

Spi.prototype.chipSelect = function(cs) {
    if (typeof(cs) != 'undefined')
	if (cs == CS['none'] || cs == CS['high'] || cs == CS['low']) {
            this._spi['chipSelect'](cs);
	    return this._spi;
	}
        else {
	    console.log('Illegal chip selection');
            return -1;
	}
    else
        return this._spi['chipSelect']();
}

Spi.prototype.bitsPerWord = function(bpw) {
    if (typeof(bpw) != 'undefined')
	if (bpw > 1) {
            this._spi['bitsPerWord'](bpw);
	    return this._spi;
	}
        else {
	    console.log('Illegal bits per word');
            return -1;
	}
    else
        return this._spi['bitsPerWord']();
}

Spi.prototype.bitOrder = function(bo) {
    if (typeof(bo) != 'undefined')
	if (bo == ORDER['msb'] || bo == ORDER['lsb']) {
            this._spi['bitOrder'](bo);
	    return this._spi;
	}
        else {
	    console.log('Illegal bit order');
            return -1;
	}
    else
        return this._spi['bitOrder']();
}

Spi.prototype.maxSpeed = function(speed) {
    if (typeof(speed) != 'undefined')
	if (speed > 0) {
            this._spi['maxSpeed'](speed);
            return this._spi;
	}
        else {
	    console.log('Speed must be positive');
	    return -1;
	}	    
    else
	return this._spi['maxSpeed']();
}

Spi.prototype.delay = function(delay) {
    if (typeof(delay) != 'undefined')
	if (delay > 0) {
            this._spi['delay'](delay);
            return this._spi;
	}
        else {
	    console.log('Delay must be positive');
	    return -1;
	}	    
    else
	return this._spi['delay']();
}

Spi.prototype.driver = function(driver) {
    if (typeof(driver) != 'undefined')
	if (driver == DRIVER['BCM2835'] || driver == DRIVER['SPIDEV']) {
            this._spi['driver'](driver);
            return this._spi;
	}
        else {
	    console.log('Illegal driver setting');
	    return -1;
	}	    
    else
	return this._spi['driver']();
}


Spi.prototype.halfDuplex = function(duplex) {
    if (typeof(duplex) != 'undefined')
	if (duplex) {
	    this._spi['halfDuplex'](true);
	    return this._spi;
	}
        else {
	    this._spi['halfDuplex'](false);
            return this._spi;
	}
    else
	return this._spi['halfDuplex']();
}

Spi.prototype.loopback = function(loop) {
    if (typeof(loop) != 'undefined')
	if (loop) {
	    this._spi['loopback'](true);
	    return this._spi;
	}
        else {
	    this._spi['loopback'](false);
	    return this._spi;
	}
    else
	return this._spi['loopback']();
}

Spi.prototype.wrPin = function(pin) {
    if (typeof(pin) != 'undefined') {
        this._spi['wrPin'](pin);
    } else
    return this._spi['wrPin']();
}

Spi.prototype.rdyPin = function(pin) {
    if (typeof(pin) != 'undefined') {
        this._spi['rdyPin'](pin);
    } else
    return this._spi['rdyPin']();
}

Spi.prototype.invertRdy = function(flag) {
    if (typeof(flag) != 'undefined') {
        this._spi['invertRdy'](flag);
    } else
    return this._spi['invertRdy']();
}

Spi.prototype.bSeries = function(pin) {
    if (typeof(pin) != 'undefined') {
        this._spi['bSeries'](pin);
    } else
    return this._spi['bSeries']();
}


module.exports.MODE = MODE;
module.exports.CS = CS;
module.exports.ORDER = ORDER;
module.exports.DRIVER = DRIVER;
module.exports.Spi = Spi;
