const spi = require("../lib/binding.js");
const assert = require("assert");

assert(spi, "The expected function is undefined");

function testConstants() {
    assert.notStrictEqual(spi.MODE.MODE_0, undefined, "MODE.MODE_0 missing");
    assert.notStrictEqual(spi.MODE.MODE_1, undefined, "MODE.MODE_2 missing");
    assert.notStrictEqual(spi.MODE.MODE_2, undefined, "MODE.MODE_2 missing");
    assert.notStrictEqual(spi.MODE.MODE_3, undefined, "MODE.MODE_3 missing");

    assert.notStrictEqual(spi.CS.high, undefined, "CS.high missing");
    assert.notStrictEqual(spi.CS.low, undefined, "CS.low missing");
    assert.notStrictEqual(spi.CS.none, undefined, "CS.none missing");

    assert.notStrictEqual(spi.ORDER.lsb, undefined, "ORDER.lsb missing");
    assert.notStrictEqual(spi.ORDER.msb, undefined, "ORDER.msb missing");



}

function testCreate()
{
    const result =  new spi.Spi("hello");
    assert.strictEqual(result, "world", "Unexpected value returned");
}

//assert.doesNotThrow(testCreate, undefined, "testBasic threw an expection");

assert.doesNotThrow(testConstants, undefined, "Test constants are defined");

console.log("Tests passed- everything looks OK!");