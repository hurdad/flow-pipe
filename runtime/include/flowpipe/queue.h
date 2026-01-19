#pragma once

#include <optional>

#include "stop_token.h"

namespace flowpipe {

template <typename T>
class IQueue {
 public:
  virtual ~IQueue() = default;

  virtual bool push(T item, const StopToken& stop) = 0;
  virtual std::optional<T> pop(const StopToken& stop) = 0;
  virtual void close() = 0;
};

}  // namespace flowpipe
