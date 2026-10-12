# ADR-003: Undo/redo por snapshots de duas pilhas

**Status**: aceito (fundação 0.1.0; revisitar quando custar)

## Contexto
Undo precisa ser infalível: um undo que deixa estado inconsistente destrói a
confiança no editor inteiro. A alternativa — padrão Command com diffs
inversos — exige N classes de comando correta em cada detalhe desde o dia 1.

## Decisão
Snapshot completo da `Sequence` antes de cada operação mutadora validada,
em duas pilhas clássicas (undo guarda pré-op; undo empilha estado atual no
redo). Operação que falha validação não gera snapshot.

## Consequências
✅ Corretude trivial de auditar; testes provam ida-e-volta.
✅ API pública desacoplada do mecanismo — troca futura por diffs não quebra.
⚠ Custo por operação O(tamanho da timeline em clipes); aceitável até
  dezenas de milhares de clipes; benchmark no roadmap antes de trocar.
