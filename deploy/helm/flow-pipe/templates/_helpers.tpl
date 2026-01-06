{{- define "flowpipe.name" -}}flow-pipe{{- end }}

{{- define "flowpipe.namespace" -}}
{{- if .Values.namespace.create -}}
{{ .Values.namespace.name }}
{{- else -}}
{{ .Release.Namespace }}
{{- end }}
{{- end }}

{{/* ---------------------------------------------------------
   Observability Endpoints
   --------------------------------------------------------- */}}

{{- define "flow-pipe.prometheus.endpoint" -}}
{{- $observability := default dict .Values.observability -}}
{{- $prometheus := default dict $observability.prometheus -}}
{{- if $prometheus.endpoint -}}
{{ $prometheus.endpoint }}
{{- else if $prometheus.enabled -}}
http://{{ .Release.Name }}-prometheus-server
{{- end -}}
{{- end }}

{{- define "flow-pipe.loki.endpoint" -}}
{{- $observability := default dict .Values.observability -}}
{{- $loki := default dict $observability.loki -}}
{{- if $loki.endpoint -}}
{{ $loki.endpoint }}
{{- else if $loki.enabled -}}
http://{{ .Release.Name }}-loki:3100
{{- end -}}
{{- end }}

{{- define "flow-pipe.tempo.endpoint" -}}
{{- $observability := default dict .Values.observability -}}
{{- $tempo := default dict $observability.tempo -}}
{{- if $tempo.endpoint -}}
{{ $tempo.endpoint }}
{{- else if $tempo.enabled -}}
http://{{ .Release.Name }}-tempo:3200
{{- end -}}
{{- end }}

{{- define "flow-pipe.grafana.endpoint" -}}
{{- $observability := default dict .Values.observability -}}
{{- $grafana := default dict $observability.grafana -}}
{{- $grafanaValues := default dict .Values.grafana -}}
{{- $grafanaIngress := default dict $grafanaValues.ingress -}}
{{- if $grafana.endpoint -}}
{{- $grafana.endpoint -}}
{{- else if and $grafana.enabled
              $grafanaIngress.enabled -}}
{{- $scheme := ternary "https" "http" (gt (len $grafanaIngress.tls) 0) -}}
{{- if gt (len $grafanaIngress.hosts) 0 -}}
{{- printf "%s://%s" $scheme (index $grafanaIngress.hosts 0) -}}
{{- else -}}
{{- printf "%s://%s-grafana" $scheme .Release.Name -}}
{{- end -}}
{{- end -}}
{{- end }}

{{- define "flow-pipe.alloy.endpoint" -}}
{{- $observability := default dict .Values.observability -}}
{{- $alloy := default dict $observability.alloy -}}
{{- if $alloy.endpoint -}}
{{ $alloy.endpoint }}
{{- else if $alloy.enabled -}}
http://{{ .Release.Name }}-alloy:4317
{{- end -}}
{{- end }}

{{/* ---------------------------------------------------------
   Alloy River Config
   --------------------------------------------------------- */}}

{{- define "flow-pipe.alloy.river" -}}
{{- $observability := default dict .Values.observability -}}
{{- $alloy := default dict $observability.alloy -}}
{{- if not $alloy.enabled -}}
{{- return -}}
{{- end -}}

{{- $prom := include "flow-pipe.prometheus.endpoint" . -}}
{{- $loki := include "flow-pipe.loki.endpoint" . -}}
{{- $tempo := include "flow-pipe.tempo.endpoint" . -}}

{{- $exporters := dict "prom" "" "loki" "" "tempo" "" -}}

{{- $alloyExporters := default dict $alloy.exporters -}}
{{- $promExporter := default dict $alloyExporters.prometheus -}}
{{- $lokiExporter := default dict $alloyExporters.loki -}}
{{- $tempoExporter := default dict $alloyExporters.tempo -}}

{{- $alloyReceivers := default dict $alloy.receivers -}}
{{- $otlp := default dict $alloyReceivers.otlp -}}

{{- if $promExporter.endpoint -}}
{{- $_ := set $exporters "prom" $promExporter.endpoint -}}
{{- else if $prom -}}
{{- $_ := set $exporters "prom" (printf "%s/api/v1/write" $prom) -}}
{{- end -}}

{{- if $lokiExporter.endpoint -}}
{{- $_ := set $exporters "loki" $lokiExporter.endpoint -}}
{{- else if $loki -}}
{{- $_ := set $exporters "loki" (printf "%s/loki/api/v1/push" $loki) -}}
{{- end -}}

{{- if $tempoExporter.endpoint -}}
{{- $_ := set $exporters "tempo" $tempoExporter.endpoint -}}
{{- else if $tempo -}}
{{- $_ := set $exporters "tempo" $tempo -}}
{{- end -}}

logging {
  level = "info"
}

otel {
  receiver "otlp" {
    protocols = {
      grpc = {
        endpoint = "{{ default "0.0.0.0:4317" $otlp.grpcEndpoint }}"
      },
      http = {
        endpoint = "{{ default "0.0.0.0:4318" $otlp.httpEndpoint }}"
      }
    }
  }

{{- if (get $exporters "prom") }}
  exporter "prometheusremotewrite" {
    endpoint = "{{ get $exporters "prom" }}"
  }
{{- end }}

{{- if (get $exporters "loki") }}
  exporter "loki" {
    endpoint = "{{ get $exporters "loki" }}"
  }
{{- end }}

{{- if (get $exporters "tempo") }}
  exporter "otlp" {
    client {
      endpoint = "{{ get $exporters "tempo" }}"
    }
  }
{{- end }}

  processor "batch" {}

{{- if (get $exporters "prom") }}
  pipeline "metrics" {
    receivers  = [otel.receiver.otlp]
    processors = [otel.processor.batch]
    exporters  = [otel.exporter.prometheusremotewrite]
  }
{{- end }}

{{- if (get $exporters "loki") }}
  pipeline "logs" {
    receivers  = [otel.receiver.otlp]
    processors = [otel.processor.batch]
    exporters  = [otel.exporter.loki]
  }
{{- end }}

{{- if (get $exporters "tempo") }}
  pipeline "traces" {
    receivers  = [otel.receiver.otlp]
    processors = [otel.processor.batch]
    exporters  = [otel.exporter.otlp]
  }
{{- end }}
}
{{- end }}
