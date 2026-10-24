// The site in real browsers (Chromium and Firefox, through Playwright): the
// page; served without script and with it, by the program in the service
// worker -- the counter pressed, a thing to do added -- and served again
// after a quiet while (a browser stops a worker that hears nothing).
// node browser.mjs BASE_URL
import { chromium, firefox } from 'playwright';

const base = process.argv[2];
let failures = 0;
const check = (ok, what) => {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${what}`);
  if (!ok) failures++;
};
const shown = async (page, text, timeout = 15000) => {
  try {
    await page.getByText(text, { exact: true }).filter({ visible: true }).first().waitFor({ state: 'visible', timeout });
    return true;
  } catch {
    return false;
  }
};

for (const [name, kind] of [['chromium', chromium], ['firefox', firefox]]) {
  const browser = await kind.launch();
  const context = await browser.newContext();
  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', (error) => errors.push(String(error)));
  try {
    // The page.
    await page.goto(base);
    check(await shown(page, 'Pressed 0 times'), `${name} page: the counter`);
    await page.getByRole('button', { name: '+', exact: true }).first().click();
    check(await shown(page, 'Pressed 1 times'), `${name} page: pressed`);

    for (const mode of ['nojs', 'js']) {
      // A served page is a stream that does not end: its load is never
      // fired. Gone to, and waited for by what it shows.
      await page.goto(base + '?mode=' + mode, { waitUntil: 'commit' });
      await page.waitForURL(/\/served\//, { timeout: 30000 }).catch(() => {});
      check(page.url().includes('/served/'), `${name} ${mode}: gone to the served page`);
      check(await shown(page, 'Pressed 0 times'), `${name} ${mode}: the counter`);
      await page.getByRole('button', { name: '+', exact: true }).filter({ visible: true }).first().click();
      check(await shown(page, 'Pressed 1 times'), `${name} ${mode}: pressed`);
      check(await shown(page, 'Write the tree'), `${name} ${mode}: the list's rows`);
      await page.getByPlaceholder('Something to do').filter({ visible: true }).first().fill('milk');
      await page.getByRole('button', { name: 'Add', exact: true }).filter({ visible: true }).first().click();
      check(await shown(page, 'milk'), `${name} ${mode}: a thing to do added`);
    }
    // A quiet while, then pressed again: the worker kept going.
    await page.waitForTimeout(40000);
    await page.getByRole('button', { name: '+', exact: true }).filter({ visible: true }).first().click();
    check(await shown(page, 'Pressed 2 times'), `${name} js: pressed after a quiet while`);
    // Back to the page, by the served page's own link.
    await page.getByRole('link', { name: 'page', exact: true }).click();
    await page.waitForURL((url) => !url.pathname.includes('/served/'), { timeout: 30000 }).catch(() => {});
    check(!page.url().includes('/served/'), `${name}: back to the page`);
  } catch (failed) {
    check(false, `${name}: ${failed}`);
  }
  for (const error of errors) console.log(`  page error (${name}): ${error}`);
  await browser.close();
}
process.exit(failures ? 1 : 0);
