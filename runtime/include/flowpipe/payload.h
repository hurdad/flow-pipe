#pragma once

#include <cstddef>
#include <cstdint>

namespace flowpipe {

/**
 * Per-record metadata carried with each payload.
 * Small, fixed-size, and cheap to copy.
 */
struct PayloadMeta {
  static constexpr int trace_id_size = 16;
  static constexpr int span_id_size = 8;

  // Monotonic enqueue timestamp (nanoseconds)
  uint64_t enqueue_ts_ns = 0;

  // W3C trace identifiers (opaque bytes)
  uint8_t trace_id[trace_id_size]{};
  uint8_t span_id[span_id_size]{};

  // Bit flags (sampled, error, future use)
  uint32_t flags = 0;

  constexpr bool has_trace() const noexcept {
    for (int i = 0; i < trace_id_size; ++i) {
      if (trace_id[i] != 0)
        return true;
    }
    return false;
  }
};

/**
 * Runtime payload passed through queues.
 * Non-owning view of bytes plus per-record metadata.
 */
struct Payload {
  const uint8_t* data = nullptr;
  size_t size = 0;

  PayloadMeta meta;

  constexpr bool empty() const noexcept {
    return data == nullptr || size == 0;
  }
};

}  // namespace flowpipe
