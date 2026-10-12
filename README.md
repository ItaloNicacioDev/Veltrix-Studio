# Veltrix Studio

**Editor de vídeo profissional open source para Windows e Linux.**
Este repositório contém a **fundação do core C++** do motor de edição — sem interface gráfica. A UI (Qt Quick/QML) será construída posteriormente sobre estes módulos.

> ⚠️ Estado: **fundação 0.1.0**. Isto NÃO é um editor pronto. É a base de engenharia: modelo temporal exato, timeline validada, persistência de projetos, análise de mídia e infraestrutura assíncrona — tudo compilado e testado, sem uma linha de interface.

## O que existe e funciona (com evidência de execução)

| Módulo | Estado | Evidência |
|---|---|---|
| Modelo temporal (ticks inteiros + timebase exata, NTSC exato) | ✅ Implementado + testado | `tests/unit/TestTime.cpp` (15 casos) |
| Timeline (tracks/clips/insert/move/split/trim validados) | ✅ Implementado + testado | `tests/unit/TestTimeline.cpp` (15 casos) |
| Undo/redo (snapshots de duas pilhas) | ✅ Implementado + testado | idem |
| Projeto + persistência JSON versionada (schema 1) | ✅ Implementado + testado | `tests/unit/TestProjectStore.cpp` (12 casos) |
| Import/registro de mídia (arquivos ausentes, duplicatas) | ✅ Implementado + testado | `tests/unit/TestMediaRegistry.cpp` |
| Análise técnica via ffprobe (descoberta runtime) | ✅ Implementado + smoke real | CLI `probe` sobre MP4 sintético |
| Tarefas assíncronas (1 worker, cancelamento, progresso) | ✅ Implementado + testado | `tests/unit/TestJobs.cpp` |
| Cache LRU/FIFO com orçamento de bytes e geração | ✅ Implementado + testado | `tests/unit/TestCache.cpp` |
| Áudio (tipagem sem ambiguidade, ganho, mixagem) | ✅ Base implementada + testada | `tests/unit/TestConfigAndDiagnostics.cpp` |
| Playback (máquina de estados) | ✅ Estado implementado; scheduling planejado | `PlaybackStateMachine` |
| Render (avaliador de timeline; grafo é contrato) | ✅ Resolução de clipes ativos testada | `TimelineEvaluator` |
| Cor | 📋 Contrato validado; conversões planejadas | `ColorContract.h` |
| Plugins | 📋 Manifesto+compatibilidade testados; carregamento planejado | `PluginManifest` |
| veltrix-cli | ✅ `info`/`new`/`check`/`probe` funcionando | smoke test executado |

**Testes: 394 assertions, 75 casos — todos aprovados** (MSVC 19.51 / Ninja / CMake 3.24+, Release).

## Build rápido

Windows (VS 2022 + workload C++, inclui CMake+Ninja):
```
scripts\build-windows.bat            # release + testes
scripts\build-windows.bat debug
```

Linux (GCC ≥ 11 / Clang ≥ 14, CMake ≥ 3.24, Ninja):
```
./scripts/build-linux.sh
```

Manual (presets):
```
cmake --preset win-msvc-release
cmake --build --preset win-msvc-release
build/win-msvc-release/tests/veltrix-tests.exe     # testes
build/win-msvc-release/src/veltrix-cli.exe info    # diagnóstico
```

## veltrix-cli

Ferramenta de diagnóstico (não é a UI do produto):
```
veltrix-cli info                                  # build/deps/plugin-api
veltrix-cli new projeto.veltrix "Nome"            # cria projeto
veltrix-cli new projeto.veltrix "Nome" clip.mp4   # + mídia registrada e clippada
veltrix-cli check projeto.veltrix                 # carrega + valida
veltrix-cli probe video.mp4                       # análise técnica via ffprobe
```
O `probe` localiza o ffprobe em runtime (PATH ou variável `VELTRIX_FFPROBE`); o core **não** linka libav*.

## Estrutura

```
CMakeLists.txt / CMakePresets.json / cmake/
include/veltrix/{core,project,timeline,media,playback,render,audio,color,cache,effects,plugins,io,jobs,diagnostics}
src/ (espelha include/) + src/cli
tests/{unit,integration}
third_party/  (nlohmann/json 3.11.3 MIT, Catch2 2.13.10 BSL-1.0 — header-only)
docs/  (ARCHITECTURE, BUILDING, MODULES, ROADMAP, DEPENDENCIES_AND_LICENSES, ADR/)
```

## Documentação

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — arquitetura, modelo temporal, políticas de erro
- [docs/BUILDING.md](docs/BUILDING.md) — requisitos e procedimentos de build
- [docs/MODULES.md](docs/MODULES.md) — estado real por módulo
- [docs/ROADMAP.md](docs/ROADMAP.md) — próximos passos do motor
- [docs/DEPENDENCIES_AND_LICENSES.md](docs/DEPENDENCIES_AND_LICENSES.md) — dependências e licenças
- [docs/ADR/](docs/ADR/) — decisões de arquitetura (tempo inteiro, undo, FFmpeg, jobs)

## Licença

GPL-3.0-or-later (ver [LICENSE](LICENSE)). Dependências vendored com licenças MIT/BSL-1.0 compatíveis.
