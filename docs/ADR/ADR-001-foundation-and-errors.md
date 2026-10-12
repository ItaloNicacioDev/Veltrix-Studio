# ADR-001: Fundação C++20 sem UI, com Result-based error handling

**Status**: aceito (fundação 0.1.0)

## Contexto
O Veltrix será um editor com UI Qt Quick/QML. A decisão fundacional: onde
começa o produto — na interface ou no motor? E qual política de erros um
motor multi-thread (jobs, decode, IO) precisa?

## Decisão
1. **Core primeiro, UI nunca acoplada.** O core é uma biblioteca sem
   dependência visual; a UI futura é uma consumidora. Nada de Qt em
   `include/` ou `src/`.
2. **C++20** como padrão (concepts não usados por necessidade; recursos
   usados: `starts_with`, designated-init em partes, `std::variant`,
   span-ready). MSVC 19.30+/GCC 11+/Clang 14+ cobrem o necessário — nenhuma
   limitação técnica exigiu desviar.
3. **Erros por valor (`Result<T>`/`ResultVoid`), sem exceções como fluxo
   normal.** Motivo: falhas (arquivo ausente, codec ruim, projeto corrompido)
   são *esperadas* no domínio; exceção é para invariantes quebrados. Em
   código multi-thread, resultados viajam entre threads sem stack unwinding
   surpresa.
4. Sem singletons: serviços (`Log`, `JobManager`, caches) são construídos
   pelo dono e injetados.

## Consequências
✅ CLI já exercita o core ponta a ponta sem UI nenhuma.
✅ Testes rodam contra a biblioteca pura.
⚠ Serialização JSON pode lançar (nlohmann) — confinada em try/catch nos
  bordões de parse, convertida em `Result` (fronteira anti-exceção).
