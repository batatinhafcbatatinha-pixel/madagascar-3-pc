# Checkpoint — 26/10/2026 ~17h (Claude): GAMEPLAY com renderer D3D11 próprio

Chega ao gameplay (Monte Carlo, Gloria/Marty, HUD, minimapa, diálogos) sem plugin Xenos; 30 fps como no console.
Build: `build-madagascar.cmd release build` via PowerShell com `MADAGASCAR_BUILD_ROOT=D:\CodexBuilds
ative-ports\madagascar3`
(PDB gerado). Teste: `python scripts/run_test.py --seconds 170 --input inputs/load-slot.txt --dump-every 600`
(janela sem foco, controle por arquivo `MADAGASCAR3_INPUT`, `--cdb` para pilhas de AV). Captura completa:
`MADAGASCAR3_DEBUG_DRAWS=400` ou `MADAGASCAR3_DEBUG_AT=<s>` → `captures/` ao lado do exe; `step-region.txt` = probe.

Correções (project/src/native_gpu): sub_822D0498 é EndTiling (não Clear); Clear real = sub_822CE5C8 (achado com
scripts/match_functions.py contra a imagem do Copa); BeginTiling sub_822D0128 + alvos em tamanho cheio durante tiling e
clears de resolve adiados; Clear do host DEPOIS do original; InsertCallback em sub_822D43C0 (Swap insere o callback de flip
0x822CE880 direto); callback de replay 0x822E6220 com dado 0 ignorado (crash 0x4); constantes literais dos shaders
(LOAD_ALU_CONST, tabelas vs+0x368/ps+0x28) — sem elas o mundo saía preto; blocos de registradores 0x2080 e 0x2388
(scissor da janela — corrigiu o minimapa); rastreio de escrita por página nas texturas (16–42 ms → 0,03 ms por frame);
pacing do flip pelo intervalo de apresentação do D3D (device+0x4240 cresce 2 por swap; o jogo rodava 2× rápido);
funções faltantes no manifesto (scripts/function_extent.py tolera VMX128; find_code_func_ptrs.py — ignorar alvos de
jump table depois de bctr). Menu PC compartilhado (src/pc_settings.cpp, F1 / Back+Start): Vulkan via DXVK
(`vulkan\`), AF até 16×, FXAA, vsync, modo de tela, resolução interna; volume ligado ao áudio nativo.

Pendências: travamento se o idioma for confirmado nos ~3 s após a tela aparecer (testes começam a apertar aos 30 s);
FATAL raro de chamada a 0x0; menu F1 ainda com o tema padrão (deve copiar o visual do jogo); MSAA não se aplica ao
G-buffer deferred (usar resolução interna como supersampling + FXAA); usuário questionou a pele da Gloria (cor/trama
vêm da textura difusa; aguardando referência); dumps de frame agora desfazem o BGRA.

# Continuação — Madagascar 3 (25/10/2026)

**Último checkpoint:** Executável recompilado com áudio host FFmpeg/SDL3 e 16.050 mappings CPU; compilador offline de shaders DXBC construído. GPU de frames ainda usa Xenos. Nenhuma execução nesta passagem. Ver analysis/native-integration.json.

## Evidência e tradução

- ISO lida diretamente de `D:\ISO\DreamWorks Madagascar 3 - The Video Game (USA)\DreamWorks Madagascar 3 - The Video Game (USA).iso`. Inventário revalidado: 9.402 arquivos, 4.169.718.232 bytes de recursos; offset XGD3 `0x0FDA0000`. Nenhuma extração completa nesta rodada.
- XEX SHA-256 `38d0d7e6665c89567b0566dff387d2925a0b7f001367b99fe08bfd48e32fd1a0`.
- Decoder local existente `../copa2014/build/analysis-tools/copa_image_inspect.exe` produziu `analysis/decoded/`. Imagem derivada de 9.371.648 bytes (SHA-256 `bda75bf65cdee0542fb511f71a7e341a215f88af20246a204cfe12c30af52ada`), oito seções, 12.627 registros válidos em `.pdata` e 168 imports nomeados. O loader modifica thunks; não é cópia crua dos bytes da mídia.
- `analysis/import-call-xrefs.json`: 654 branches diretos para 163 imports; chamadas indiretas não cobertas. `analysis/ppc-helpers.json`: candidatos save/restore por assinatura estática.
- Manifesto anterior em `config/` era metadado sem o schema do SDK. Preservado como `initial-game-metadata.toml`; manifesto operacional: `project/madagascar3_manifest.toml`.
- `rexglue init` e `codegen` locais (SDK `0.10.0.24-dev.gbd833a2`) concluíram. Primeiro diagnóstico: branch `0x8248533C -> 0x820A70E0` sem função descoberta. Disassembly confirmou leaf até `blr` em `0x820A7154`; próxima função começa em `0x820A7158`. Limites adicionados ao manifesto, sem alteração das instruções. Ver `analysis/leaf-function-fix.json`.
- Tradução: 71 unidades C++ recomp, 15.999 mappings de funções guest e 168 imports. Ver `analysis/codegen-summary.json`. Essas contagens não validam cobertura completa nem execução.

## Build e espaço

`build-madagascar.cmd release codegen|configure|build`: cada etapa inclui as anteriores. CMake configurado com Clang 22.1.2 e SDK já existente; dois jobs, fontes recomp sem debug info. Plugin gráfico selecionado: Xenos do SDK. GPU e áudio ainda dependem do runtime, sem implementação nativa própria validada.

`pipeline.py` exige 20 GiB livres e mantém `project/` abaixo de 2 GiB. Monitor a cada dois segundos interrompe a árvore do processo ao alcançar um limite; pode haver avanço entre verificações. `analysis/disk-guard-checks.json` registra recusa de reserva baixa, recusa de orçamento excedido e admissão do estado atual.

`extract_iso.py`: orçamento adicional padrão de 512 MiB, reserva 20 GiB, seleção por `--include`. Verificado: `--include default.xex` não escreve arquivo já presente; extração completa (~4,15 GB adicionais) é recusada antes de abrir os arquivos de saída. `inspect_iso.py --inventory-only` reconhece o offset XGD3 e evita duplicar XEX.

## Validação e pendências

Compilação Release concluída com exit 0; resultado final registrado abaixo. `scripts/smoke.py` limita execução a 20 segundos e logs/dados de teste a 32 MiB observados, mantendo reserva de 20 GiB. Teste de processo não substitui validação visual.

A maioria dos recursos permanece na ISO. Mesmo com build concluída, boot, menu, gameplay, áudio, controles e saves precisam de evidência própria. Próxima etapa: observar bloqueio real de runtime e disponibilizar somente recursos comprovadamente necessários, respeitando orçamento.

## Resultado final desta rodada

Build Release concluída (primeiro build 79 etapas, incremental posterior 6 etapas). Executável `project/out/build/win-amd64-release/madagascar3.exe`, 24.956.928 bytes antes do último incremento; identidade final em `analysis/build-result.json`. `rexruntime.dll` e `rexgpu-xenos.dll` estão junto ao executável. Launcher e smoke ativam `--gpu_plugin=xenos` explicitamente.

Primeiro smoke com GPU ativa inicializou shaders e detectou controle, mas abortou em função indireta não registrada `0x82321668`. Confirmado leaf `lwz r3,0x18(r3); blr` até `0x82321670`, declarada e recompilada (evidência local `analysis/indirect-leaf-fix.json`). O smoke final avançou para outro alvo indireto ausente, **`0x822F7ED8`**, e saiu com `0xC0000409` em ~3 segundos. Esse é o próximo bloqueador concreto, ainda sem correção nesta rodada. Ver `analysis/runtime-smoke.json` e `analysis/runtime-final.log`.

Também há falhas de resolução para o device opcional `UPDATE:`. Os logs não demonstram que elas causam o abort. Não mascaradas com recursos ou stubs adivinhados. Não houve validação visual de menu/partida nem de áudio, controles em jogo ou saves.

## Integração nativa — continuação sem execução

- `recover_indirect_entries.py` encontrou e aplicou 51 entradas apontadas por dados, fora de `.pdata`, com limites obtidos por CFG. Quatro candidatos ambíguos foram excluídos. O alvo anterior `0x822F7ED8` agora está traduzido até `0x822F7FCC`; isso remove a lacuna estática, sem presumir um resultado de boot. Manifesto e relatório preservam bytes/hashes. Total: **16.050 mappings de funções**.
- Áudio substituído pela fábrica `m3audio::NativeAudioSystem`: decoder FFmpeg XMA + saída SDL3, com sete exports realmente importados pelo jogo. Não instancia a APU do SDK. A biblioteca XMAPlayback ainda usa um adaptador de comandos/contextos; essa compatibilidade e o comportamento ainda precisam de validação. Código em `project/src/native_audio/`, build concluída.
- Compilador offline `madagascar3_shaderc.exe` e biblioteca `madagascar3_native_shaders.lib` compilados. Convertem microcódigo Xenos big-endian em HLSL e DXBC Shader Model 5 usando D3DCompile. Recusam shaders com memexport ainda não integrado. Uso: `madagascar3_shaderc vs|ps arquivo.bin diretorio-novo`. Não foi executado nesta rodada. É uma etapa do renderer nativo, não um renderer completo.
- O desenho de frames ainda usa o plugin Xenos. Falta recuperar a ABI/layouts próprios do D3D deste jogo e ligar draw/clear/resolve/present/recursos ao backend do host. Endereços e offsets do Battlefield não foram copiados como se fossem deste jogo.

Executável e ferramentas recompilados; `analysis/native-integration.json` registra identidades e limites. Nenhum jogo foi executado nesta continuação. Build/codegen mantêm reserva de 20 GiB e teto observado de 2 GiB no projeto.

## Reservas de espaço removidas — 26/10/2026

Por solicitação do usuário, os pipelines não exigem mais reservas mínimas de 20 GiB nem de 1 GiB para fontes/logs. As referências anteriores a essas reservas são históricas. Permanecem os limites de tamanho de artefatos e logs, as verificações de integridade e, nas extrações, a verificação de espaço para os próprios arquivos. A estimativa de build do Ben10 é apenas informativa.
