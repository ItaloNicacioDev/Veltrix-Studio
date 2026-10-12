# ADR-008: Decodificação por vinculação dinâmica de libav* (Estratégia A)

**Status**: aceito (Fase 2 — decodificação real)

Suplementa o ADR-006 (que definiu o probe por processo externo para
METADADOS e adiou a decisão de decoding). Esta é a decisão de decoding.

## Contexto

O objetivo é um editor profissional com reprodução interativa, seek
responsivo e exportação futura. O ADR-006 exigia comparar:

**A. Vincular libavformat/libavcodec/libavutil (+ swresample/swscale).**
**B. Processo decodificador externo com transporte de quadros.**

## Comparação

| Critério | A: linked | B: processo externo |
|---|---|---|
| Latência de seek/preview | ✅ direto no process | ⚠ IPC + marshalling por quadro (cópia e RTT) |
| Throughput (4K60+) | ✅ zero overhead de transporte | ⚠ custo de serializar megabytes/frame |
| Manutenção | API C estável, amplamente documentada | protocolo próprio versionado a manter |
| Complexidade | moderada (RAII sobre handles C) | alta (IPC, framing, backpressure, restart) |
| Segurança/estabilidade | ⚠ codec crash derruba o editor | ✅ crash isolado no host |
| Distribuição | DLLs FFmpeg junto ao app | binário externo junto ao app |
| Licenciamento | depende do build (ver ADR-006/DEPENDENCIES) | igual (mesmas libs dentro do host) |

**Decisão: A (linked, DINÂMICO).** Para um editor com preview interativo, a
latência e o throughput por quadro são requisitos de produto; a isolamento
de crash de B é valiosa, mas seu custo (protocolo, cópias, complexidade)
atrapalha justamente o caminho crítico. B permanece o modelo alvo para
PLUGINS de terceiros (ADR-007) — situações diferentes, decisões diferentes.
Mitigamos o risco de instabilidade com: (1) build LGPL oficial sem codecs
experimentais; (2) limits de memória/erros explícitos no contrato; (3) um
futuro "decoder sandbox" pode reusar a MESMA interface `IDecoder` fora do
processo sem tocar nos consumidores.

## Requisitos técnicos da integração

1. **Isolamento total**: tipos `AV*` existem SOMENTE dentro de
   `src/media/ffmpeg/` (não em `include/`, não em outros módulos). O core
   público fala apenas com os contratos existentes.
2. **LGPL compliance via dynamic linking**: distribuímos as DLLs FFmpeg
   separadas do binário Veltrix; usuários podem trocá-las por builds
   próprios (relinkagem = trocar DLL). Nenhum código FFmpeg é copiado.
   Build usado: BtbN win64 **LGPL** shared n9.0 (sem --enable-gpl ⇒ sem
   x264 no runtime; decode H.264 usa o decoder nativo LGPL da FFmpeg).
3. **Versão mínima**: FFmpeg 6.1 (APIs novas de ch/ibp usadas com guard);
   testado contra n9.0 LGPL shared. No Linux: pacotes de distro (libav*-dev).
4. **Descoberta do SDK em build**: variável `VELTRIX_FFMPEG_DIR` (raiz com
   include/ e lib/) ou pkg-config (Linux); CMake `find_library` — nunca
   path absoluto hardcoded. Sem SDK ⇒ alvo `veltrix-ffmpeg` e seus testes
   são desligados com mensagem clara (build do resto segue funcionando).
5. **Runtime (Windows)**: DLLs resolvidas ao lado do executável, depois
   `VELTRIX_FFMPEG_BIN`, depois PATH. (Linux): libav*.so do sistema.

## Consequências

✅ Seek/decode com latência mínima; caminho para export direto.
✅ Core público permanece 100% limpo de `AV*`.
⚠ Binário final exige DLLs FFmpeg compatíveis em runtime (documentado).
⚠ Instabilidade de codec extrema derruba o editor — mitigação documentada;
  sandbox por processo é possível futuramente atrás da mesma interface.
