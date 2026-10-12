# Compilando o Veltrix Core

## Requisitos

| Ferramenta | Versão mínima | Observação |
|---|---|---|
| CMake | 3.24 | presets |
| Compilador | MSVC 19.30+ / GCC 11+ / Clang 14+ | C++20 |
| Ninja | 1.10 | gerador padrão dos presets |
| **FFmpeg SDK (opcional)** | libavformat/avcodec/avutil/swresample/swscale — testado com n9.0 LGPL | habilita a camada de decodificação |

Sem dependências externas para compilar o CORE: as bibliotecas de
terceiros (nlohmann/json, Catch2) estão vendored em `third_party/`.

**FFmpeg (camada de decode)** — opcional em build, descoberto assim:

1. Defina `VELTRIX_FFMPEG_DIR` (variável CMake OU de ambiente) apontando
   para a raiz do SDK com `include/` e `lib/`.
   Windows (exemplo, BtbN win64-lgpl-shared):
   https://github.com/BtbN/FFmpeg-Builds/releases →
   `ffmpeg-n9.0-latest-win64-lgpl-shared-9.0.zip` → extrair → apontar
   `VELTRIX_FFMPEG_DIR` para a pasta extraída.
2. Linux: pacotes de distro (`sudo apt install libavformat-dev
   libavcodec-dev libavutil-dev libswresample-dev libswscale-dev`) —
   encontrados via busca padrão do CMake, sem variável.

**Sem o SDK**, `veltrix-ffmpeg` e seus testes são pulados com aviso
explícito — o resto do projeto compila idêntico (comando `decode` do CLI
responde "no decode engine").

**Runtime (Windows)**: os binários linkados contra FFmpeg (veltrix-cli,
veltrix-tests, veltrix-bench-decode) precisam das DLLs `av*.dll`/`sw*.dll`
ao lado do executável (ou no PATH). Em distribuição, as DLLs LGPL
acompanham o pacote separadas do binário (relinkagem = trocar DLL).
**Linux**: `libav*.so` do sistema.

**FFmpeg binário (runtime de testes)**: os testes de decode GERAM
fixtures sintéticas (lavfi — conteúdo próprio, redistribuível) invocando
o `ffmpeg`; descoberta via PATH/`VELTRIX_FFPROBE` (mesma do probe).
Sem o binário, esses casos fazem SKIP explícito — nunca passam à toa.

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

- Configure + build Release: **OK** (MSVC 19.51.36256, Ninja, x64) — com
  camada FFmpeg LGPL 9.0 linkada (ADR-008).
- Testes: **1631 assertions / 90 casos aprovados** (inclui decodificação
  real de vídeo/áudio, seek exato, cancelamento, destruição segura).
- CLI smoke: `info`, `new` (com e sem mídia), `check`, `probe`, `decode`
  (frames RGBA8 reais com layout verificado) — tudo OK, com códigos de
  saída corretos inclusive nas falhas.
- Benchmarks de decode executados (ver benchmarks/README.md).
