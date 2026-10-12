# ADR-006: FFmpeg como ferramenta externa em runtime (sem linking) nesta etapa

**Status**: aceito para a fundação; revisão necessária antes do decoding real

## Contexto
FFmpeg é a infraestrutura de mídia óbvia, mas (a) "FFmpeg" não é UMA
licença: a efetiva depende das flags de build (LGPL base; GPL quando
`--enable-gpl`/x264; há componentes com cláusulas próprias); (b) linkar
herda a escolha para o binário do Veltrix; (c) obter um build auditável
multi-plataforma é trabalho próprio, não um download.

## Decisão (etapa atual)
- O core NÃO linka libav*. Análise técnica (`probe`) executa o binário
  `ffprobe` localizado em runtime (PATH / `VELTRIX_FFPROBE` / explícito),
  com timeout, e parseia o JSON de saída. Sem caminhos fixos no build.
- Falhas explícitas: binário ausente, timeout, exit ≠ 0, JSON inválido,
  nenhum stream decodificável.

## Decisões pendentes (antes do decode)
1. Linkar libav (qual build/licença) vs. processo decodificador com IPC.
2. Se linkar: dinâmico (LGPL viável com relinkagem) vs. estático (GPL).
3. Build próprio via FetchContent/ExternalProject com flags auditadas.
4. Isolamento de codecs potencialmente instáveis fora do processo do
   editor (crash de codec ≠ crash do editor).

## Consequências
✅ Core compilável sem FFmpeg; licença do binário hoje não contaminada.
✅ Probe funciona e é testado sobre mídia sintética.
⚠ Sem decode: playback/export reais dependem da revisão deste ADR.
