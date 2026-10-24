// The page as its own server: what skiff-sw.js asks, answered by the page's
// program (skiff.app's loopback) -- a session opened for each stream, its
// pieces handed to the stream as the program makes them, a form posted given
// to its session. And skiff.served.show(mode): the page served, without
// script ('nojs') or with ('js'), shown over the page in a frame; or not
// ('page').
'use strict';
(() => {
  const sessions = new Map();  // session -> the stream's port
  // A piece the program handed out: read, and freed.
  const taken = (at) => {
    if (!at) return '';
    const text = Module.UTF8ToString(at);
    Module._free(at);
    return text;
  };
  const pump = () => {
    if (!Module.calledRun) return;
    for (const [id, port] of sessions) {
      const piece = taken(Module._skiff_loopback_next(id));
      if (piece) port.postMessage(piece);
    }
  };
  if (!('serviceWorker' in navigator)) return;
  // The program running: what is asked before then (a frame restored with
  // the page, asking at once) waits for it.
  const running = new Promise((resolve) => {
    if (Module.calledRun) return resolve();
    Module.postRun = [].concat(Module.postRun || [], [resolve]);
  });
  // A reload starts at the page: the switch and the frame as they were made,
  // not as the browser restored them.
  addEventListener('pageshow', () => {
    const frame = document.getElementById('skiff-served');
    const choice = document.getElementById('skiff-served-switch');
    if (frame) {
      frame.hidden = true;
      frame.src = 'about:blank';
    }
    if (choice) choice.reset();
  });
  navigator.serviceWorker.addEventListener('message', async (event) => {
    const said = event.data;
    const port = event.ports[0];
    await running;
    if (said.open) {
      const id = Module.ccall('skiff_loopback_open', 'number', ['number', 'string', 'string', 'string'],
                              [said.open.script ? 1 : 0, said.open.action, said.open.base, said.open.path]);
      sessions.set(id, port);
      port.onmessage = (message) => {
        if (message.data !== 'close') return;
        sessions.delete(id);
        Module._skiff_loopback_close(id);
      };
      port.postMessage(taken(Module._skiff_loopback_begin(id)));
    } else if (said.post) {
      Module.ccall('skiff_loopback_post', null, ['number', 'string'], [said.post, said.body]);
      port.postMessage('done');
      pump();
    }
  });
  // Taken as the program settles -- an answer come, a request's time to be
  // tried again -- not looked for at intervals.
  window.addEventListener('skiff-settled', pump);
  // Where the page may not run a service worker (a policy against it, a
  // private window): the switch says so, rather than showing nothing.
  const ready = navigator.serviceWorker.register('skiff-sw.js').then(() => navigator.serviceWorker.ready)
    .then(() => null, (failed) => String((failed && failed.message) || failed));
  window.skiff = window.skiff || {};
  window.skiff.served = {
    async show(mode) {
      const frame = document.getElementById('skiff-served');
      if (!frame) return;
      if (mode === 'page') {
        frame.hidden = true;
        frame.src = 'about:blank';
        return;
      }
      const refused = await ready;
      if (refused) {
        frame.srcdoc = '<p style="font:15px system-ui;padding:24px">The page cannot be served here: its service worker could not start (' +
          refused.replace(/[<&]/g, (c) => (c === '<' ? '&lt;' : '&amp;')) + ').</p>';
        frame.hidden = false;
        return;
      }
      frame.removeAttribute('srcdoc');
      frame.src = 'served/?script=' + (mode === 'js' ? '1' : '0') + '&at=' + Date.now();
      frame.hidden = false;
    },
  };
})();
