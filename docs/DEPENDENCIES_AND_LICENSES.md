# Dependências e licenças

Política: o mínimo necessário; nenhuma dependência paga; nenhuma dependência
obrigatória para compilar que restrinja a distribuição.

## Vinculadas agora (vendored, header-only)

| Biblioteca | Versão | Licença | Finalidade | Windows | Linux | Obrigatória? |
|---|---|---|---|---|---|---|
| nlohmann/json | 3.11.3 | MIT | serialização do projeto, parse de config/manifestos/probe | ✅ | ✅ | Sim (persistência) |
| Catch2 | 2.13.10 | BSL-1.0 | testes unitários/integração | ✅ | ✅ | Apenas testes |

Redistribuição: MIT e BSL-1.0 são permissivas e compatíveis com GPL-3.0
(seção "compatible licenses"); os arquivos com copyright/licença estão em
`third_party/{nlohmann,catch2}/`. Catch2 não é linkado ao core — só ao
executável de testes.

## Usadas em runtime (não linkadas ao core)

| Ferramenta | Licença | Como usada | Observações de licença |
|---|---|---|---|
| FFmpeg/ffprobe | LGPL-2.1+ **ou** GPL-2+ conforme build | binário externo localizado em runtime (PATH/`VELTRIX_FFPROBE`) | O core NÃO linka e NÃO redistribui FFmpeg. A licença efetiva depende das flags do build usado (`--enable-gpl`, `--enable-libx264` ⇒ GPL). Nesta máquina há um build GPL (x264), usado apenas como ferramenta de desenvolvimento; a decisão de distribuição está no ADR-006. |

## Avaliadas e ainda NÃO integradas

| Tecnologia | Licença | Status |
|---|---|---|
| libav* (linking) | LGPL/GPL por build | Decisão adiada — ADR-006. Linkar copia a escolha de licença para o binário; precisa de build próprio auditável. |
| OpenFX | BSD-3-Clause | Investigação futura para efeitos. Nenhuma promessa de compatibilidade com plugins do Premiere. |
| VST3 SDK | Proprietária (licença Steinberg com restrições; **não é open source**) | Impedimento real para um produto open source puro: exigiria binário separado/cláusula de sistema. Reavaliar ou substituir por CLAP (MIT) antes de prometer. |
| spdlog/fmt | MIT | Desnecessárias agora: `Log` próprio atende sem custo extra. |

## Consequências para a distribuição

- O core+CLI hoje: GPL-3.0 (projeto) + MIT/BSL (compatíveis). Sem ônus.
- Quando linkar libav LGPL: o binário final precisa permitir relinkagem
  (fornecer objetos/estático com fonte) — build dinâmico simplifica.
- Quando linkar libav GPL (x264 etc.): o binário inteiro vira GPL — aceitável
  para o Veltrix (GPL-3.0), mas deve ser declarado explicitamente.
- VST3: exigiria relicenciamento ou isolamento forte — não prometer.
