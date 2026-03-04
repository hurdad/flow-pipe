#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace flowpipe {

// Transparent hash enabling string_view lookups into unordered_map<string,...>
// without constructing a temporary std::string.
struct StringViewHash {
  using is_transparent = void;
  std::size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

/**
 * Per-record metadata carried with each payload.
 * Small and cheap to copy.
 */
struct PayloadMeta {
  static constexpr int trace_id_size = 16;
  static constexpr int span_id_size = 8;

  using MetaValue = std::variant<int64_t, double, bool, std::string>;
  using MetadataMap = std::unordered_map<std::string, MetaValue, StringViewHash, std::equal_to<>>;

  // Monotonic enqueue timestamp (nanoseconds)
  uint64_t enqueue_ts_ns = 0;

  // W3C trace identifiers (opaque bytes)
  uint8_t trace_id[trace_id_size]{};
  uint8_t span_id[span_id_size]{};

  // Bit flags (sampled, error, future use)
  uint32_t flags = 0;

  // Schema identifier for payload validation (optional).
  std::string schema_id;

  // Optional extensible metadata for stage-to-stage contracts.
  // Shared as immutable by default; writes detach with copy-on-write.
  std::shared_ptr<const MetadataMap> attrs;

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

  bool has_attrs() const noexcept {
    return attrs && !attrs->empty();
  }

  const MetaValue* get_attr(std::string_view key) const noexcept {
    if (!attrs) {
      return nullptr;
    }

    auto it = attrs->find(key);
    return it == attrs->end() ? nullptr : &it->second;
  }

  void set_attr(std::string key, MetaValue value) {
    auto copy = attrs ? std::make_shared<MetadataMap>(*attrs) : std::make_shared<MetadataMap>();
    (*copy)[std::move(key)] = std::move(value);
    attrs = std::move(copy);
  }

  bool erase_attr(std::string_view key) {
    if (!attrs) {
      return false;
    }

    if (attrs->find(key) == attrs->end()) {
      return false;
    }

    auto copy = std::make_shared<MetadataMap>(*attrs);
    copy->erase(std::string(key));
    attrs = copy->empty() ? nullptr : std::shared_ptr<const MetadataMap>(std::move(copy));
    return true;
  }

  void clear_attrs() noexcept {
    attrs.reset();
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

// Allocates a shared buffer for payload data.
// Throws std::bad_alloc on OOM so callers always receive a valid buffer.
inline std::shared_ptr<uint8_t[]> AllocatePayloadBuffer(size_t size) {
  return std::shared_ptr<uint8_t[]>(new uint8_t[size]);
}

}  // namespace flowpipe
