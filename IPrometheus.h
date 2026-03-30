#ifndef IPROMETHEUS_H
#define IPROMETHEUS_H
#include <string>

class IPrometheus {
public:
  virtual void *AddCounter(const std::string &name,
                           const std::string &help) = 0;
  virtual void IncrementCounter(void *counter) = 0;
};

#endif // IPROMETHEUS_H