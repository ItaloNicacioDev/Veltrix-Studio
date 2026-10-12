# ADR-002: Tempo de edição como ticks inteiros com timebase exata

**Status**: aceito (fundação 0.1.0)

## Contexto
Editores que representam tempo em `double` acumulam erro: 29,97 não é
representável em binário; somas repetidas divergem; projetos corrompem em
taxas mistas; cortes "no frame" caem fora do frame.

## Decisão
- Posições/durações de edição: `EditTicks` = int64.
- Interpretação: `Timebase` = racional exato "segundos por tick"
  (`from_fps(n, d)` ⇒ 1 tick = 1 frame de n/d fps; NTSC 30000/1001 exato).
- Conversões entre timebases: por segundos racionais exatos.
- `double` somente na fronteira IO com arredondamento documentado
  (half away from zero) e rejeição de não-finitos.
- Aritmética com overflow checado (`MathUtils`).

## Consequências
✅ Zero drift em qualquer taxa comum; cortes exatos; diff/undo estável.
✓ Testes cobrem NTSC exato, conversão cruzada 24↔30, negativos, não-finitos.
⚠ Timecode drop-frame NTSC fica fora desta etapa (reporta `Unsupported`).
