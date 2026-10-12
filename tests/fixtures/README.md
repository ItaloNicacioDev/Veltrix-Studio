# Test fixtures

## Politica
- **Nada de mídia protegida por direitos autorais.** Fixtures são geradas
  sinteticamente e/ou pequenas o suficiente para redistribuição.
- Testes unitários/integração NÃO dependem de mídia em disco: usam arquivos
  temporários vazios criados pelo próprio teste (o core só valida
  existência/estrutura, não conteúdo de codec).

## Mídia sintética redistribuível
`assets/sample_media/generated_color_bars.mp4` — gerado com FFmpeg lavfi
(sem conteúdo de terceiros):
```
ffmpeg -f lavfi -i testsrc2=duration=3:size=320x240:rate=24 \
       -f lavfi -i sine=frequency=440:duration=3 \
       -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest \
       generated_color_bars.mp4
```
Usado apenas pelo smoke test do CLI (`probe`) e por verificação manual —
não pelos testes automatizados (que não exigem ffmpeg na máquina de CI).

## Para regenerar a fixture
Requer FFmpeg qualquer build (o resultado é mídia própria, sem restrição).
