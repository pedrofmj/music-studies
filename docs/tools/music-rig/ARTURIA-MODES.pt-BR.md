# Modos da Arturia

Este guia descreve os modos da Arturia KeyLab Essential 61 mk3 usados com o
Pedro Performance Rig. A referencia completa do Performance Rig esta em
`src/performance-rigs/pedro-performance-rig/README.md`.

O seletor de pads da Arturia e um servico da sessao de usuario Linux. Ele nao
precisa do editor Windows da Arturia.

## Iniciar E Parar

O servico do seletor esta instalado em `airstar`, mas nao e ativado no login.

```bash
systemctl --user start music-rig-arturia-profile-session.service
systemctl --user stop music-rig-arturia-profile-session.service
```

Parar o servico restaura `full-live-rack` e remove as conexoes temporarias dos
modos candidatos.

## Modos Dos Pads

O gerenciamento dos pads considera somente o canal MIDI 10. As notas normais do
teclado usam o canal 1 e nao sao interpretadas como mudancas de modo.

| Pad | Modo |
| --- | --- |
| Bank A Pad 1 | Tonewheel Organ |
| Bank A Pad 2 | Synth Programmer |
| Bank A Pad 3 | Full Live Rack |
| Bank A Pad 4 | Worship Piano |
| Bank A Pad 5 | Gospel Keys |
| Bank A Pad 6 | Ambient Worship |
| Bank A Pad 7 | Jazz Keys |
| Bank A Pad 8 | Soul and R&B |
| Bank B Pad 1 | Cinematic Strings |
| Bank B Pad 2 | Orchestral |
| Bank B Pad 3 | Brass and Winds |
| Bank B Pad 4 | Synthwave |
| Bank B Pad 5 | Retro Keys |
| Bank B Pad 6 | Intimate Pads |
| Bank B Pad 7 | Praise Leads |
| Bank B Pad 8 | Acoustic Worship |

## Full Live Rack

Este e o modo protegido, padrao e de recuperacao.

| Fader | Volume do instrumento | Knob | Reverb do instrumento |
| --- | --- | --- | --- |
| 1 | Basic Piano | 1 | Basic Piano |
| 2 | Nord White Grand | 2 | Nord White Grand |
| 3 | Alt Strings | 3 | Alt Strings |
| 4 | Good Flute | 4 | Good Flute |
| 5 | SAX Lirakeys | 5 | SAX Lirakeys |
| 6 | Hammond Organ Fast | 6 | Hammond Organ Fast |
| 7 | Optik Synth | 7 | Optik Synth |
| 8 | PAD EFEITOS | 8 | PAD EFEITOS |
| 9 | AtmosferaPAD | 9 | AtmosferaPAD |

## Synth Programmer

Os faders usam comportamento de captura: o valor muda depois que o controle
fisico ultrapassa o valor armazenado.

| Controle | Parametro |
| --- | --- |
| Fader 1 | Mix do oscilador 1, `DCO1_BALANCE` |
| Fader 2 | Nivel/equilibrio do oscilador 2, `DCO2_BALANCE` |
| Fader 3 | Corte do filtro, `DCF1_CUTOFF` |
| Fader 4 | Ressonancia do filtro, `DCF1_RESO` |
| Fader 5 | Ataque do amplificador, `DCA1_ATTACK` |
| Fader 6 | Release do amplificador, `DCA1_RELEASE` |
| Fader 7 | Profundidade da modulacao, `LFO1_BALANCE` |
| Fader 8 | Velocidade da modulacao, `LFO1_RATE` |
| Fader 9 | Unisono/detune |
| Knob 1 | Drive/compressao |
| Knob 2 | Envio/mix de efeitos |
| Knob 3 | Tempo do delay |
| Knob 4 | Feedback do delay |
| Knob 5 | Tamanho do reverb |
| Knob 6 | Mix do reverb |
| Knob 7 | Modulacao de pitch |
| Knob 8 | Volume do synth, `OUT1_VOLUME` |
| Knob 9 | Largura do reverb do synth |

## Tonewheel Organ

O orgao usa setBfree. Os drawbars sao invertidos como em um orgao fisico: para
baixo e mais alto e para cima e desligado.

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

## Modos De Genero Materializados

Os pads 4 a 16 carregam projetos Carla headless separados. Cada projeto tem
nove patches reais de SoundFont ou DecentSampler da biblioteca maior. Eles nao
sao os nove instrumentos do live rack. Os caminhos de origem estao registrados
em `benchmarks/arturia-genre-patches.json`.

Em todos os modos de genero, os Faders 1-9 controlam o volume dos nove patches
abaixo e os Knobs 1-9 controlam o reverb correspondente. O encoder central
continua sendo o volume Master e o clique continua sendo o mute Master.

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

SMK-25, SMC-Mixer, SMC-PAD e SMC-PAD Pocket nao fazem parte dessas mudancas de
modo da Arturia. O comportamento MIDI, de audio, pads, transporte e mixer
existente continua ativo.

## Carla Headless

O rack completo protegido tambem pode rodar como um servico de usuario Carla
headless opcional. Feche primeiro a Carla GUI normal.

```bash
docs/tools/music-rig/packaging/linux/install-pedro-carla-headless
systemctl --user start music-rig-pedro-carla-headless.service
~/.local/bin/carla-pedro-osc-gui
```

A GUI OSC se conecta ao backend em execucao; ela nao inicia uma segunda engine
Carla. Pare o backend antes de reabrir a Carla GUI normal:

```bash
systemctl --user stop music-rig-pedro-carla-headless.service
```

## Seguranca

- `full-live-rack` continua sendo o modo padrao e de rollback.
- As engines candidatas pertencem ao usuario e nao sao instaladas no sistema pelo
  instalador da sessao.
- O servico e iniciado manualmente e pode ser parado imediatamente.
- O projeto Carla protegido e os servicos live atuais nao sao substituidos por
  um modo de pad.
