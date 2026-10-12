# Roadmap do motor

Ordem priorizada. Cada item só é marcado concluído com implementação,
integração e testes — não por existência de interface.

## 1. Decoding real (próximo grande passo)
- Decisão ADR-006 executada: linking de libavcodec/libavformat (escolha de
  licença/compilação LGPL vs GPL, build via FetchContent ou sistema) OU
  processo decodificador separado com pipe.
- `IDecoder` real para vídeo (h264/hevc + 4:2:0 8-bit primeiro), seek
  keyframe/exato, decode de áudio (PCM float).
- Testes com fixtures sintéticas (lavfi) redistribuíveis.

## 2. Playback de preview
- Scheduler de frames: clock mestre, fila de solicitações, cancelamento de
  frames obsoletos (token por posição), buffer de áudio.
- Composição trivial (1 trilha de vídeo, corte duro) para validar ponta a
  ponta. **Só então** alegar "playback" — e ainda não "profissional".

## 3. Render/export
- Composição de camadas (bottom-up), escala/posicionamento, blend básico.
- Encoder via ffmpeg CLI (primeiro) com configurações de resolução/fps/
  codec/qualidade; export renderizado por jobs com progresso.

## 4. Áudio
- Resampling, mapa de canais, ganho por clipe, fades lineares,
  mixagem completa da timeline; device de saída (WASAPI/ALSA/Pulse) atrás
  de contrato.

## 5. Cor
- Conversões YUV↔RGB com vetores de teste por padrão (BT.709 primeiro),
  limited/full range, depois 10-bit e HDR (PQ/HLG) com metadados.

## 6. Editor/UI (fase separada)
- Qt Quick/QML consumindo este core: nada de lógica de edição na UI.
- Contratos de seleção, snapping, drag com preview de sobreposição.

## 7. Plugins
- Host fora do processo (IPC local), descoberta em diretório de plugins,
  SDK documentado, sandbox e timeout. OpenFX/VST3: análise de licença e
  viabilidade antes de qualquer promessa (ADR-007).

## 8. Escala e qualidade
- Undo por diffs quando snapshots pesar; pool de jobs por tipo;
  cache de frames de decodificação; telemetria de pipeline; crash reports;
  fuzzing do parser de projeto; CI (Windows+Linux) e presets sanitizers.
