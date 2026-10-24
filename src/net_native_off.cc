// skiff.app.net.native without the framework's net feature: a transport that
// sends nothing, for a server (skiff.app.serve) built without it.
module;

#include <boost/asio.hpp>

export module skiff.app.net.native;

export namespace skiff::app::net {
class native {
public:
  explicit native(boost::asio::any_io_executor) {}
  void wakes(boost::asio::steady_timer *) {}
};
} // namespace skiff::app::net
