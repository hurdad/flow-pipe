{{- define "flowpipe.name" -}}flow-pipe{{- end }}

{{- define "flowpipe.namespace" -}}
{{- if .Values.namespace.create -}}
{{ .Values.namespace.name }}
{{- else -}}
{{ .Release.Namespace }}
{{- end }}
{{- end }}

{{- define "flow-pipe.prometheus.endpoint" -}}
{{- $observability := default (dict) .Values.observability -}}
{{- $prometheus := default (dict) $observability.prometheus -}}
{{- if $prometheus.endpoint -}}
{{ $prometheus.endpoint }}
{{- else if $prometheus.enabled -}}
http://{{ .Release.Name }}-prometheus-server
{{- end -}}
{{- end }}

{{- define "flow-pipe.loki.endpoint" -}}
{{- $observability := default (dict) .Values.observability -}}
{{- $loki := default (dict) $observability.loki -}}
{{- if $loki.endpoint -}}
{{ $loki.endpoint }}
{{- else if $loki.enabled -}}
http://{{ .Release.Name }}-loki:3100
{{- end -}}
{{- end }}

{{- define "flow-pipe.tempo.endpoint" -}}
{{- $observability := default (dict) .Values.observability -}}
{{- $tempo := default (dict) $observability.tempo -}}
{{- if $tempo.endpoint -}}
{{ $tempo.endpoint }}
{{- else if $tempo.enabled -}}
http://{{ .Release.Name }}-tempo:3200
{{- end -}}
{{- end }}

{{- define "flow-pipe.grafana.endpoint" -}}
{{- $observability := default (dict) .Values.observability -}}
{{- $grafanaObs := default (dict) $observability.grafana -}}
{{- if $grafanaObs.endpoint -}}
{{- $grafanaObs.endpoint -}}
{{- else if and $grafanaObs.enabled
              .Values.grafana
              .Values.grafana.ingress
              .Values.grafana.ingress.enabled -}}
{{- $scheme := ternary "https" "http" (gt (len .Values.grafana.ingress.tls) 0) -}}
{{- if gt (len .Values.grafana.ingress.hosts) 0 -}}
{{- printf "%s://%s" $scheme (index .Values.grafana.ingress.hosts 0) -}}
{{- else -}}
{{- printf "%s://%s-grafana" $scheme .Release.Name -}}
{{- end -}}
{{- end -}}
{{- end -}}


{{- define "flow-pipe.alloy.endpoint" -}}
{{- $observability := default (dict) .Values.observability -}}
{{- $alloy := default (dict) $observability.alloy -}}
{{- if $alloy.endpoint -}}
{{ $alloy.endpoint }}
{{- else if $alloy.enabled -}}
http://{{ .Release.Name }}-alloy:4317
{{- end -}}
{{- end }}

{{- define "flow-pipe.alloy.river" -}}
{{- $observability := default (dict) .Values.observability -}}
{{- $alloy := default (dict) $observability.alloy -}}
{{- $prom := include "flow-pipe.prometheus.endpoint" . -}}
{{- $loki := include "flow-pipe.loki.endpoint" . -}}
{{- $tempo := include "flow-pipe.tempo.endpoint" . -}}
{{- $exporters := dict "prom" "" "loki" "" "tempo" "" -}}
{{- if and $alloy.exporters $alloy.exporters.prometheus $alloy.exporters.prometheus.endpoint -}}
{{- $_ := set $exporters "prom" $alloy.exporters.prometheus.endpoint -}}
{{- else if $prom -}}
{{- $_ := set $exporters "prom" (printf "%s/api/v1/write" $prom) -}}
{{- end -}}
{{- if and $alloy.exporters $alloy.exporters.loki $alloy.exporters.loki.endpoint -}}
{{- $_ := set $exporters "loki" $alloy.exporters.loki.endpoint -}}
{{- else if $loki -}}
{{- $_ := set $exporters "loki" (printf "%s/loki/api/v1/push" $loki) -}}
{{- end -}}
{{- if and $alloy.exporters $alloy.exporters.tempo $alloy.exporters.tempo.endpoint -}}
{{- $_ := set $exporters "tempo" $alloy.exporters.tempo.endpoint -}}
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
        endpoint = "{{ default "0.0.0.0:4317" (dig "receivers" "otlp" "grpcEndpoint" "" $alloy) }}"
      },
      http = {
        endpoint = "{{ default "0.0.0.0:4318" (dig "receivers" "otlp" "httpEndpoint" "" $alloy) }}"
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