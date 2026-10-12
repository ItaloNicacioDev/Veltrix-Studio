# Compilando o Veltrix Core

## Requisitos

| Ferramenta | Versão mínima | Observação |
|---|---|---|
| CMake | 3.24 | presets |
| Compilador | MSVC 19.30+ / GCC 11+ / Clang 14+ | C++20 |
| Ninja | 1.10 | gerador padrão dos presets |
| Python | — | não requerido |

Sem dependências externas para compilar: as bibliotecas de terceiros
(nlohmann/json, Catch2) estão vendored em `third_party/` (header-only).

FFmpeg **não é requerido para compilar**. O `veltrix-cli probe` precisa do
binário `ffprobe` em runtime (PATH ou variável `VELTRIX_FFPROBE`).

## Windows

Pré-requisito: Visual Studio 2022 (ou Build Tools) com a workload
"Desenvolvimento para desktop com C++", incluindo "Ferramentas CMake C++
para Windows" (traz CMake e Ninja).

```
scripts\build-windows.bat            :: release + testes
scripts\build-windows.bat debug      :: debug + testes
```

Se o vswhere não localizar sua instalação:
```
set VELTRIX_VS_PATH=E:\VS_TOOLS
scripts\build-windows.bat
```

## Linux

```
sudo apt install build-essential ninja-build cmake   # ou equivalente
./scripts/build-linux.sh
./scripts/build-linux.sh debug
```

## Manual (presets)

```
cmake --preset win-msvc-release      # ou linux-release
cmake --build --preset win-msvc-release
ctest --preset win-msvc-release --output-on-failure
```

Binários: `build/<preset>/src/veltrix-cli.exe`,
`build/<preset>/tests/veltrix-tests(.exe)`.

## Opções CMake

| Opção | Default | Efeito |
|---|---|---|
| `VELTRIX_BUILD_CLI` | ON | ferramenta veltrix-cli |
| `VELTRIX_BUILD_TESTS` | ON | testes Catch2 |
| `VELTRIX_BUILD_BENCHMARKS` | OFF | benchmarks |
| `VELTRIX_WARNINGS_AS_ERRORS` | OFF | -Werror / /WX |
| `VELTRIX_ENABLE_LTO` | OFF | IPO em Release |

## Verificação executada nesta fundação

- Configure + build Release: **OK** (MSVC 19.51.36256, Ninja, x64).
- Testes: **394 assertions / 75 casos aprovados**.
- CLI smoke: `info`, `new` (com e sem mídia), `check`, `probe` de MP4
  sintético gerado por FFmpeg lavfi (h264 320x240 24/1 + aac 44100 mono,
  3000 ms) — tudo OK, com códigos de saída corretos inclusive nas falhas.
