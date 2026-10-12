# Estado real dos módulos

Cada módulo lista: o que está **implementado e testado**, o que é
**contrato** (interface sem implementação real) e o que está **planejado**.
Nada além disso. Sem inflação de estado.

## core — ✅ implementado
- `Result<T>`/`ResultVoid` + `Error` com `ErrorCode` estável.
- Ids tipados (`MediaId`…`ProjectId`) com geração sem colisão prática
  (semente aleatória XOR contador monotônico).
- `Log` com sinks pluggables e `LogHistory` limitado — sem singleton.
- `CancellationToken` (latch, callbacks pós-cancel executam uma vez) e
  `CancelOnDestroy`.
- `AppConfig` com parse JSON, defaults e warnings.
- `MathUtils`: add/sub/mul com overflow checado, floor_div/mod.
- `Version.h` gerado pelo CMake (versão, commit, compilador, build type).

## timeline — ✅ implementado
- `Rational` exato (normalizado, gcd, sinal no numerador).
- `Timebase` (from_fps, presets, conversões exatas/round/double, timecode
  inteiro; NTSC drop-frame reporta Unsupported).
- `EditTicks` int64; `Sequence` (invariantes: clipes ordenados, sem
  sobreposição, duração > 0, mídia não-nula); `TimelineEditor` com
  insert/move/split/trim validados e undo/redo por snapshot.

## project — ✅ implementado
- `VideoSettings`/`AudioSettings` validados (fps racional exato).
- `Project` (settings + registry + sequence, sempre com sequência válida).
- `ProjectStore`: serialize/strict-parse/save atômico/load+validate,
  schema_version 1 com recusa de versão futura.

## media — ✅ implementado
- `MediaItem`/`StreamInfo` (metadados persistentes vs. estado transitório).
- `MediaRegistry`: ids/caminhos únicos, detecção de ausentes.
- `MediaProber`: localização runtime do ffprobe + execução com timeout
  (CreateProcess/pipe no Windows, fork/poll no POSIX) + parse do JSON.
  FALHAS EXPLÍCITAS: binário ausente, timeout, exit ≠ 0, JSON inválido,
  sem streams decodificáveis.

## jobs — ✅ implementado (fundação)
- `Job` (estados com latch: Queued→Running→Succeeded/Failed/Cancelled),
  progresso clampado, `FunctionJob`.
- `JobManager`: 1 worker FIFO, submit valida, cancel de fila e de execução,
  shutdown cancela pendentes e faz join, wait_all, snapshot de estados.
- **Não implementado**: captura do `Error` dentro do job failed; pool
  multi-worker; prioridades.

## cache — ✅ implementado
- `LruCache<T>`: orçamento em bytes, LRU/FIFO, invalidação por geração
  (edição de timeline), entrega por `shared_ptr` (entrada evictada segue
  viva para quem a segura), estatísticas exatas (hits/misses/evictions).

## playback — 🟢 decode REAL implementado (Fase 2)
- ✅ `IVideoDecoder`/`IAudioDecoder` (contratos revisados) + `AvVideoDecoder`
  e `AvAudioDecoder` sobre libav (ADR-008): open/info, seek Exact (com
  forward-decode até o alvo) e Keyframe, read_frame/read_block com
  polling de cancelamento, EOS explícito, erros mapeados
  (NotFound/Unsupported/ParseError/OutOfRange/Cancelled).
- ✅ Vídeo: saída RGBA8 packed (stride = w*4, buffer próprio
  shared_ptr), pts preservado, keyframe flag, mudança de tamanho
  mid-stream suportada. Sem AV* nos headers públicos.
- ✅ Áudio: planar float32 (mesma convenção de veltrix::audio), blocos de
  tamanho variável, sample_rate/canais/pts, swresample para conversão.
- ✅ `FrameRequestor`: media-id + ticks → frame; rejeição de request
  obsoleta (Conflict "superseded"), cancelamento (token compartilhado),
  resolver sobre MediaRegistry (faltante → MediaUnavailable).
- ✅ `IDecoderFactory` + `AvDecoderFactory` (engine_name para diagnostics);
  build sem SDK degrada com `Unsupported` explícito (sem crash).
- ✅ Testes: layout por frame, pts monotônico, seek início/meio/fim,
  arquivo inválido/vazio/ausente, só-áudio/só-vídeo, contagem de amostras,
  cancelamento, destruição sem close, 25 ciclos open/close.
- ⛔ Ausente (roadmap): composição/pixel-blend, clock mestre, scheduling,
  sincronização A/V, device de saída. **Playback contínuo NÃO existe** —
  existe "frame em posição" (preview pontual), comprovado por testes.

## render — 🟡 parcial
- ✅ `TimelineEvaluator` (clipes de vídeo/áudio ativos num instante,
  ordenados para composição) testado.
- 📋 Contrato `IRenderNode`/`RenderImage` definido.
- ⛔ Ausente: composição de pixels, blend, transformações, efeitos,
  encoder, export. Nenhuma API de GPU existe.

## audio — 🟡 parcial
- ✅ Vocabulário sem ambiguidade (`AudioSample`/`AudioFrame`/
  `SampleRate`/`SampleCount`), buffers planares, ganho com clip, mixagem
  validada (mesmos canais/taxa; sem resampling — erro explícito).
- ⛔ Ausente: fades, keyframes, resampling, efeitos, device/saída.

## color — 📋 contrato
- Enums nomeados (primárias, transfer functions, formatos) + validação de
  `ColorMetadata`. **Nenhuma conversão implementada de propósito** — só
  entrará com vetores de teste por padrão. Nada de pseudo-ciência.

## plugins — 🟡 parcial
- ✅ Manifesto JSON estrito + compatibilidade de API (recusa api_version
  mais novo) testados.
- ⛔ Ausente: descoberta, carregamento, SDK, isolamento por processo
  (risco de segurança documentado — nunca in-process com não-confiáveis).

## effects — ⛔ planejado
- Diretório/arquitetura reservados. Nada implementado.

## io — ✅ implementado
- Leitura de arquivo, escrita atômica (temp+rename), tamanho, exists,
  normalize_path. Falhas como NotFound/IoError.

## diagnostics — ✅ implementado
- Relatório de build (versão/compilador/deps/plugin api) sem dados
  sensíveis. Crash reports e métricas de pipeline no roadmap.

## CLI (veltrix-cli) — ✅ implementado
- `info`, `new` (com mídia opcional: registra, probe, clipa), `check`,
  `probe`. Códigos de saída 0/1/2. Smoke test executado.
