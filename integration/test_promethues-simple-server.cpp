#include "promethues-simple-server.h"

#include <chrono>
#include <thread>

int main() {
  PrometheusSimpleServer server;
  auto counter = server.AddCounter("number_seconds_total",
                                   "Total number of seconds elapsed");
  server.Start();
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    server.IncrementCounter(counter);
  }
  return 0;
}
