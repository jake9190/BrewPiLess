const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

const root = path.join(__dirname, '..');
const compiler = process.argv[2] || path.join(os.homedir(), '.platformio', 'packages',
    'toolchain-xtensa-esp32', 'bin', 'xtensa-esp32-elf-g++.exe');

function extractFunction(source, signature) {
    const start = source.indexOf(signature);
    assert.notEqual(start, -1, `Missing ${signature}`);
    const open = source.indexOf('{', start);
    let depth = 1;
    let end = open + 1;
    while (depth && end < source.length) {
        if (source[end] === '{') depth++;
        if (source[end] === '}') depth--;
        end++;
    }
    assert.equal(depth, 0);
    return source.slice(start, end);
}

const proxy = fs.readFileSync(path.join(root, 'src', 'BrewPiProxy.cpp'), 'utf8');
const formats = fs.readFileSync(path.join(root, 'src', 'TemperatureFormats.cpp'), 'utf8');
const definitions = fs.readFileSync(path.join(root, 'src', 'TemperatureFormats.h'), 'utf8')
    .split('#if 0')[0].replace('#include "Brewpi.h"', '').replace('#pragma once', '');
const converter = extractFunction(proxy, 'static temperature float2InternalTemp(')
    .replace('static temperature', 'constexpr temperature');
const toInternal = extractFunction(formats, 'long_temperature convertToInternalTempImpl(')
    .replace('long_temperature convertToInternalTempImpl', 'constexpr long_temperature convertToInternalTempImpl');
const constrain = extractFunction(formats, 'temperature constrainTemp16(')
    .replace('temperature constrainTemp16', 'constexpr temperature constrainTemp16');

for (const setter of ['setBeerSet', 'setFridgetSet']) {
    assert.match(extractFunction(proxy, `void BrewPiProxy::${setter}(`), /float2InternalTemp\(temp\)/);
}

// Compile the production functions as constant expressions so checks run without ESP hardware.
const inputs = [46.0, 46.1, 46.2, 46.4, 46.5, 46.6, 46.9999, 0, -0.1, -1.5, -10.6, 32, 100, -100];
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'bpl-temperature-'));
try {
    for (const unit of ['C', 'F']) {
        const checks = inputs.map(input => {
            const scaled = Math.sign(input) * Math.round(Math.abs(Math.fround(input)) * 512);
            const internal = unit === 'F' ? Math.trunc(scaled * 5 / 9) - 33678 : scaled - 24576;
            const expected = Math.max(-32767, Math.min(32767, internal));
            const literal = Number.isInteger(input) ? `${input}.0` : String(input);
            return `static_assert(float2InternalTemp(${literal}f) == ${expected}, "${unit}: ${input}");`;
        });
        const source = [
            '#include <cmath>', definitions,
            `constexpr struct { struct { char tempFormat; } cc; } tempControl = {{'${unit}'}};`,
            toInternal,
            'constexpr long_temperature convertToInternalTemp(long_temperature value) { return convertToInternalTempImpl(value, true); }',
            constrain, converter, ...checks
        ].join('\n');
        const file = path.join(directory, `temperature_${unit}.cpp`);
        fs.writeFileSync(file, source);
        execFileSync(compiler, ['-std=c++14', '-c', file, '-o', path.join(directory, `temperature_${unit}.o`)], { stdio: 'inherit' });
    }
    console.log(`PASS: ${inputs.length * 2} C++ temperature checks (C/F, decimals, integers, negatives, rounding, clamping); both setters use the converter`);
} finally {
    fs.rmSync(directory, { recursive: true, force: true });
}
