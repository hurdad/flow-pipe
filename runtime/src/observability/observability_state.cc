#include "flowpipe/observability/observability_state.h"

namespace flowpipe::observability {

OtelState& GetOtelState() {
  static OtelState state;
  return state;
}

}  // namespace flowpipe::observability
