// skiff's server in the browser: a service worker that is the server -- the
// program itself loaded here (index.js, its main not run), its sessions of
// the page kept here (skiff.app's loopback) -- answering what is asked under
// ./served/ as skiff.app.serve would: the page's stream, and the forms posted
// from it. The page served, with script or without, opens as a page of its
// own, with no server and no other page behind it.
//
//   GET  served/PATH?script=0|1  the page at that path, streamed: a session made for it
//   POST served/skiff/event?s=N  a form posted from it: 204
'use strict';

// The program, of the same revision as this worker.
const version = new URL(self.location.href).searchParams.get('v');
self.Module = {noInitialRun: true, print() {}, printErr(said) { console.warn(said); }};
const running = new Promise((resolve) => { self.Module.onRuntimeInitialized = resolve; });
importScripts('index.js' + (version ? '?v=' + version : ''));

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));

const encoder = new TextEncoder();
const sessions = new Map();  // session -> its stream's controller
// A piece the program handed out: read, and freed.
const taken = (at) => {
  if (!at) return '';
  const text = Module.UTF8ToString(at);
  Module._free(at);
  return text;
};
const pump = () => {
  for (const [id, stream] of sessions) {
    const piece = taken(Module._skiff_loopback_next(id));
    if (piece) stream.enqueue(encoder.encode(piece));
  }
};
// Taken as the program settles -- an answer come, a timer gone off.
self.addEventListener('skiff-settled', pump);

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);
  const at = url.pathname.indexOf('/served/');
  if (at < 0) return;
  const rest = url.pathname.slice(at + '/served/'.length);
  const base = url.pathname.slice(0, at) + '/served/';
  if (event.request.method === 'POST' && rest === 'skiff/event') {
    event.respondWith((async () => {
      await running;
      const body = await event.request.text();
      const known = Module.ccall('skiff_loopback_post', 'number', ['number', 'string'], [Number(url.searchParams.get('s')), body]);
      pump();
      // A session this worker does not have (it was stopped, and started
      // again): gone -- the page, with script, loads itself again.
      return new Response(null, {status: known ? 204 : 410});
    })());
    return;
  }
  // What keeps this worker going while a page it serves is open: asked again
  // every ten seconds by a frame of the page's (a browser stops a worker
  // that hears nothing, and its sessions with it), with no script.
  if (event.request.method === 'GET' && rest === 'skiff/alive') {
    event.respondWith(new Response('<!doctype html><meta http-equiv="refresh" content="10">',
                                   {headers: {'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store'}}));
    return;
  }
  // Any other page under it: the page at that path (the program's route).
  if (event.request.method === 'GET' && !rest.startsWith('skiff/')) {
    event.respondWith((async () => {
      await running;
      const script = url.searchParams.get('script') !== '0';
      const id = Module.ccall('skiff_loopback_open', 'number', ['number', 'string', 'string', 'string', 'string'],
                              [script ? 1 : 0, base + 'skiff/event', base, rest === 'index.html' ? '' : rest, version || '']);
      // The stream: what the program hands out, as it comes; its session
      // let go with it.
      const stream = new ReadableStream({
        start(controller) {
          sessions.set(id, controller);
          controller.enqueue(encoder.encode(taken(Module._skiff_loopback_begin(id))));
        },
        cancel() {
          sessions.delete(id);
          Module._skiff_loopback_close(id);
        },
      });
      return new Response(stream, {headers: {'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store'}});
    })());
  }
});
