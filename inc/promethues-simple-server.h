#ifndef PROMETHEUS_SIMPLE_SERVER_H
#define PROMETHEUS_SIMPLE_SERVER_H

#include "prometheus/counter.h"
#include "prometheus/exposer.h"
#include "prometheus/registry.h"

#include "IPrometheus.h"

class PrometheusSimpleServer : public IPrometheus {
public:
  PrometheusSimpleServer() {
    registry = std::make_shared<prometheus::Registry>();
  }
  void Start() { exposer.RegisterCollectable(registry, "/metrics"); }
  void *AddCounter(const std::string &name, const std::string &help) override {
    auto &counter_family =
        prometheus::BuildCounter().Name(name).Help(help).Register(*registry);
    return &counter_family.Add({});
  }
  void IncrementCounter(void *counter) override {
    static_cast<prometheus::Counter *>(counter)->Increment();
  }

private:
  prometheus::Exposer exposer{"0.0.0.0:8080"};
  std::shared_ptr<prometheus::Registry> registry;
};

#endif // PROMETHEUS_SIMPLE_SERVER_H
