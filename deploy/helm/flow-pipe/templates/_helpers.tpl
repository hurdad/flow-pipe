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
{{- if gt (len $globalObs) 0 -}}
{{- toYaml $globalObs -}}
{{- else -}}
{{- toYaml $localObs -}}
{{- end -}}
{{- end }}

{{- define "flow-pipe.prometheus.endpoint" -}}
{{- $observability := default (dict) (include "flow-pipe.observability" . | fromYaml) -}}
{{- $prometheus := default (dict) $observability.prometheus -}}
{{- if $prometheus.endpoint -}}
{{ $prometheus.endpoint }}
{{- else if $prometheus.enabled -}}
http://{{ .Release.Name }}-prometheus-server
{{- end -}}
{{- end }}

{{- define "flow-pipe.loki.endpoint" -}}
{{- $observability := default (dict) (include "flow-pipe.observability" . | fromYaml) -}}
{{- $loki := default (dict) $observability.loki -}}
{{- if $loki.endpoint -}}
{{ $loki.endpoint }}
{{- else if $loki.enabled -}}
http://{{ .Release.Name }}-loki:3100
{{- end -}}
{{- end }}

{{- define "flow-pipe.tempo.endpoint" -}}
{{- $observability := default (dict) (include "flow-pipe.observability" . | fromYaml) -}}
{{- $tempo := default (dict) $observability.tempo -}}
{{- if $tempo.endpoint -}}
{{ $tempo.endpoint }}
{{- else if $tempo.enabled -}}
http://{{ .Release.Name }}-tempo:3200
{{- end -}}
{{- end }}

{{- define "flow-pipe.grafana.endpoint" -}}
{{- $observability := default (dict) (include "flow-pipe.observability" . | fromYaml) -}}
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
{{- $observability := default (dict) (include "flow-pipe.observability" . | fromYaml) -}}
{{- $alloy := default (dict) $observability.alloy -}}
{{- if $alloy.endpoint -}}
{{ $alloy.endpoint }}
{{- else if $alloy.enabled -}}
http://{{ .Release.Name }}-alloy:4317
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
{{- $prom := include "flow-pipe.prometheus.endpoint" . -}}
{{- $loki := include "flow-pipe.loki.endpoint" . -}}
{{- $tempo := include "flow-pipe.tempo.endpoint" . -}}
logging {
  level = "info"
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
    traces  = [otelcol.exporter.otlp.tempo.input]
    metrics = [otelcol.exporter.prometheus.default.input]
    logs    = [otelcol.exporter.loki.default.input]
  }
}

otelcol.exporter.otlp "tempo" {
  client {
    endpoint = "tempo:4317"
    tls {
      insecure = true
    }
  }
}

otelcol.exporter.prometheus "default" {
  forward_to = [prometheus.remote_write.default.receiver]
}

prometheus.remote_write "default" {
  endpoint {
    url = "http://prometheus:9090/api/v1/write"
  }
}

otelcol.exporter.loki "default" {
  client {
    endpoint = "http://loki:3100/loki/api/v1/push"
  }
}


{{- end }}
