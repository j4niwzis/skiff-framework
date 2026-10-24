// The page's side of skiff's server in the browser (skiff-sw.js): the worker
// registered, of this page's revision, and skiff.served.show(mode) -- the page
// as it is ('page'), or served, without script ('nojs') or with ('js'): the
// window gone to the served page, this page and its program let go. The
// choice remembered: a page opened again goes where it was (index.html's
// first script, before anything of the program loads).
'use strict';
(() => {
  if (!('serviceWorker' in navigator)) return;
  const version = new URL(document.currentScript.src).searchParams.get('v');
  // Where the page may not run a service worker (a policy against it, a
  // private window): said, rather than nothing happening.
  // Registered only once served is asked for (or remembered): a page that
  // stays a page starts no worker.
  let started = null;
  const ready = () => started || (started = navigator.serviceWorker.register('skiff-sw.js' + (version ? '?v=' + version : ''))
    .then(() => navigator.serviceWorker.ready)
    .then(() => null, (failed) => String((failed && failed.message) || failed)));
  const go = (mode) => { location.href = 'served/?script=' + (mode === 'js' ? '1' : '0'); };
  window.skiff = window.skiff || {};
  window.skiff.served = {
    async show(mode) {
      if (mode === 'page') {
        localStorage.removeItem('skiff:served');
        return;
      }
      const refused = await ready();
      if (refused) {
        alert('The page cannot be served here: its service worker could not start (' + refused + ').');
        return;
      }
      localStorage.setItem('skiff:served', mode);
      go(mode);
    },
  };
  // Remembered as served, and the worker not running this page yet (a first
  // visit, or one after it was let go): gone there once it is.
  const remembered = localStorage.getItem('skiff:served');
  if (remembered) ready().then((refused) => { if (!refused) go(remembered); });
})();
