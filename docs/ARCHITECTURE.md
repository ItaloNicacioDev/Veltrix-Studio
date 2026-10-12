# Arquitetura do Veltrix Core

## Visão geral

O core é uma biblioteca estática (`veltrix-core`) sem dependência de UI e sem
acesso direto a APIs visuais. Camadas (dependências apontam para baixo):

```
┌────────────────────────────────────────────┐
│  CLI / futura UI (Qt Quick)  [fora do core]│
├────────────────────────────────────────────┤
│  project        modelo + persistência       │
│  timeline       sequências, trilhas, clipes │
│  media          registro, probe (ffprobe)   │
├────────────────────────────────────────────┤
│  playback/render/audio/color  contratos e  │
│                               avaliação    │
├────────────────────────────────────────────┤
│  cache/jobs/plugins/io/diagnostics          │
├────────────────────────────────────────────┤
│  core          Result, Ids, Log, Time(rat), │
│                cancelamento, config, math   │
└────────────────────────────────────────────┘
```

Regras estruturais:
- **Sem singletons globais.** Serviços são objetos criados e injetados
  explicitamente (ex.: `Log` é construído pelo dono e passado adiante).
- **Sem exceções como fluxo normal.** Toda operação falhível retorna
  `Result<T>` / `ResultVoid` com `ErrorCode` estável (ver abaixo).
- **RAII e propriedade explícita**: `std::unique_ptr` para posse exclusiva,
  `std::shared_ptr` apenas onde há compartilhamento real (cache, frames).
- **Core independente de SO** sempre que viável; código de processo/pipe do
  probe é o único bloco por plataforma e vive isolado em `MediaProber.cpp`.

## Modelo temporal (ADR-002)

**O tempo de edição nunca é `double`.** Posições e durações são `EditTicks`
(int64) interpretados contra uma `Timebase` (racional exato
segundos-por-tick). Uma timebase construída "from fps" faz 1 tick = 1 frame
daquela taxa, incluindo NTSC (30000/1001 permanece exato).

- Conversões entre timebases passam por segundos racionais exatos — editar a
  23,976 e exportar a 30 fps não acumula erro de arredondamento dentro da
  faixa int64.
- `double` só aparece na fronteira (ffprobe, entrada do usuário) via
  `ticks_for_double_seconds` com arredondamento documentado (half away from
  zero) e rejeição de não-finitos.
- Toda aritmética de ticks passa por `checked_add/sub/mul` (sem overflow
  silencioso).

## Política de erros

`Error { code, message, context }`. Códigos são enum estável
(`InvalidArgument`, `OutOfRange`, `NotFound`, `AlreadyExists`, `Conflict`,
`IoError`, `ParseError`, `VersionUnsupported`, `Unsupported`, `Cancelled`,
`MediaUnavailable`, `ProbeFailed`, `PluginIncompatible`, `ValidationFailed`).
Nunca renumerar; apenas anexar. Mensagens nunca contêm segredos nem caminhos
privados desnecessários (regra dos diagnósticos).

## Persistência (ADR-004)

`.veltrix` = JSON com `schema_version` no topo. O loader **recusa** versões
mais novas (`VersionUnsupported`) em vez de adivinhar. Migrações serão
funções registradas por passo (1→2→…), executadas em ordem. Escrita é
atômica: temp + rename, nunca arquivo parcial. Ids viajam como strings
u64 e são restaurados exatamente — nunca regenerados.

## Undo/redo (ADR-003)

Snapshots completos da `Sequence` em duas pilhas. Correto e trivial de
auditar; na escala atual (milhares de clipes) o custo por operação é uma
cópia ordenada de vetores. Quando custar, troca-se por diffs atrás da
mesma API pública.

## Concorrência (ADR-005)

Um worker FIFO no `JobManager`. Regras explícitas: o manager nunca destrói
job em execução; cancelamento é latch one-way observado por polling do
token; estados finais nunca regridem. Um pool real só entra quando houver
carga que justifique — com as mesmas garantias contratuais.

## Mídia (ADR-006)

O core **não** linka libav*. Análise técnica usa o binário `ffprobe`
descoberto em runtime (PATH / `VELTRIX_FFPROBE` / caminho explícito), com
timeout e parse JSON. Isso mantém o build limpo, a licença do core
desacoplada das opções de compilação do FFmpeg, e a decisão de linking
adiada com ADR documentada. Quando o decoding real entrar, a fronteira é a
interface `IDecoder` já definida e testada com implementações fake.

## Plugins (ADR-007)

Manifesto JSON + checagem de compatibilidade implementados. **Nenhum
carregamento dinâmico existe nesta etapa.** Plugins de terceiros são código
nativo arbitrário: o destino arquitetural é um host fora do processo. Nada
de plugins não confiáveis no processo principal — risco documentado.

## Limitações conhecidas

- Janela do timecode SMPTE só para fps inteiro; drop-frame NTSC reporta
  `Unsupported` explicitamente.
- Caminhos no Windows usam a code page do sistema (não UTF-8 end-to-end);
  migração para UTF-8 está no roadmap (edição via u8path deprecado).
- `mark_failed` não captura o `Error` do job ainda — os estados são
  observáveis, o erro detalhado ainda não (próximo passo do jobs).
- Sem resampling de áudio (mixagem exige mesma taxa, erro explícito).
- Uma sequência por projeto (multi-sequência no roadmap).
