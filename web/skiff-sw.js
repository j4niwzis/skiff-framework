// skiff's server in the browser: a service worker that answers what is asked
// under ./served/ as skiff.app.serve would -- the page's stream, and the forms
// posted from it -- with the page's own program doing the server's part
// (skiff.app's loopback, through skiff-served.js in the page). With it, the
// page served live, with script or without, is seen with no server at all.
//
//   GET  served/?script=0|1      the page, streamed: a session made for it
//   POST served/skiff/event?s=N  a form posted from it: 204
'use strict';

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));

// The page that runs the program: the window that is not itself served.
const host = async () => {
  const all = await self.clients.matchAll({type: 'window', includeUncontrolled: true});
  return all.find((one) => !new URL(one.url).pathname.includes('/served/'));
};

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);
  const at = url.pathname.indexOf('/served/');
  if (at < 0) return;
  const rest = url.pathname.slice(at + '/served/'.length);
  const base = url.pathname.slice(0, at) + '/served/';
  if (event.request.method === 'POST' && rest === 'skiff/event') {
    event.respondWith((async () => {
      const page = await host();
      if (!page) return new Response('no page to serve it', {status: 503});
      const body = await event.request.text();
      const channel = new MessageChannel();
      const done = new Promise((resolve) => { channel.port1.onmessage = () => resolve(); });
      page.postMessage({post: Number(url.searchParams.get('s')), body}, [channel.port2]);
      await done;
      return new Response(null, {status: 204});
    })());
    return;
  }
  if (event.request.method === 'GET' && (rest === '' || rest === 'index.html')) {
    event.respondWith((async () => {
      const page = await host();
      if (!page) return new Response('no page to serve it', {status: 503});
      const channel = new MessageChannel();
      const encoder = new TextEncoder();
      // The stream: each piece the program hands out, as it comes; closed
      // when the frame showing it goes, and the session with it.
      const stream = new ReadableStream({
        start(controller) {
          channel.port1.onmessage = (message) => controller.enqueue(encoder.encode(message.data));
        },
        cancel() { channel.port1.postMessage('close'); },
      });
      page.postMessage({open: {script: url.searchParams.get('script') !== '0', action: base + 'skiff/event'}}, [channel.port2]);
      return new Response(stream, {headers: {'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store'}});
    })());
  }
});
