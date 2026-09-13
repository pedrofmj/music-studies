# Pedro Performance Rig

Este diretorio contem a definicao autoral portatil do primeiro Performance Rig.
A versao do schema `music-studies/performance-rig/v1` usa JSON Schema Draft
2020-12.

Estado atual: schema, stable-slot, Hardware Preset, Device Profile, Rig Profile,
extracao do Linux Platform Binding, envelope de compilacao deterministica,
gatilhos dos pads, candidatos mapeados de synthv1/setBfree e projetos de genero
materializados. Os arquivos autorais continuam sendo a fonte da verdade para a
selecao de candidatos; o `setup.json`, o projeto Carla, os servicos e o grafo
protegidos continuam sendo a autoridade de producao e o estado padrao de
recuperacao.

`rig.json` registra os cinco slots estaveis de controladores do rack protegido.
Cada slot ordena seletores de modelo, alias semantico e finalidade do endpoint,
ate um discriminador local opcional e um USB ID opcional. Os quatro dispositivos
M-VAVE compartilham o USB ID `4353:4b4d`, portanto esse ID e apenas evidencia de
apoio e nao pode diferencia-los sozinho. Os platform bindings resolvem esses
seletores portateis para as identidades de dispositivos do sistema operacional.

Todos os seis IDs de Hardware Preset resolvem para arquivos verificados em
`hardware-presets`. As atribuicoes do SMC-PAD e do SMC-PAD Pocket sao conferidas
com o [live capture de 2026-08-11](../../../docs/tools/music-rig/benchmarks/hardware-preset-airstar-2026-08-11.json),
incluindo as atribuicoes exatas de canal/nota de cada pad e os oito pads de
controle internos do Pocket.

Os Device Profiles atuais sao resolvidos em `device-profiles/<slot>/<id>`:

- Arturia `multi-instrument-rack`;
- SMK-25 `ambient-pad-layers`;
- SMC-Mixer `eight-band-eq`;
- SMC-PAD `drum-set`; e
- SMC-PAD Pocket `drum-set`.

O S2 adiciona perfis candidatos da Arturia:

- `tonewheel-organ`, com nove mapeamentos de drawbars e controles de orgao;
- `tonewheel-organ-setbfree`, com o mesmo layout da Arturia e backend MIDI setBfree;
- `synth-programmer`, com mapeamentos de oscilador, filtro, envelope, modulacao e efeitos;
- `synth-programmer-synthv1`, com um mapa de controle nativo do synthv1; e
- treze perfis de genero usando conjuntos de patches SoundFont/DecentSampler materializados.

Os candidatos setBfree e synthv1 usam suas proprias engines headless e a
superficie de controle Arturia verificada. Os perfis de genero usam projetos
Carla candidatos separados, gerados a partir de
`docs/tools/music-rig/benchmarks/arturia-genre-patches.json`. Todos os candidatos
continuam sendo sessoes explicitas e reversiveis; o projeto Carla live protegido
nao e substituido.

Eles conectam mapeamentos semanticos aos controles do Hardware Preset
selecionado e declaram capacidades, propriedade, dependencias, prontidao,
estado, takeover e seguranca de troca sem caminhos de plataforma ou
identificadores de backend. Os dois perfis de pads compartilham
intencionalmente `drum-set.notes`; as demais propriedades atuais continuam
exclusivas ou somente leitura.

[`rig-profiles/full-live-rack.json`](rig-profiles/full-live-rack.json) combina
os cinco papeis como Rig Profile padrao e de fallback. Ele declara as capacidades
agregadas de endpoint e musicais, fixa as dezoito engines de som atuais,
identifica a engine de bateria e os efeitos compartilhados e mantem as politicas
seguras de takeover e rollback. E um arquivo autoral; o setup protegido continua
sendo o padrao ativo. O catalogo de gerenciamento resolvido em
[`switch-triggers.json`](switch-triggers.json) mapeia os 16 pads da Arturia para
os perfis de orgao, synth, live e genero.

[`rig-profiles/tonewheel-organ.json`](rig-profiles/tonewheel-organ.json),
[`rig-profiles/tonewheel-organ-setbfree.json`](rig-profiles/tonewheel-organ-setbfree.json)
e os perfis de synth/genero combinam alternativas da Arturia com os papeis
inalterados do SMK-25, SMC-Mixer, SMC-PAD e Pocket. `full-live-rack` continua
sendo o padrao protegido e o fallback.

Os sete schemas sao:

- `common.schema.json`: tipos compartilhados de identificadores, seletores, capacidades, MIDI, propriedade, prontidao, takeover, estado e seguranca de troca;
- `rig.schema.json`: o catalogo completo do Rig e os slots estaveis de dispositivos;
- `rig-profile.schema.json`: uma composicao global;
- `device-profile.schema.json`: um papel para um slot de dispositivo logico;
- `hardware-preset.schema.json`: atribuicoes de mensagens brutas locais do controlador;
- `platform-binding.schema.json`: identidades de dispositivos do backend, localizadores de alvos semanticos, caminhos de maquina, recursos de ciclo de vida e status de evidencia; e
- `switch-triggers.schema.json`: eventos de gerenciamento persistentes traduzidos para as mesmas operacoes futuras de troca usadas pela CLI.

As definicoes autorais de nivel superior contem capacidades semanticas. Caminhos
do sistema operacional, nomes de portas PipeWire/JACK, indices de parametros da
Carla, IDs de dispositivos Windows e identificadores de servicos pertencem
somente aos Platform Bindings. O binding Linux autoral
[`airstar-current`](platform-bindings/linux/airstar-current.json) resolve o
contrato completo do `full-live-rack` contra o setup protegido. Ele e somente
autoral e nao pode alterar nem ativar o runtime. O documento Windows nos
fixtures do validador comprova o formato portatil do contrato; ele nao e suporte
fisico nem certificacao para Windows.

A validacao da raiz verifica o Rig, todos os Hardware Presets, Device Profiles,
Rig Profiles, Platform Bindings e Switch Triggers. Ela resolve referencias de
slot, modelo, endpoint, preset, controle, perfil, capacidade, recurso, binding,
origem do gatilho e operacao do gatilho; verifica cobertura dos slots exigidos,
capacidades e prontidao agregadas, recursos fixados e compartilhados,
propriedade do estado inicial, conflitos explicitos de propriedade da
composicao, atribuicao MIDI de gerenciamento, limites de prontidao e conflitos
de mapeamento de eventos consumidos; e fixa o binding Linux atual aos aliases,
caminhos, checksums e servicos protegidos da Airstar. Ela le apenas arquivos
autorais e evidencias protegidas; nao se conecta ao rig em funcionamento.

Execute o conjunto offline de schemas a partir da raiz do repositorio depois de
instalar a dependencia exclusiva de autoria:

```bash
python3 -m pip install -r docs/tools/music-rig/requirements-schema.txt
python3 docs/tools/music-rig/validate-performance-rig.py --self-test
python3 docs/tools/music-rig/validate-performance-rig.py \
  --validate-root src/performance-rigs/pedro-performance-rig \
  --authority-setup docs/tools/airstar-live-setup/setup.json \
  --authority-pad-capture \
    docs/tools/music-rig/benchmarks/hardware-preset-airstar-2026-08-11.json

python3 docs/tools/music-rig/compile-performance-rig.py \
  --rig-root src/performance-rigs/pedro-performance-rig \
  --platform-binding airstar-current \
  --check-only
```

O compilador grava somente em um caminho de saida explicito e se recusa a
sobrescrever a fonte autoral. Seus contratos de consulta do Milestone 2,
propriedade, graph-delta e fingerprint estao documentados em
[`COMPILER.md`](../../../docs/tools/music-rig/COMPILER.md). Ele nao materializa
nem ativa estado de Carla, Patchbay, MIDI, audio, servico ou runtime.

## Guia Do Musico

A Arturia KeyLab Essential 61 mk3 pode selecionar os seguintes modos pelos seus
16 pads. Os bancos A e B tem oito pads cada. O gerenciamento dos pads usa o canal
MIDI 10; notas normais do teclado usam o canal 1 e nao sao gatilhos de modo.

| Pad | Modo |
| --- | --- |
| Bank A 1 | Tonewheel Organ (`tonewheel-organ-setbfree`) |
| Bank A 2 | Synth Programmer (`synth-programmer-synthv1`) |
| Bank A 3 | Full Live Rack (`full-live-rack`) |
| Bank A 4 | Worship Piano (`worship-piano`) |
| Bank A 5 | Gospel Keys (`gospel-keys`) |
| Bank A 6 | Ambient Worship (`ambient-worship`) |
| Bank A 7 | Jazz Keys (`jazz-keys`) |
| Bank A 8 | Soul and R&B (`soul-rnb`) |
| Bank B 1 | Cinematic Strings (`cinematic-strings`) |
| Bank B 2 | Orchestral (`orchestral`) |
| Bank B 3 | Brass and Winds (`brass-winds`) |
| Bank B 4 | Synthwave (`synthwave`) |
| Bank B 5 | Retro Keys (`retro-keys`) |
| Bank B 6 | Intimate Pads (`intimate-pads`) |
| Bank B 7 | Praise Leads (`praise-leads`) |
| Bank B 8 | Acoustic Worship (`acoustic-worship`) |

Inicie a sessao explicita de usuario Linux na `airstar` antes de tocar:

```bash
systemctl --user start music-rig-arturia-profile-session.service
```

Pare-a depois de tocar. Isso restaura `full-live-rack` e remove os processos e
links candidatos:

```bash
systemctl --user stop music-rig-arturia-profile-session.service
```

O encoder central da Arturia e o volume Master em todos os modos. Pressiona-lo
alterna o mute Master. Os faders e knobs abaixo usam as mesmas posicoes fisicas
em todos os projetos de genero: cada fader controla o volume do patch listado e
o knob abaixo dele controla o reverb desse patch.

### Full Live Rack

Este e o modo padrao e de recuperacao atual protegido.

| Fader | Instrumento | Knob | Funcao |
| --- | --- | --- | --- |
| 1 | Basic Piano | 1 | Reverb do Basic Piano |
| 2 | Nord White Grand | 2 | Reverb do Nord White Grand |
| 3 | Alt Strings | 3 | Reverb do Alt Strings |
| 4 | Good Flute | 4 | Reverb do Good Flute |
| 5 | SAX Lirakeys | 5 | Reverb do SAX Lirakeys |
| 6 | Hammond Organ Fast | 6 | Reverb do Hammond Organ Fast |
| 7 | Optik Synth | 7 | Reverb do Optik Synth |
| 8 | PAD EFEITOS | 8 | Reverb do PAD EFEITOS |
| 9 | AtmosferaPAD | 9 | Reverb do AtmosferaPAD |

### Synth Programmer

| Controle | Funcao |
| --- | --- |
| Fader 1 | Mix do oscilador 1 (`DCO1_BALANCE`) |
| Fader 2 | Nivel/equilibrio do oscilador 2 (`DCO2_BALANCE`) |
| Fader 3 | Corte do filtro (`DCF1_CUTOFF`) |
| Fader 4 | Ressonancia do filtro (`DCF1_RESO`) |
| Fader 5 | Ataque do amplificador (`DCA1_ATTACK`) |
| Fader 6 | Release do amplificador (`DCA1_RELEASE`) |
| Fader 7 | Profundidade da modulacao (`LFO1_BALANCE`) |
| Fader 8 | Velocidade da modulacao (`LFO1_RATE`) |
| Fader 9 | Unisono/detune |
| Knob 1 | Drive/compressao |
| Knob 2 | Envio/mix de efeitos |
| Knob 3 | Tempo do delay |
| Knob 4 | Feedback do delay |
| Knob 5 | Tamanho do reverb |
| Knob 6 | Mix do reverb |
| Knob 7 | Modulacao de pitch |
| Knob 8 | Volume do synth (`OUT1_VOLUME`) |
| Knob 9 | Largura do reverb do synth |

### Tonewheel Organ

| Fader | Drawbar | Knob | Funcao |
| --- | --- | --- | --- |
| 1 | 16' | 1 | Ativacao/modo da percussao |
| 2 | 5 1/3' | 2 | Decay da percussao |
| 3 | 8' | 3 | Entrada do overdrive |
| 4 | 4' | 4 | Selecao de vibrato/coro |
| 5 | 2 2/3' | 5 | Rotary parado/lento/rapido |
| 6 | 2' | 6 | Saida do overdrive |
| 7 | 1 3/5' | 7 | Roteamento do vibrato |
| 8 | 1 1/3' | 8 | Volume swell |
| 9 | 1' | 9 | Mix do reverb |

Os drawbars sao invertidos como em um orgao fisico: para baixo e mais alto e
para cima e desligado.

### Conjuntos De Patches Por Genero

Os pads 4 a 16 usam projetos Carla materializados, construidos a partir das
bibliotecas SoundFont e DecentSampler instaladas. Os caminhos exatos dos patches
de origem estao registrados em
[`arturia-genre-patches.json`](../../../docs/tools/music-rig/benchmarks/arturia-genre-patches.json).

| Modo | Faders 1 a 9, nesta ordem |
| --- | --- |
| Worship Piano | Wurlitzer, CFX Grand, Slinky Violin Duet, JF Gospel Organ, Cinematic Strings, JF Worship Piano, Sky Pad Evolving, Endless Church Chime, Worship Guitar |
| Gospel Keys | Wurlitzer, Motif ES6 Piano, Slinky Violin Duet, JF Gospel Organ, Alto Sax, Dark Violins, Choir Korg Aahhs, Laboriel Slap Bass, Shiny Effects Pad |
| Ambient Worship | Sky Pad Evolving, CFX Grand, Subfrost Harmonic Bow, Cinematic Strings, Shiny Effects Pad, CS-20M Big Waves, JF Worship Piano, Endless Church Chime, Choir Korg Aahhs |
| Jazz Keys | Wurlitzer, Rhodes VS Extreme, Slinky Violin Duet, Motif ES6 Piano, Alto Sax, Dark Violins, Worship Guitar, Short Scale Bass, Endless Church Chime |
| Soul and R&B | Wurlitzer, Rhodes VS Extreme, Slinky Violin Duet, JF Worship Piano, Alto Sax, Laboriel Slap Bass, Worship Guitar, Dark Violins, Shiny Effects Pad |
| Cinematic Strings | Sky Pad Evolving, Dark Violins, Subfrost Harmonic Bow, CFX Grand, Cinematic Strings, Choir Korg Aahhs, Endless Church Chime, D-50 Stack, Motif ES6 Piano |
| Orchestral | Box Harp Picked, Cinematic Strings, Slinky Violin Duet, Alto Sax, Mariachi Trumpet, Choir Korg Aahhs, Endless Church Chime, CFX Grand, Subfrost Harmonic Bow |
| Brass and Winds | Wurlitzer, Alto Sax, CS-20M Big Waves, Motif ES6 Piano, Worship Guitar, Laboriel Slap Bass, Dark Violins, Endless Church Chime, Mariachi Trumpet |
| Synthwave | CS-20M Big Waves, D-50 Stack, Sky Pad Evolving, Rhodes VS Extreme, Wurlitzer, Shiny Effects Pad, Motif ES6 Piano, Laboriel Slap Bass, Endless Church Chime |
| Retro Keys | Wurlitzer, Rhodes VS Extreme, CS-20M Big Waves, Motif ES6 Piano, Hammond B3 Slow, JF Super Saw, Laboriel Slap Bass, Shiny Effects Pad, Endless Church Chime |
| Intimate Pads | Wurlitzer, CFX Grand, Slinky Violin Duet, Dark Violins, Sky Pad Evolving, Subfrost Harmonic Bow, Box Harp Picked, Endless Church Chime, Choir Korg Aahhs |
| Praise Leads | CS-20M Big Waves, JF Gospel Organ, Sky Pad Evolving, D-50 Stack, JF Worship Piano, Cinematic Strings, Shiny Effects Pad, Choir Korg Aahhs, Endless Church Chime |
| Acoustic Worship | Wurlitzer, CFX Grand, Slinky Violin Duet, Dark Violins, Worship Guitar, JF Gospel Organ, Alto Sax, Box Harp Picked, Sky Pad Evolving |

O SMK-25, SMC-Mixer, SMC-PAD e SMC-PAD Pocket nao fazem parte dessas mudancas
de modo da Arturia. O comportamento MIDI, de audio, pads, transporte e mixer
existente continua ativo.

### Carla Headless

O live rack protegido e os projetos de genero podem rodar headless. Para instalar
o servico opcional do rack completo headless:

```bash
docs/tools/music-rig/packaging/linux/install-pedro-carla-headless
```

Feche primeiro a Carla GUI normal e depois inicie o backend headless:

```bash
systemctl --user start music-rig-pedro-carla-headless.service
```

Para conectar a GUI de controle opcional da Carla via OSC:

```bash
~/.local/bin/carla-pedro-osc-gui
```

Nao abra o mesmo projeto em uma segunda instancia normal da Carla. Pare o servico
headless antes de voltar para a Carla GUI:

```bash
systemctl --user stop music-rig-pedro-carla-headless.service
```
