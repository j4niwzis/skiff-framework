// skiff.app.net without the framework's net feature: nothing asked of the
// network, nothing of knot in the build for it. A runner that runs nothing.
export module skiff.app.net;

export namespace skiff::app::net {
struct browser {};
template <class Model, class Transport> class Runner {
public:
  explicit Runner(Transport transport) : fTransport(transport) {}
  [[nodiscard]] Transport &transport() noexcept { return fTransport; }
  [[nodiscard]] bool busy() const { return false; }
  void step(Model &) {}

private:
  Transport fTransport;
};
} // namespace skiff::app::net
