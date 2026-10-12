# ADR-007: Plugins — manifesto primeiro, hospedagem fora do processo

**Status**: aceito (fundação 0.1.0); hospedagem = planejado

## Contexto
Plugins de terceiros são código nativo arbitrário. Carregá-los no processo
principal dá a eles o end space do editor: um plugin com defeito trava o
editor; um plugin malicioso rouba projetos. Além disso, prometer
compatibilidade com formatos proprietários (ex.: efeitos do Premiere,
VST3 sem ler a licença) é irresponsável antes de análise.

## Decisão
- Agora: manifesto JSON estrito (id, nome, vendor, versão semver-ish,
  api_version, tipo, entry_library) + checagem de compatibilidade.
  `api_version` MAIOR que a atual = recusa (`PluginIncompatible`),
  nunca "melhor esforço".
- Futuro: hospedagem FORA do processo (host dedicado com IPC), descoberta
  em diretório de plugins, timeout e capacidade de kill/restart. Nada de
  plugins não-confiáveis in-process — sem exceções.
- SDK documentado só quando existir API estável para prometer.
- VST3: licença Steinberg é proprietária com restrições — inviável como
  requisito de um produto open source puro; alternativa a avaliar: CLAP
  (MIT). OpenFX (BSD-3) é o caminho provisório para efeitos visuais.
  Compatibilidade com plugins do Premiere: não prometida, exige análise
  específica caso a caso.

## Consequências
✅ Parsing/compatibilidade já testados; nenhum risco de execução hoje.
⚠ Hospedagem real é trabalho significativo (IPC, sandbox) — roadmap.
