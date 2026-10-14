const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

async function main() {
    let enabled = false;
    let backupBlob;
    const sent = [];
    const handlers = {};
    const fields = {
        'select.slot-select': { value: '2' },
        'select.device-function': { value: '2' },
        'select.device-pintype': { value: '1' },
        'input.device-enabled': { get checked() { return enabled; } }
    };
    const context = {
        console, Blob, setTimeout, clearTimeout,
        document: { querySelectorAll: () => [{ querySelector: selector => fields[selector] }] },
        BWF: {
            on: (key, handler) => { handlers[key] = handler; },
            send: message => sent.push(message)
        }
    };
    vm.createContext(context);
    vm.runInContext(fs.readFileSync(path.join(__dirname, '..', 'htmljs', 'src', 'js', 'script-setup.js'), 'utf8'), context);
    context.blockscreen = () => {};
    context.unblockscreen = () => {};
    context.download = blob => { backupBlob = blob; };
    context.installed_list = [{ i: 2, c: 1, b: 0, f: 2, h: 1, p: 5, x: 1, d: 1 }];

    let checkbox;
    const template = {
        cloneNode: () => ({
            querySelector: () => ({ parentNode: { appendChild: () => {} } }),
            querySelectorAll: () => []
        })
    };
    context.window = { board: 'e', pinContainer: template };
    context.document.createElement = tag => {
        const element = { appendChild: () => {} };
        if (tag === 'input') checkbox = element;
        return element;
    };
    context.document.createTextNode = text => text;
    context.document.getElementById = () => ({ appendChild: () => {} });
    for (const [deactivated, expected] of [[undefined, true], [0, true], [1, false]]) {
        context.devices.add(0, { i: 2, f: 2, h: 1, p: 5, x: 1, d: deactivated });
        assert.equal(checkbox.checked, expected);
        assert.equal(checkbox.className, 'device-enabled');
    }

    assert.equal(context.cmdfrom(0).d, 1);
    enabled = true;
    assert.equal(context.cmdfrom(0).d, 0);
    assert.equal(context.cmdfrom(0).p, 5);
    assert.equal(context.cmdfrom(0).f, 2);
    context.backup();
    assert.equal(JSON.parse(await backupBlob.text())[0].d, 1);
    context.installed_list[0].d = 0;
    context.backup();
    assert.equal(JSON.parse(await backupBlob.text())[0].d, 0);

    context.restoreJson([{ i: 2 }, { i: 3, d: 1 }]);
    assert.equal(JSON.parse(sent[0].slice(1)).d, 0);
    handlers.U();
    assert.equal(JSON.parse(sent[1].slice(1)).d, 1);
    handlers.U();
    assert.equal(handlers.U, null);

    context.list = () => sent.push('refresh');
    context.device_apply(0);
    handlers.U();
    assert.equal(sent.at(-1), 'refresh');
    console.log('PASS: disable/enable, configuration retention, backup, legacy/disabled restore, apply refresh');
}

main().catch(error => {
    console.error(error);
    process.exitCode = 1;
});
