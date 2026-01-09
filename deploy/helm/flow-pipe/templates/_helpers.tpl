{{- define "flowpipe.name" -}}flow-pipe{{- end }}

{{- define "flowpipe.namespace" -}}
{{- if .Values.namespace.create -}}
{{ .Values.namespace.name }}
{{- else -}}
{{ .Release.Namespace }}
{{- end }}
{{- end }}

{{- define "flow-pipe.observability" -}}
{{- $global := default (dict) (get .Values "global") -}}
{{- $globalObs := default (dict) (get $global "observability") -}}
{{- $localObs := default (dict) (get .Values "observability") -}}
{{- $mergedObs := mergeOverwrite (deepCopy $globalObs) $localObs -}}
{{- toYaml $mergedObs -}}
{{- end }}

{{- define "flow-pipe.alloy.endpoint" -}}
{{- $observability := default (dict) (include "flow-pipe.observability" . | fromYaml) -}}
{{- $alloy := default (dict) $observability.alloy -}}
{{- if $alloy.endpoint -}}
{{ $alloy.endpoint }}
{{- else if $alloy.enabled -}}
http://flow-pipe-alloy:4317
{{- end -}}
{{- end }}

{{- define "flow-pipe.alloy.otlp.endpoint" -}}
{{- $endpoint := include "flow-pipe.alloy.endpoint" . -}}
{{- if $endpoint -}}
{{- $endpoint = trimPrefix "http://" $endpoint -}}
{{- $endpoint = trimPrefix "https://" $endpoint -}}
{{- $endpoint -}}
{{- end -}}
{{- end }}

{{- define "flow-pipe.alloy.river" -}}
{{- $observability := default (dict) (include "flow-pipe.observability" . | fromYaml) -}}
{{- $alloy := default (dict) $observability.alloy -}}
{{- $metricsEndpoint := dig "exporters" "metrics" "endpoint" "" $alloy -}}
{{- $tracesEndpoint := dig "exporters" "traces" "endpoint" "" $alloy -}}
{{- $logsEndpoint := dig "exporters" "logs" "endpoint" "" $alloy -}}
logging {
  level = "debug"
}

otelcol.receiver.otlp "default" {
  grpc {
    endpoint = "{{ default "0.0.0.0:4317" (dig "receivers" "otlp" "grpcEndpoint" "" $alloy) }}"
  }

  http {
    endpoint = "{{ default "0.0.0.0:4318" (dig "receivers" "otlp" "httpEndpoint" "" $alloy) }}"
  }

  output {
    traces  = [otelcol.processor.batch.default.input]
    metrics = [otelcol.processor.batch.default.input]
    logs    = [otelcol.processor.batch.default.input]
  }
}

otelcol.processor.batch "default" {
  output {
    traces  = [otelcol.exporter.otlp.traces.input]
    metrics = [otelcol.exporter.otlp.metrics.input]
    logs    = [otelcol.exporter.otlp.logs.input]
  }
}

otelcol.exporter.otlp "traces" {
  client {
    endpoint = "{{ $tracesEndpoint }}"
    tls {
      insecure = true
    }
  }
}

otelcol.exporter.otlp "metrics" {
  client {
    endpoint = "{{ $metricsEndpoint }}"
    tls {
      insecure = true
    }
  }
}

otelcol.exporter.otlp "logs" {
  client {
    endpoint = "{{ $logsEndpoint }}"
    tls {
      insecure = true
    }
  }
}

{{- end }}
