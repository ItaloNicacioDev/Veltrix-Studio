# Roadmap do motor

Ordem priorizada. Cada item só é marcado concluído com implementação,
integração e testes — não por existência de interface.

## 1. Decodificação real — ✅ CONCLUÍDO (Fase 2)
- [x] ADR-008: vinculação dinâmica LGPL de libav* (isolada em
  `src/media/ffmpeg/`; zero tipos AV* na API pública).
- [x] `IVideoDecoder` real: H.264 (qualquer pixel format via sws→RGBA8
  packed), seek Exact com forward-decode, Keyframe, EOS, cancelamento,
  destruição segura. Outros codecs entram conforme os decoders LGPL do
  build do FFmpeg usado.
- [x] `IAudioDecoder` real: planar float32 via swresample, seek, EOS.
- [x] `FrameRequestor` (media-id + ticks → frame, stale-reject, cancel).
- [x] Benchmarks: open 1232 ms / first frame 121 ms / seek+frame 613 ms /
  146 fps sequenciais @1080p30 / áudio 977x realtime (MSVC Release, dev).
- [ ] Próximo dentro do tema: decoder cache por MediaId (evitar open por
  request); decodificação por JOB para thumbnails em lote.

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
