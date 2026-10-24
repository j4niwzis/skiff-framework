// skiff.app.timer: time, as a part of the model. A program that wants
// something to happen every so often, or once after a while, says so in its
// model -- Every<Tag> (on, how often, how many times it has gone off) or
// After<Tag> (asked for, how long, whether it has gone off) -- and the
// framework makes it happen: the part edited when its time comes, as an edit
// from the program would be, so that a view bound to it, or a reaction to
// it, moves. In a browser, woken for the soonest by one timer; served, by
// the session's loop. No loop runs for it in between.
//
//   struct Clock {};                                  // a tag, the timer's name
//   ... bound<timer::Every<Clock>>(view) ...           // its count, each second
//   model::over<timer::Every<Clock>>(timer::start<Clock>(std::chrono::seconds(1)))
export module skiff.app.timer;

import std;
import skiff.model;

export namespace skiff::app::timer {

using Clock = std::chrono::steady_clock;

template <class Tag> struct Every {
  bool on = false;
  std::chrono::milliseconds every{1000};
  std::uint64_t count = 0;  // how many times it has gone off
  friend bool operator==(const Every &, const Every &) = default;
};
template <class Tag> struct After {
  std::uint64_t round = 0;  // asked for again: one more
  std::chrono::milliseconds in{1000};
  bool done = false;
  friend bool operator==(const After &, const After &) = default;
};
// The edits a handler answers: an Every started or stopped; an After asked for.
template <class Tag> [[nodiscard]] auto start(std::chrono::milliseconds every) {
  return [every](Every<Tag> e) {
    e.on = true;
    e.every = every;
    return e;
  };
}
template <class Tag> [[nodiscard]] auto stop() {
  return [](Every<Tag> e) {
    e.on = false;
    return e;
  };
}
template <class Tag> [[nodiscard]] auto after(std::chrono::milliseconds in) {
  return [in](After<Tag> a) {
    a.in = in;
    a.done = false;
    ++a.round;
    return a;
  };
}

namespace detail {
// What the ticker keeps of each timer: when it goes off next, and for which
// asking of it.
template <class Part> struct due {
  std::optional<Clock::time_point> at;
  std::uint64_t round = 0;
  bool on = false;
};
template <class Part> struct due_of {
  using type = std::tuple<>;
};
template <class Tag> struct due_of<skiff::model::Tracked<Every<Tag>>> {
  using type = std::tuple<due<Every<Tag>>>;
};
template <class Tag> struct due_of<skiff::model::Tracked<After<Tag>>> {
  using type = std::tuple<due<After<Tag>>>;
};
template <class Root> struct dues_in {
  using type = std::tuple<>;
};
template <class... P> struct dues_in<skiff::model::Assembled<P...>> {
  using type = decltype(std::tuple_cat(std::declval<typename due_of<P>::type>()...));
};
} // namespace detail

// What runs a model's timers: each Every and After of its root, gone off as
// its time comes. Stepped whenever the program settles; next() is when to be
// woken for the soonest.
template <class Model> class Ticker {
public:
  void step(Model &model) {
    const auto now = Clock::now();
    std::apply([&](auto &...one) { (this->follow(model, one, now), ...); }, fDues);
  }
  [[nodiscard]] std::optional<Clock::time_point> next() const {
    std::optional<Clock::time_point> soonest;
    const auto take = [&](const auto &one) {
      if (one.at && (!soonest || *one.at < *soonest)) soonest = one.at;
    };
    std::apply([&](const auto &...one) { (take(one), ...); }, fDues);
    return soonest;
  }

private:
  template <class Tag> void follow(Model &model, detail::due<Every<Tag>> &d, Clock::time_point now) {
    const auto seen = model.template look<Every<Tag>>();
    if (!seen || !seen->on) {
      d = {};
      return;
    }
    if (!d.on) {
      d.on = true;
      d.at = now + seen->every;
      return;
    }
    if (d.at && now >= *d.at) {
      // Gone off as many times as it should have, in one edit: a page left
      // in the background is not made to catch up one by one.
      const auto every = std::max(seen->every, std::chrono::milliseconds(1));
      const auto late = std::chrono::duration_cast<std::chrono::milliseconds>(now - *d.at);
      const std::uint64_t times = 1 + static_cast<std::uint64_t>(late / every);
      d.at = *d.at + every * static_cast<long long>(times);
      (void)model.apply(skiff::model::over<Every<Tag>>([times](Every<Tag> e) {
        e.count += times;
        return e;
      }));
    }
  }
  template <class Tag> void follow(Model &model, detail::due<After<Tag>> &d, Clock::time_point now) {
    const auto seen = model.template look<After<Tag>>();
    if (!seen || seen->round == 0) return;
    if (seen->round != d.round) {
      d.round = seen->round;
      d.at = now + seen->in;
      return;
    }
    if (d.at && now >= *d.at) {
      d.at.reset();
      (void)model.apply(skiff::model::over<After<Tag>>([](After<Tag> a) {
        a.done = true;
        return a;
      }));
    }
  }

  typename detail::dues_in<typename Model::RootType>::type fDues;
};

} // namespace skiff::app::timer
