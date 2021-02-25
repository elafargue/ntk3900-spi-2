const spi = require("../lib/binding.js");
const assert = require("assert");

assert(spi, "The expected function is undefined");

function testConstants() {
    assert.notStrictEqual(spi.MODE.MODE_0, undefined, "MODE.MODE_0 missing");
    console.log("MODE_0:",spi.MODE.MODE_0 );
    assert.notStrictEqual(spi.MODE.MODE_1, undefined, "MODE.MODE_2 missing");
    console.log("MODE_1:",spi.MODE.MODE_1 );
    assert.notStrictEqual(spi.MODE.MODE_2, undefined, "MODE.MODE_2 missing");
    console.log("MODE_2:",spi.MODE.MODE_2 );
    assert.notStrictEqual(spi.MODE.MODE_3, undefined, "MODE.MODE_3 missing");
    console.log("MODE_3:",spi.MODE.MODE_3 );

    assert.notStrictEqual(spi.CS.high, undefined, "CS.high missing");
    console.log("CS.high:", spi.CS.high);
    assert.notStrictEqual(spi.CS.low, undefined, "CS.low missing");
    console.log("CS.low:", spi.CS.low);
    assert.notStrictEqual(spi.CS.none, undefined, "CS.none missing");
    console.log("CS.none:", spi.CS.none);

    assert.notStrictEqual(spi.ORDER.lsb, undefined, "ORDER.lsb missing");
    assert.notStrictEqual(spi.ORDER.msb, undefined, "ORDER.msb missing");
}

function testCreate()
{
    const instance =  new spi.Spi("/dev/spi1.0");
    assert.notStrictEqual(instance._spi, undefined, "Spi new failed");

    console.log("Testing Spi.mode()");
    mode = instance.mode();
    assert.strictEqual(mode, spi.MODE.MODE_0, "Default mode is not MODE_0 as expected");
    instance.mode(spi.MODE.MODE_1);
    mode = instance.mode();
    assert.strictEqual(mode, spi.MODE.MODE_1, "Could not switch to mode 1");

    console.log("Testing Spi.chipSelect()");
    cs = instance.chipSelect();
    assert.strictEqual(cs, spi.CS.low, "Default CS is not low as expected");
    instance.chipSelect(spi.CS.high);
    cs = instance.chipSelect();
    assert.strictEqual(cs, spi.CS.high, "Could not switch CS to high as expected");

    console.log("Testing Spi.bitsPerWord()");
    bpw = instance.bitsPerWord();
    assert.strictEqual(bpw, 8, "Default bits per word is not 8 as expected");
    instance.bitsPerWord(10);
    bpw = instance.bitsPerWord();
    assert.strictEqual(bpw, 10, "Could not switch bits per word to 10 as expected");

    console.log("Testing Spi.maxSpeed()");
    val = instance.maxSpeed();
    assert.strictEqual(val, 1000000, "Default max speed is not 1MHz as expected");
    instance.maxSpeed(4000000);
    val = instance.maxSpeed();
    assert.strictEqual(val, 4000000, "Could not switch max speed to 4MHz as expected");

    console.log("Testing Spi.wrPin()");
    val = instance.wrPin();
    assert.strictEqual(val, 0, "Default wr pin is not 0 as expected");
    instance.wrPin(4);
    val = instance.wrPin();
    assert.strictEqual(val, 4, "Could not switch wrPin to 4 as expected");

    console.log("Testing Spi.rdyPin()");
    val = instance.rdyPin();
    assert.strictEqual(val, 0, "Default rdyPin is not 0 as expected");
    instance.rdyPin(4);
    val = instance.rdyPin();
    assert.strictEqual(val, 4, "Could not switch rdyPin to 4 as expected");

    console.log("Testing Spi.invertRdy()");
    val = instance.invertRdy();
    assert.strictEqual(val, false, "Default invertRdy is not false as expected");
    instance.invertRdy(true);
    val = instance.invertRdy();
    assert.strictEqual(val, true, "Could not switch invertRdy to true as expected");

    console.log("Testing Spi.bSeries()");
    val = instance.bSeries();
    assert.strictEqual(val, false, "Default bSeries is not false as expected");
    instance.bSeries(true);
    val = instance.bSeries();
    assert.strictEqual(val, true, "Could not switch bSeries to true as expected");

    console.log("Testing Spi.delay()");
    val = instance.delay();
    assert.strictEqual(val, 0, "Default delay is not 0 as expected");
    instance.delay(100);
    val = instance.delay();
    assert.strictEqual(val, 100, "Could not switch delay to 100 as expected");

    console.log("Testing Spi.loopback()");
    val = instance.loopback();
    assert.strictEqual(val, false, "Default loopback is not false as expected");
    instance.loopback(true);
    val = instance.loopback();
    assert.strictEqual(val, true, "Could not switch loopback to true as expected");

    console.log("Testing Spi.bitOrder()");
    val = instance.bitOrder();
    assert.strictEqual(val, false, "Default bitRrder is not false as expected");
    instance.bitOrder(true);
    val = instance.bitOrder();
    assert.strictEqual(val, true, "Could not switch bitOrder to true as expected");

    console.log("Testing Spi.halfDuplex()");
    val = instance.halfDuplex();
    assert.strictEqual(val, false, "Default halfDuplex is not false as expected");
    instance.halfDuplex(true);
    val = instance.halfDuplex();
    assert.strictEqual(val, true, "Could not switch halfDuplex to true as expected");


}

function illegalMode() {
    const instance =  new spi.Spi("/dev/spi1.0");
    instance.mode(99);
}


console.log("Constants");
assert.doesNotThrow(testConstants, undefined, "Test constants are defined");
console.log("Create instance, test functions")
assert.doesNotThrow(testCreate, undefined, "testCreate threw an expection");
console.log("Check that illegal SPI modes are rejected");
assert.throws(illegalMode, undefined, "testCreate threw an expection");


console.log("Tests passed- everything looks OK!");