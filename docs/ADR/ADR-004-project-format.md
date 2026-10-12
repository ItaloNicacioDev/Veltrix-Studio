# ADR-004: Formato de projeto JSON versionado

**Status**: aceito (fundação 0.1.0)

## Contexto
Projetos são o ativo do usuário: precisam ser legíveis, diffáveis, resistentes
a corrupção parcial e evoluíveis sem quebrar versões antigas.

## Decisão
- `.veltrix` = JSON UTF-8 com `schema_version` inteiro no topo (atual: 1).
- Loader estrito: tipos errados e campos estruturais ausentes = erro
  (nunca objeto parcialmente preenchido). Versões MAIORES desconhecidas =
  `VersionUnsupported` — recusa, sem adivinhação.
- Migração futura: funções registradas por passo (1→2→…), aplicadas em
  ordem; writer sempre emite a versão atual do código.
- Ids serializados como strings u64; restaurados exatamente, nunca
  regenerados (referências internas dependem disso).
- Escrita atômica: temp no mesmo diretório + rename. Sem arquivo parcial.
- Load não exige mídia presente (edição offline); disponibilidade é
  snapshot recalculado por `refresh_availability()`.

## Consequências
✅ Round-trip testado (ids, tracks, clipes, mídia preservados).
✅ Projeto legível/diffável em git.
⚠ JSON é mais verboso que binário; compactação é extensão futura opcional.
