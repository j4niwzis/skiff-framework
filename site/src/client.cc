// The site's parts that are the browser's own, alone (client.js): what a
// page served with script loads -- the moving boxes, and nothing else of the
// site.
import std;
import skiff.app;
import site.page;
#include <skiff/app.h>

SKIFF_APP_CLIENT(site::clients, site::Hooks)
