// skiff.app.timer: an Every goes off as often as it says, counting -- all
// the times it should have, at once, where it is looked at late -- and an
// After once, after a while, and again where it is asked for again.
import std;
import skiff.model;
import skiff.app.timer;

namespace probe {
struct Tick {};
struct Once {};
struct Reactions {};
using Model = skiff::model::Model<skiff::model::Assembled<skiff::model::Tracked<skiff::app::timer::Every<Tick>>,
                                                          skiff::model::Tracked<skiff::app::timer::After<Once>>>,
                                  Reactions>;
} // namespace probe

int failures = 0;
void check(bool ok, std::string_view what) {
  std::println("{} {}", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

int main() {
  using namespace skiff::app::timer;
  using namespace std::chrono_literals;
  probe::Model m;
  Ticker<probe::Model> ticker;
  ticker.step(m);
  check(!ticker.next(), "nothing on: nothing due");
  (void)m.apply(skiff::model::over<Every<probe::Tick>>(start<probe::Tick>(10ms)));
  (void)m.apply(skiff::model::over<After<probe::Once>>(after<probe::Once>(20ms)));
  ticker.step(m);
  check(ticker.next().has_value(), "started: something due");
  std::this_thread::sleep_for(35ms);
  ticker.step(m);
  check(m.look<Every<probe::Tick>>()->count >= 3, "looked at late: every time it should have gone off, counted");
  check(m.look<After<probe::Once>>()->done, "after its while: done");
  (void)m.apply(skiff::model::over<Every<probe::Tick>>(stop<probe::Tick>()));
  const auto counted = m.look<Every<probe::Tick>>()->count;
  ticker.step(m);
  std::this_thread::sleep_for(20ms);
  ticker.step(m);
  check(m.look<Every<probe::Tick>>()->count == counted, "stopped: no more");
  (void)m.apply(skiff::model::over<After<probe::Once>>(after<probe::Once>(5ms)));
  ticker.step(m);
  check(!m.look<After<probe::Once>>()->done, "asked again: not done yet");
  std::this_thread::sleep_for(10ms);
  ticker.step(m);
  check(m.look<After<probe::Once>>()->done, "and done again");
  return failures == 0 ? 0 : 1;
}
