# ADR-005: Jobs com um worker FIFO e regras de concorrência explícitas

**Status**: aceito (fundação 0.1.0)

## Contexto
Tarefas (import/probe/thumb/render/export) precisam de cancelamento,
progresso e encerramento ordenado. Um pool genérico prematuro complicaria a
verificação de posse de recursos sem ganho real na escala atual.

## Decisão
- `JobManager` com UM worker FIFO. `Job` = objeto com estados com latch
  (Queued→Running→final; final nunca regride), progresso clampado [0,1],
  token de cancelamento.
- Regras: manager nunca destrói job em execução; handles via
  `shared_ptr` (quem segura mantém vivo); cancel de fila E de execução;
  shutdown cancela pendentes e faz join; wait_all drena.
- Implementações pollam o token e devolvem `failure(Cancelled, …)`.

## Consequências
✅ Determinismo em testes; cancelamento comprovado em fila e em execução.
⚠ Sem paralelismo de tarefas — intencional até existir carga real
  (decoding) e medição; pool por tipo de recurso entra com os mesmos
  contratos.
⚠ `mark_failed` ainda não carrega o `Error` dentro do job (roadmap).
