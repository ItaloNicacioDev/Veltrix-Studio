# Benchmarks

Micro-benchmarks de escala do core, sem dependências externas (harness
próprio com `std::chrono`). Ativáveis com `-DVELTRIX_BUILD_BENCHMARKS=ON`.

## Objetivo atual
Medir onde os snapshots de undo (ADR-003) começam a pesar e dar baseline
para o parser de projetos — os dois custos óbvios da fundação.

## Execução

```
cmake --preset win-msvc-release -DVELTRIX_BUILD_BENCHMARKS=ON
cmake --build --preset win-msvc-release
build/win-msvc-release/benchmarks/veltrix-benchmarks.exe
```

## Números medidos (MSVC 19.51 x64 Release, máquina de dev, 2026-10-11)

```
undo-snapshots  clips=1000   insert_total_ms=13.5     undo_avg_ms=0.008
undo-snapshots  clips=5000   insert_total_ms=119.5    undo_avg_ms=0.099
undo-snapshots  clips=20000  insert_total_ms=2730.1   undo_avg_ms=0.163
roundtrip       clips=1000   serialize_ms=1.8   deserialize_ms=2.7    bytes=176 KB
roundtrip       clips=5000   serialize_ms=11.0  deserialize_ms=36.4   bytes=885 KB
roundtrip       clips=20000  serialize_ms=80.7  deserialize_ms=354.6  bytes=3.5 MB
```

Leitura: o **undo por snapshot é barato** mesmo a 20k clipes (0,16 ms por
operação) — o ADR-003 aguenta muito além da escala alvo. Já o **insert é
quadrático** (validação de sobreposição percorre a trilha + insert ordenado
O(n) por clipe): 20k inserts custam 2,7 s. Aceitável para projetos típicos;
a otimização (busca binária + validação por vizinhos) está no roadmap antes
de qualquer uso massivo. Serialização é linear e rápida; o parse ~4x mais
lento (JSON) — compactação e parser incremental são extensões futuras.

