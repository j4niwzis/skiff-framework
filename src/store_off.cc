// skiff.app.store without the framework's store feature: nothing kept.
export module skiff.app.store;

export namespace skiff::app::store {
template <class Model> class Keeper {
public:
  void restore(Model &) {}
  void save(const Model &) {}
};
} // namespace skiff::app::store
