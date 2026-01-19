#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>

namespace flowpipe {

/**
 * Per-record metadata carried with each payload.
 * Small and cheap to copy.
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

  // Schema identifier for payload validation (optional).
  std::string schema_id;

  bool has_schema_id() const noexcept {
    return !schema_id.empty();
  }

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
 * Owns its buffer via shared ownership to avoid manual lifetime management.
 */
struct Payload {
  std::shared_ptr<uint8_t[]> buffer;
  size_t size = 0;

  PayloadMeta meta;

  constexpr Payload() = default;

  constexpr Payload(std::shared_ptr<uint8_t[]> buf, size_t buffer_size, PayloadMeta m = {})
      : buffer(std::move(buf)), size(buffer_size), meta(m) {}

  constexpr const uint8_t* data() const noexcept {
    return buffer.get();
  }

  constexpr uint8_t* data() noexcept {
    return buffer.get();
  }

  constexpr bool empty() const noexcept {
    return buffer == nullptr || size == 0;
  }
};

// Allocates a shared buffer for payload data using nothrow new.
inline std::shared_ptr<uint8_t[]> AllocatePayloadBuffer(size_t size) {
  return std::shared_ptr<uint8_t[]>(new (std::nothrow) uint8_t[size]);
}

}  // namespace flowpipe
