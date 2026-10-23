// The site, as a program: skiff.app's. In a browser, the page over the copy
// written when the site was built, its clock moved at every frame, and the
// same tree drawn on a canvas where the backends' tabs say so; natively,
// the page written out. What it adds is site::Hooks (page.cc).
import std;
import skiff.app;
import site.page;
#include <skiff/app.h>

SKIFF_APP(site::page, site::Hooks)
