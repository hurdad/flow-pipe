#pragma once

#include <cstddef>
#include <cstdint>

namespace flowpipe {

/**
 * Per-record metadata carried with each payload.
 * Small, fixed-size, and cheap to copy.
 */
struct PayloadMeta {
  // Monotonic enqueue timestamp (nanoseconds)
  uint64_t enqueue_ts_ns = 0;

  // W3C Trace Context identifiers
  uint64_t trace_id_hi = 0;
  uint64_t trace_id_lo = 0;
  uint64_t span_id = 0;

  // Bit flags (sampled, error, future use)
  uint32_t flags = 0;

  constexpr bool has_trace() const noexcept {
    return trace_id_hi != 0 || trace_id_lo != 0;
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
