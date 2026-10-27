#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Compile the actual DOM bridge through EM_JS, then exercise file import in Node."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='em++')
parser.add_argument('--node', default=shutil.which('node'))
args = parser.parse_args()
if not args.node:
    parser.error('Node.js is required')
root = Path(__file__).resolve().parents[1]
source = (root / 'src/browser/browser.cc').read_text()
bridge = source.split('// clang-format off\n', 1)[1].split('// clang-format on', 1)[0]
fixture = r'''
EM_JS(void, prepareDocument, (), {
  Module.bridgeHandlers = {};
  Module.bridgeCalls = [];
  Module.ccall = (name, result, types, values) => Module.bridgeCalls.push({name, values});
  globalThis.document = {
    head: {append: style => {Module.bridgeStyle = style.textContent;}},
    createElement: () => ({}),
    addEventListener: (name, handler) => {
      (Module.bridgeHandlers[name] ||= []).push(handler);
    },
    visibilityState: 'visible',
    getElementById: () => null,
    body: {style: {}}
  };
  globalThis.innerWidth = 800;
  globalThis.innerHeight = 600;
  globalThis.devicePixelRatio = 2;
  Module.skiffDom = {els: {7: {scrollTop: 123}}};
});
EM_ASYNC_JS(int, checkImport, (), {
  const check = (ok, why) => {if (!ok) throw new Error(why);};
  const deliver = async (kind, files) => {
    Module.bridgeCalls.length = 0;
    let prevented = false;
    const event = {preventDefault: () => {prevented = true;}};
    event[kind === 'drop' ? 'dataTransfer' : 'clipboardData'] = {files};
    for (const handler of Module.bridgeHandlers[kind]) handler(event);
    for (let i = 0; i < 100; ++i) {
      if (Module.bridgeCalls.some(call => call.name === 'skiff_browser_file_error' ||
          (call.name === 'skiff_browser_file' && call.values[1] === 1))) break;
      await new Promise(resolve => setTimeout(resolve, 1));
    }
    check(prevented, kind+' must prevent the default file action');
    return Module.bridgeCalls;
  };
  const file = name => ({name, arrayBuffer: async () => new Uint8Array([0, 127, 255]).buffer});
  try {
    check(Module.bridgeStyle === 'body{margin:0}', 'CSS reaches the document');
    const unsafe = 'a/b'+String.fromCharCode(92)+'c'+String.fromCharCode(0, 10, 31)+'d';
    const paths = ['/uploads/drop-1-a_b_c___d', '/uploads/drop-2-file', '/uploads/drop-3-report 😀.txt'];
    let calls = await deliver('drop', [file(unsafe), file(""), file('report 😀.txt')]);
    check(calls.length === 4, 'one callback per file and one completion');
    for (let i = 0; i < paths.length; ++i) {
      check(calls[i].name === 'skiff_browser_file' && calls[i].values[0] === paths[i] && calls[i].values[1] === 0,
        'sanitized drop path '+i);
      check(Array.from(FS.readFile(paths[i])).join(',') === '0,127,255', 'file bytes survive import');
    }
    check(calls[3].values[0] === "" && calls[3].values[1] === 1, 'drop completion');
    calls = await deliver('paste', [file('report 😀.txt')]);
    check(calls.length === 2 && calls[0].values[0] === '/uploads/drop-4-report 😀.txt' && calls[1].values[1] === 1,
      'paste has a fresh path and completion');
    calls = await deliver('drop', [{name: 'bad', arrayBuffer: async () => {throw new Error('read failed');}}]);
    check(calls.length === 1 && calls[0].name === 'skiff_browser_file_error' && calls[0].values[0] === 'read failed',
      'failed reads report the error without a completion');
    console.log('DOM bridge links; drop, paste, filenames, bytes and read errors pass');
    return 0;
  } catch (error) {
    console.error(error);
    return 1;
  }
});
int main() {
  prepareDocument();
  skiffBrowserInstall("body{margin:0}");
  skiffBrowserSelection();
  skiffBrowserZoom(1.25f);
  if (skiffBrowserWidth() != 800 || skiffBrowserHeight() != 600 ||
      skiffBrowserScale() != 2 || !skiffBrowserVisible() || skiffBrowserScroll(7) != 123) return 2;
  return checkImport();
}
'''
with tempfile.TemporaryDirectory(prefix='skiff-browser-bridge-') as directory:
    work = Path(directory)
    unit = work / 'bridge.cc'
    program = work / 'bridge.js'
    unit.write_text('#include <emscripten.h>\n' + bridge + fixture)
    subprocess.run([args.compiler, str(unit), '-O2', '-Werror=invalid-pp-token', '-pthread',
                    '-sALLOW_MEMORY_GROWTH=1', '-sASYNCIFY', '-sFORCE_FILESYSTEM=1',
                    '-sEXPORTED_RUNTIME_METHODS=ccall', '-sENVIRONMENT=node', '-sEXIT_RUNTIME=1',
                    '-o', str(program)], check=True)
    subprocess.run([args.node, str(program)], check=True)
