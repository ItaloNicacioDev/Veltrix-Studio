# Benchmarks

Micro-benchmarks de escala do core, sem dependências externas (harness
próprio com `std::chrono`). Ativáveis com `-DVELTRIX_BUILD_BENCHMARKS=ON`.

## Objetivo atual
1. Escala do undo por snapshots (ADR-003) e do parser de projetos.
2. **Decode (Fase 2)**: `veltrix-bench-decode` mede open, first frame,
   seek, throughput sequencial e memória de buffers — sobre decodificação
   REAL.

## Execução

```
cmake --preset win-msvc-release -DVELTRIX_BUILD_BENCHMARKS=ON [-DVELTRIX_FFMPEG_DIR=...]
cmake --build --preset win-msvc-release
build/win-msvc-release/benchmarks/veltrix-benchmarks.exe      # core
build/win-msvc-release/benchmarks/veltrix-bench-decode.exe    # decode (requer SDK)
```

## Decode — números medidos (Fase 2)

Ambiente: Windows x64, MSVC 19.51 Release, AMD (dev machine), FFmpeg
n9.0.2 LGPL shared, fixture **1080p H.264 veryfast + AAC stereo, 10 s @
30 fps** gerada por lavfi (conteúdo sintético), `std::chrono::steady_clock`,
uma medição por métrica (evidência, não meta).

```
open_ms=1231.6  first_frame_ms=120.6
seek_to_5s_plus_frame_ms=612.5  (pts=76800 = exato 5.0 s @ tb 1/15360)
seek_to_0_plus_frame_ms=13.1
sequential: frames=300  total_ms=2048.9  fps=146.4  rgba_mb=2488.3
audio: blocks=431  samples=441000  realtime_x=977.0  (44100 Hz, 2ch)
```

Leitura honesta:
- **Open é caro (1,2 s)** — inclui probe + codec init + primeira leitura
  de cabeçalho. O `FrameRequestor` atual abre por request; o decoder cache
  por MediaId (roadmap) é a otimização óbvia para preview interativo.
- **Throughput 146 fps @ 1080p30** = ~4,9x realtime para vídeo (30 fps),
  com conversão RGBA8 inclusa. Suficiente para preview de 1 trilha;
  composição multi-trilha usará esse orçamento.
- **Seek+frame 613 ms** = custo de aterrissar em keyframe + decodificar
  até 5,0 s. Preview responsivo exigirá o cache + keyframe-mode para
  scrub grosseiro (ambos no roadmap, atrás da mesma API).
- Áudio decodifica a ~977x realtime — irrelevante como gargalo.

## Core — números (fundação)

```
undo-snapshots  clips=1000   insert_total_ms=13.5     undo_avg_ms=0.008
undo-snapshots  clips=5000   insert_total_ms=119.5    undo_avg_ms=0.099
undo-snapshots  clips=20000  insert_total_ms=2730.1   undo_avg_ms=0.163
roundtrip       clips=1000   serialize_ms=1.8   deserialize_ms=2.7    bytes=176 KB
roundtrip       clips=5000   serialize_ms=11.0  deserialize_ms=36.4   bytes=885 KB
roundtrip       clips=20000  serialize_ms=80.7  deserialize_ms=354.6  bytes=3.5 MB
```

O undo por snapshot é barato mesmo a 20k clipes (0,16 ms/op); o insert é
quadrático (validação linear + insert ordenado) — otimização documentada
no roadmap antes de uso massivo.

