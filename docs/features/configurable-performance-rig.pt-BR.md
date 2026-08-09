# Proposta de Funcionalidade: Rig de Performance Configurável

Idioma: [English](configurable-performance-rig.md) | **Português (Brasil)**

Implementação: [Plano de Implementação (English)](configurable-performance-rig-implementation-plan.md)

Status: proposta para revisão; este documento não implica implementação.

## Resumo

O sistema musical completo e transferível será chamado de **Rig de Performance
(Performance Rig)**, ou **Rig** quando o contexto estiver claro. Um Rig inclui
controladores, motores sonoros, efeitos, mapeamentos, rotas, assets, serviços,
configurações independentes de máquina e as regras necessárias para
materializá-los e operá-los.

O rack ao vivo atual de Pedro no Carla é uma configuração fixa desse Rig. Esta
funcionalidade o generaliza em dois níveis selecionáveis:

- Um **Perfil de Rig (Rig Profile)** é uma configuração global de todo o Rig.
  Ele seleciona os Perfis de Dispositivo participantes, módulos sonoros,
  processamento compartilhado, rotas e estado inicial.
- Um **Perfil de Dispositivo (Device Profile)** atribui a um slot de dispositivo
  uma função musical ou operacional específica. O SMC-Mixer pode ser um
  equalizador em um perfil e um mixer de tracks em outro, por exemplo.

Os dois níveis devem poder ser trocados por uma CLI. O projeto do runtime também
deve permitir que, futuramente, eventos MIDI CC, note ou program change invoquem
as mesmas operações de troca. A troca deve priorizar baixa latência previsível e
baixo consumo de recursos em repouso.

As mesmas definições de Rig e Perfis devem ser portáveis entre os sistemas
operacionais suportados. Comportamentos específicos de plataforma para áudio,
MIDI, grafo, host de plugins, IPC, filesystem e ciclo de vida dos serviços devem
ficar atrás de adaptadores, e não dentro do modelo de perfil musical.

## Por Que Isso É Necessário

O repositório agora preserva e transfere a configuração atual completa entre
máquinas, mas as funções estão incorporadas a um projeto monolítico do Carla, ao
grafo do PipeWire, aos serviços e ao manifesto `setup.json`. A instalação é
reproduzível, mas sua configuração musical é efetivamente estática.

Exemplos do acoplamento atual incluem:

- a Arturia é sempre um controlador de rack com nove instrumentos;
- o SMC-Mixer é sempre um equalizador de oito bandas;
- o SMC-PAD e o SMC-PAD Pocket sempre tocam o mesmo instrumento de bateria; e
- o SMK-25 sempre controla oito camadas de pads com latch.

Alterar uma função hoje significa editar e recapturar o rack inteiro. O modelo
desejado mantém a reprodutibilidade da configuração existente enquanto permite
selecionar deliberadamente a função de um dispositivo ou toda a configuração do
Rig.

## Terminologia

### Rig de Performance (Performance Rig)

O conceito durável para o sistema completo. O Rig não é um computador específico
nem um único projeto do Carla. Ele é a definição portátil a partir da qual uma
configuração funcional pode ser instalada e executada em uma máquina compatível.

Identificador sugerido para o primeiro Rig deste repositório:
`pedro-performance-rig`.

### Perfil de Rig (Rig Profile)

Uma composição nomeada, versionada e globalmente selecionável dentro de um Rig.
Um Perfil de Rig escolhe os Perfis de Dispositivo e o grafo de áudio/MIDI
compartilhado necessário para um determinado contexto de performance. A
configuração existente se tornaria inicialmente um Perfil de Rig como
`full-live-rack`, sem mudanças de comportamento.

"Perfil global" é uma forma abreviada aceitável na CLI e na interface com o
usuário para se referir a um Perfil de Rig.

### Slot de Dispositivo (Device Slot)

Uma posição lógica estável em um Rig, como `arturia-main`, `smc-mixer-main`,
`smc-pad-main` ou `smc-pad-pocket`. Um slot é diferente de um modelo de
dispositivo e de um nome transitório de porta ALSA ou PipeWire. Isso é importante
porque vários dispositivos M-VAVE compartilham um USB product ID e endpoints
SINCO semelhantes.

### Perfil de Dispositivo (Device Profile)

Um comportamento nomeado e versionado atribuído a um slot de dispositivo. Ele
define:

- modelos de dispositivo compatíveis e endpoints obrigatórios;
- a finalidade semântica de cada controle físico participante;
- mensagens MIDI aceitas e premissas sobre o modo do hardware;
- transformações, ações, parâmetros de destino e feedback;
- motores sonoros, efeitos, rotas ou lógica auxiliar sob sua responsabilidade;
- estado padrão e regras de persistência de estado;
- dependências de recursos e política de preparação; e
- comportamento de entrada, saída, falha e rollback durante a troca.

Um Perfil de Dispositivo descreve a função do dispositivo no Rig, não apenas as
mensagens MIDI brutas emitidas por seus controles.

### Preset de Hardware (Hardware Preset)

Configurações armazenadas dentro de um controlador ou aplicadas com o MIDI
Control Center ou o CubeSuite. Essas configurações determinam quais mensagens
brutas o hardware emite.

O arquivo existente `controller-profiles.md` usa "profile" para esse conceito.
Durante a implementação, ele deve ser renomeado ou reformulado como **Presets de
Hardware** para não ser confundido com Perfis de Dispositivo. Um Perfil de
Dispositivo referencia um Preset de Hardware compatível em vez de duplicá-lo.

### Gatilho de Troca (Switch Trigger)

Uma solicitação da CLI ou um evento MIDI que pede ao runtime do Rig para ativar
um Perfil de Rig ou um Perfil de Dispositivo. Toda origem de gatilho usa o mesmo
motor de validação e troca.

## Modelo Conceitual

```text
Rig de Performance
  +-- slots de dispositivo e regras de descoberta de dispositivos físicos
  +-- Perfis de Dispositivo disponíveis por slot
  +-- Perfis de Rig disponíveis
  |     +-- um Perfil de Dispositivo selecionado por slot participante
  |     +-- módulos, rotas e estado inicial compartilhados
  +-- Presets de Hardware
  +-- gatilhos de troca e política de segurança
  +-- requisitos de implantação e runtime
```

O Rig é o contêiner. Um Perfil de Rig altera a composição global. A troca de um
Perfil de Dispositivo altera apenas um slot e suas dependências próprias,
preservando o restante do Perfil de Rig ativo.

Apenas um Perfil de Dispositivo fica ativo por slot na primeira implementação.
No futuro, um perfil poderá ser composto por fragmentos reutilizáveis de
comportamento, mas esses fragmentos não são necessários para o projeto inicial.

## Exemplos de Perfis

Os nomes abaixo são identificadores ilustrativos, não nomes finais de presets.

### M-VAVE SMC-Mixer

#### `eight-band-eq`

Este é o comportamento atual. Os faders controlam as bandas de 63, 125, 250,
500, 1000, 2000, 4000 e 8000 Hz por meio de mapeamentos de ganho com limites. O
perfil é responsável pelas transformações de CC e pelos vínculos com os
parâmetros do equalizador.

#### `multilevel-volume`

A superfície controla vários níveis da mixagem ao vivo em vez de bandas de
frequência. Os bancos possíveis incluem instrumentos individuais, famílias de
instrumentos, submixes, retornos de efeitos, volume de monitor e volume master.
Os encoders podem controlar pan ou envios de efeitos, enquanto os botões
selecionam bancos ou grupos de mute.

#### `track-control`

O controlador usa sua função pretendida de superfície para DAW: os faders
controlam o volume das tracks, os encoders controlam pan, os botões controlam
mute/solo/record/select e a navegação muda os bancos de tracks. Esse perfil pode
usar Mackie Control em vez do Preset de Hardware atual baseado em CCs simples.

#### `effects-mixer`

Os faders controlam retornos de efeitos ou balanços wet/dry, os encoders
controlam níveis de envio ou parâmetros centrais dos efeitos, e os botões fazem
bypass ou selecionam grupos de efeitos. Isso é útil quando a mixagem principal
dos instrumentos pertence a outro dispositivo.

### M-VAVE SMC-PAD e SMC-PAD Pocket

As duas unidades ocupam slots independentes. Elas podem usar o mesmo perfil,
perfis diferentes, ou apenas uma delas pode participar de um Perfil de Rig.

#### `drum-set`

Este é o comportamento atual baseado em notas. Os pads tocam um SoundFont de
bateria; os knobs controlam o volume do instrumento e o ganho pós-instrumento
quando disponível.

#### `pad-layer-controller`

Os pads ativam tracks ou instrumentos de pads no estilo das camadas atuais do
SMK-25, adaptadas para uma superfície centrada em pads. Um pad pode usar
comportamento momentâneo, latch, toggle ou one-shot. Os bancos de encoders podem
controlar volume, filtro, reverb ou outros parâmetros das camadas.

Um exemplo é usar o SMC-PAD completo para 16 camadas tocáveis de forma
independente e o Pocket para sons de bateria. Outro é reservar a unidade
completa para bateria e usar o Pocket para alternar quatro ou oito camadas
ambientes.

#### `clip-and-scene-launcher`

Os pads disparam clips, seções, backing tracks, loops ou cenas de uma música.
Cores e aftertouch podem fornecer estado ou feedback expressivo quando o hardware
e o runtime oferecerem suporte. Os controles de transporte assumem as ações de
stop, play, record e tempo.

#### `melodic-grid`

Os pads formam um layout cromático, travado em uma escala, de acordes ou
isomórfico para um sintetizador ou sampler. Os bancos mudam a oitava, a escala ou
o instrumento.

#### `percussion-zones`

Grupos de pads controlam diferentes motores de percussão, como bateria acústica,
sons eletrônicos, percussão orquestral e efeitos one-shot. Os encoders controlam
o volume ou a forma sonora de cada grupo.

### Arturia KeyLab Essential 61 mk3

#### `multi-instrument-rack`

Este é o comportamento atual. As teclas tocam o rack, os faders controlam o
volume de nove instrumentos, os knobs controlam os envios de reverb e o
encoder/clique central controla o volume e o mute do rack.

#### `tonewheel-organ`

A Arturia se torna um controlador detalhado de órgão. Os nove faders funcionam
como drawbars; os knobs controlam parâmetros como percussion, key click,
leakage, chorus/vibrato, velocidade do rotary, drive ou balanço de microfones. Os
pads podem selecionar registrations ou alternar o estado do rotary. A direção
dos drawbars e os intervalos dos parâmetros devem ser declarados explicitamente,
pois drawbars de órgão usam uma metáfora física invertida.

#### `synth-programmer`

As teclas tocam um único sintetizador enquanto os faders e knobs moldam o som.
As atribuições podem abranger mixagem de osciladores, envelopes, cutoff e
resonance do filtro, profundidade e velocidade de modulação, unison, drive e
efeitos. Os pads podem escolher estados de osciladores, rotas de modulação,
sequências ou variações salvas.

#### `choir-designer`

As teclas tocam um motor de coral. Os controles moldam o balanço das seções,
vogal ou formant, attack/release, dynamics, expression, divisi, largura estéreo,
sala e reverb. Os pads podem selecionar articulações ou combinações de vozes.

#### `modeled-piano`

As teclas tocam um motor detalhado de piano. Knobs e faders expõem os parâmetros
suportados por esse motor, incluindo abertura da caixa, tensão ou afinação das
cordas, dureza dos martelos, resposta à velocidade, ressonância simpática, ruído
dos abafadores e do key-off, posição dos microfones, sala e reverb. O perfil deve
declarar compatibilidade específica com o motor, em vez de presumir que todo
plugin de piano fornece todos os parâmetros.

#### `split-orchestra`

O teclado é dividido em zonas para piano, cordas, metais, coral, baixo ou lead.
Os faders mixam as zonas, os knobs controlam expression ou efeitos, e os pads
selecionam articulações e variações.

#### `daw-and-looper-control`

A porta MIDI padrão continua disponível para as teclas, enquanto a porta
MCU/HUI e a superfície de transporte controlam gravação, seleção de tracks,
looping e reprodução. Esse perfil deve manter as mensagens de controle da DAW
isoladas dos mapeamentos de instrumentos.

### M-VAVE SMK-25

O perfil atual `ambient-pad-layers` pode ser acompanhado por alternativas como
`split-instrument`, `chord-and-arpeggio`, `clip-launcher` ou um
`instrument-parameter-editor` compacto. Esses exemplos também confirmam que
Perfis de Dispositivo são uma abstração compartilhada, e não casos especiais
para apenas três produtos.

## Requisitos da CLI

O nome proposto para o executável é `music-rig`. Ele evita acoplar a interface
ao Carla, ao PipeWire, a um hostname de referência ou ao nome do projeto atual
de uma pessoa.

A superfície mínima de comandos é:

```bash
# Inspecionar as definições e o estado do runtime.
music-rig status
music-rig status --json
music-rig profiles list
music-rig profiles list --device smc-mixer-main

# Trocar a composição completa do Rig.
music-rig switch --global full-live-rack

# Trocar apenas um slot de dispositivo.
music-rig switch \
  --device smc-mixer-main \
  --profile multilevel-volume

# Verificar ou preparar uma alteração sem efetivá-la.
music-rig switch --global modeled-piano --dry-run
music-rig prepare --global modeled-piano
music-rig validate
```

Comportamento da CLI:

- `--global` e `--device` são escopos de troca mutuamente exclusivos.
- Uma troca de dispositivo altera apenas o slot solicitado e os recursos que
  pertencem exclusivamente ao perfil anterior ou ao próximo.
- Uma troca global é transacional: todos os perfis que a compõem são ativados,
  ou nenhum deles é.
- `--dry-run` informa compatibilidade, alterações no grafo, assets ausentes,
  prontidão dos recursos e se seria necessário um carregamento a frio.
- `prepare` carrega ou valida dependências caras sem torná-las audíveis nem
  alterar a propriedade dos controles.
- Uma troca normal durante uma performance falha rapidamente se um perfil
  necessário estiver frio. Uma opção explícita fora do modo live pode permitir
  um carregamento frio bloqueante.
- Os comandos retornam somente depois que a nova geração for efetivada ou que a
  geração anterior tiver sido preservada. Saída legível por máquina e códigos de
  saída estáveis são obrigatórios para scripts e futuras interfaces de usuário.
- `status` identifica o Perfil de Rig ativo, overrides por dispositivo, a
  prontidão dos perfis, o uso de recursos e o resultado da última troca.

A CLI é um cliente fino. Ela deve se comunicar com um único serviço de usuário
residente por um socket Unix local ou IPC equivalente de baixo custo. Ela não
deve reconstruir um projeto, iniciar um pipeline de shell nem iniciar um novo
motor MIDI/áudio a cada troca.

## Gatilhos MIDI de Troca

A troca acionada por MIDI é uma capacidade planejada e deve orientar o primeiro
modelo de dados, mesmo que seja implementada depois da CLI.

Um gatilho associa um evento completamente qualificado à mesma operação exposta
pela CLI. Exemplo de intenção:

```json
{
  "source_slot": "arturia-main",
  "endpoint": "standard-midi",
  "event": { "type": "cc", "channel": 16, "number": 24 },
  "edge": "value-at-least",
  "threshold": 64,
  "consume": true,
  "action": {
    "type": "switch-device-profile",
    "device_slot": "smc-mixer-main",
    "profile": "multilevel-volume"
  }
}
```

Comportamento obrigatório dos gatilhos:

- O modelo oferece suporte a mensagens CC, note e program change.
- Endpoint, canal, tipo de evento, número e valor/edge são correspondidos
  explicitamente.
- Pares de pressionamento e liberação usam debounce para que uma pressão no pad
  não possa executar duas trocas.
- Um gatilho declara se seu evento é consumido ou se também chega ao perfil
  musical ativo.
- Gatilhos de gerenciamento ficam em uma camada de controle persistente do Rig,
  independente do Perfil de Dispositivo musical ativo. Trocar a função musical
  da Arturia não pode remover acidentalmente a única rota usada para voltar.
- Conflitos entre gatilhos e loops recursivos de troca são rejeitados durante a
  validação.
- No modo live, gatilhos MIDI só podem ativar perfis que tenham passado pela
  validação e atendam à sua política de prontidão. Um carregamento frio pesado
  não pode bloquear o caminho de processamento dos eventos MIDI.
- O processamento MIDI chama internamente o serviço de troca. Ele nunca inicia o
  executável da CLI para cada evento.

Exemplos de uso dos pads da Arturia incluem selecionar Perfis de Rig completos,
selecionar um perfil de registration de órgão, mudar apenas o SMC-Mixer de EQ
para controle de tracks ou alternar o SMC-PAD entre bateria e controle de
camadas. Feedback por LEDs ou display é opcional inicialmente, mas pertence ao
modelo de perfil.

## Arquitetura de Runtime

### Serviço de Controle Residente

Um único serviço por usuário mantém o estado dos perfis ativos, os mapeamentos
MIDI compilados, as transações de troca e a coordenação com adaptadores de
áudio/MIDI. Ele deve ser orientado a eventos e permanecer inativo quando não
houver comandos ou entrada MIDI.

O serviço separa um plano de controle não real-time dos caminhos de eventos de
áudio e MIDI. Parsing de JSON, descoberta de dependências, carregamento de
plugins, planejamento do grafo e trabalho no filesystem ocorrem antes do commit
e nunca dentro de um callback real-time.

### Geração Compilada de Perfis

Durante a instalação, validação ou preparação, os perfis declarativos são
resolvidos em mapeamentos imutáveis de runtime e em um delta do grafo. A ativação
troca uma geração previamente validada por outra, em vez de analisar novamente
os manifestos ou recapturar todo o grafo do PipeWire.

Alterações somente de controle devem se resumir a uma troca atômica da geração
de mapeamentos. Alterações no grafo devem aplicar o menor delta necessário e
preservar motores, efeitos e rotas compartilhados.

### Troca Transacional

Uma troca segue estas etapas:

1. Resolver o perfil global ou de dispositivo solicitado e suas dependências.
2. Validar compatibilidade, propriedade, prontidão e orçamento de recursos.
3. Preparar uma próxima geração imutável fora do processamento real-time.
4. Aplicar qualquer mute/rampa curta ou crossfade necessário.
5. Efetivar atomicamente os mapeamentos e o delta mínimo do grafo.
6. Publicar o estado e liberar recursos que não são mais necessários.

Qualquer falha antes do commit mantém a geração ativa intacta. Qualquer falha
durante o commit executa rollback para a geração completa anterior.

### Estado e Controles Físicos

O estado do perfil pertence a um endereço qualificado que contém o Rig, o slot
de dispositivo, o perfil e o parâmetro. Dois perfis não podem compartilhar
acidentalmente o estado de um CC bruto apenas porque usam o mesmo número.

Cada controle absoluto declara uma política de troca, como `jump`, `pickup`,
`scaled-pickup` ou `ignore-until-moved`. Padrões seguros devem impedir que a
posição antiga de um fader cause uma mudança repentina de volume ou parâmetro
após a troca. Encoders relativos declaram suas regras de codificação e
aceleração.

Os perfis também declaram se seus últimos valores persistem entre ativações, se
voltam aos padrões definidos pelo autor ou se seguem valores armazenados pelo
motor controlado.

## Requisitos de Desempenho e Recursos

Trocas rápidas e baixo consumo de recursos podem entrar em conflito quando um
novo perfil exige um SoundFont grande, uma biblioteca de sampler ou um plugin. O
projeto deve tornar essa escolha explícita, em vez de afirmar que todo perfil
frio pode ser trocado instantaneamente.

Os perfis usam uma destas classes de prontidão:

- **Somente controle (Control-only):** altera mapeamentos ou destinos reutilizando
  motores já em execução. Deve poder ser trocado imediatamente.
- **Preparado (Prepared):** exige motores ou assets carregados antecipadamente,
  mas silenciosos ou desconectados até o commit.
- **Frio (Cold):** exige o carregamento de plugin ou asset. É adequado para a
  preparação do sistema ou para uma troca explicitamente bloqueante, não para um
  gatilho MIDI inesperado durante uma performance.

Metas iniciais de desempenho na máquina de referência documentada `airstar`:

- troca somente de controle: p95 igual ou inferior a 20 ms desde o recebimento
  pelo serviço até o commit;
- troca preparada de áudio/perfil: p95 igual ou inferior a 100 ms, incluindo uma
  rampa curta ou crossfade sem clicks;
- gatilho de controle MIDI até o commit somente de controle: p95 igual ou
  inferior a 30 ms;
- CPU ociosa do serviço de troca: abaixo de 0,5% de um núcleo quando nenhum
  evento estiver chegando;
- memória residente do serviço de troca: abaixo de 50 MB, excluindo motores de
  plugins, assets de samples, Carla e PipeWire; e
- nenhum xrun de áudio atribuível à ativação de perfis no benchmark reproduzível
  de troca.

Estas são metas de aceitação que devem ser medidas e ajustadas, não afirmações
sobre a implementação atual. Elas podem se tornar mais rigorosas depois que uma
linha de base for medida.

Regras de recursos:

- Não usar polling quando notificações ALSA/PipeWire ou eventos de socket
  estiverem disponíveis.
- Não duplicar desnecessariamente instrumentos ou efeitos compartilhados entre
  perfis.
- Manter aquecidos apenas os perfis preparados explicitamente; oferecer um
  limite configurável de memória e remoção determinística dos perfis preparados
  que não estejam fixados.
- Carregar assets e instanciar plugins fora do caminho real-time.
- Manter em cache definições validadas e mapeamentos compilados, invalidando-os
  apenas quando os arquivos de origem ou suas dependências mudarem.
- Aplicar deltas do grafo em vez de reiniciar o Carla ou restaurar todo o grafo
  salvo para uma alteração em um único dispositivo.
- Reutilizar uma assinatura de entrada MIDI por endpoint físico e despachar por
  meio da geração imutável de mapeamentos ativa.
- Medir latência de ativação, xruns, CPU e memória em benchmarks automatizados.

A política de preparação faz parte de cada Perfil de Rig. Um perfil de
performance pode manter um órgão e um piano preparados para troca instantânea,
enquanto um perfil de prática com recursos limitados pode manter residente
apenas o motor ativo.

## Propriedade Declarativa e Validação

Todo perfil deve declarar aquilo que controla. Os objetos sob sua
responsabilidade podem incluir eventos MIDI, controles semânticos, parâmetros de
destino, processadores auxiliares, plugins, portas do grafo, rotas, chaves de
estado e saídas de feedback.

A validação rejeita:

- dois perfis ativos reivindicando o mesmo controle ou parâmetro exclusivo;
- descoberta ambígua de dispositivo ou ausência de um endpoint obrigatório;
- um perfil incompatível com o modelo associado ao slot ou com o Preset de
  Hardware;
- plugins, assets ou capacidades auxiliares ausentes e parâmetros sem suporte;
- colisões entre CCs privados transformados;
- gatilhos MIDI de gerenciamento ocultados por mapeamentos musicais;
- perfis globais que não possam ser efetivados como uma única geração completa;
  e
- um perfil exclusivo para uso live cujas dependências não possam atender à sua
  política de prontidão.

Os perfis devem apontar para parâmetros semânticos quando um adaptador puder
fornecê-los, por exemplo, `organ.drawbar.16ft` ou `piano.lid`, enquanto os
adaptadores resolvem esses nomes para portas específicas do plugin, MIDI CCs ou
parâmetros do Carla. MIDI bruto e índices de plugins continuam permitidos nos
limites dos adaptadores, mas não devem definir a composição de alto nível do
Rig.

## Requisitos de Portabilidade e Multiplataforma

Portabilidade é um requisito do produto, não apenas uma direção futura de
refatoração. A primeira implementação pode atingir paridade live no Linux antes
de outra plataforma, mas schema, runtime central, CLI, protocolo IPC, modelo de
estado e testes devem permanecer neutros em relação à plataforma desde sua
primeira versão.

Os alvos iniciais de suporte são:

- **Linux:** o primeiro backend validado, usando a implantação existente com
  PipeWire/JACK/Carla/systemd;
- **Windows:** o segundo backend obrigatório, usando adaptadores compatíveis com
  Windows para MIDI, áudio, IPC, ciclo de vida e host de plugins; e
- **macOS:** não é obrigatório para o critério inicial de conclusão, mas as
  interfaces dos adaptadores não podem impedir uma implementação posterior com
  CoreMIDI/CoreAudio.

O comportamento multiplataforma exige:

- uso dos mesmos documentos de Rig, Perfil de Rig, Perfil de Dispositivo,
  Preset de Hardware e Gatilho de Troca, sem cópias específicas por plataforma;
- bindings de plataforma que resolvam capacidades semânticas para portas,
  plugins, parâmetros, operações de grafo e locais do filesystem disponíveis;
- ausência de aliases PipeWire, caminhos Unix, unidades systemd, caminhos
  Windows ou identificadores de plugins de plataforma na composição musical de
  alto nível;
- os mesmos nomes de comandos, argumentos, semântica de estado, campos de
  resposta e códigos de erro estáveis do `music-rig` em toda plataforma
  suportada;
- formatos compilado e de estado versionados, inspecionáveis e transferíveis
  entre as plataformas suportadas;
- diagnóstico explícito de capacidades quando uma plataforma não puder atender
  aos requisitos de um perfil; e
- medições de desempenho específicas por plataforma usando as mesmas definições
  de benchmark, com limites registrados por máquina de referência quando
  necessário.

Um adaptador pode substituir um plugin ou mecanismo de grafo somente quando
atender às mesmas capacidades semânticas declaradas. Uma redução silenciosa de
comportamento é inválida. Pelo menos um Perfil de Rig completo deve passar por
instalação, validação, troca global, troca de dispositivo, restauração de estado
e recuperação no Linux e no Windows antes de o sistema ser considerado
multiplataforma.

## Estrutura Proposta do Repositório

O schema exato continua sendo uma decisão de implementação, mas esta separação é
um ponto de partida útil:

```text
src/performance-rigs/pedro-performance-rig/
  rig.json
  rig-profiles/
    full-live-rack.json
    modeled-piano.json
  device-profiles/
    arturia-main/
      multi-instrument-rack.json
      tonewheel-organ.json
      synth-programmer.json
    smc-mixer-main/
      eight-band-eq.json
      multilevel-volume.json
      track-control.json
    smc-pad-main/
      drum-set.json
      pad-layer-controller.json
    smc-pad-pocket/
      drum-set.json
      pad-layer-controller.json
  hardware-presets/
  platform-bindings/
    linux/
    windows/
  switch-triggers.json
```

A primeira extração pode referenciar o projeto atual do Carla, o manifesto da
configuração, o snapshot do Patchbay e os serviços existentes em vez de movê-los
ou reescrevê-los. Quando o runtime conseguir reproduzir o comportamento atual
por meio da nova composição, esses artefatos poderão ser divididos somente onde
os limites dos perfis exigirem.

## Requisitos Funcionais

1. Instalar e validar um Rig de Performance nomeado independentemente do
   hostname e da conta de usuário do sistema operacional, seguindo as mesmas
   regras de portabilidade da configuração atual.
2. Listar Perfis de Rig e Perfis de Dispositivo disponíveis e ativos.
3. Trocar o Rig completo com uma operação global da CLI.
4. Trocar um slot de dispositivo sem reinicializar slots não relacionados.
5. Preservar um override explícito por dispositivo no estado do runtime até que
   uma troca global o substitua ou que o usuário o redefina para o padrão do
   Perfil de Rig.
6. Simular, preparar, efetivar, relatar e reverter uma transação de troca.
7. Reproduzir o rack ao vivo completo atual como o primeiro Perfil de Rig, sem
   regressão musical ou de roteamento.
8. Permitir que o SMC-PAD e o SMC-PAD Pocket usem Perfis de Dispositivo
   diferentes ao mesmo tempo.
9. Representar gatilhos da CLI e MIDI como chamadas para a mesma operação de
   troca.
10. Preservar um caminho MIDI de gerenciamento independente dos mapeamentos
    musicais ativos.
11. Impedir saltos inseguros de parâmetros por meio de políticas declaradas de
    takeover.
12. Informar dependências frias antes da tentativa de uma troca sensível ao
    tempo.
13. Compilar as mesmas definições autorais no Linux e no Windows sem copiar ou
    bifurcar os documentos de perfis musicais.
14. Manter comandos da CLI, semântica das transações, semântica de estado e
    códigos de erro consistentes entre as plataformas suportadas.
15. Informar capacidades da plataforma e requisitos de perfil não resolvidos
    antes da instalação ou ativação.
16. Resolver caminhos do sistema operacional, APIs de dispositivos, operações
    de grafo, serviços e plugins por meio de bindings de plataforma.
17. Validar pelo menos um Perfil de Rig completo no Linux e no Windows.

## Fora do Escopo da Primeira Implementação

- Um editor gráfico de perfis.
- Inferência automática de um Perfil de Dispositivo útil a partir de tráfego
  MIDI bruto.
- Carregamento frio transparente de bibliotecas de samples arbitrariamente
  grandes.
- Troca de perfis diretamente dentro de um callback real-time.
- Lançamento simultâneo de adaptadores para todos os sistemas operacionais. O
  Linux é o primeiro backend live, o Windows é o segundo backend obrigatório, e
  outras plataformas podem seguir o mesmo contrato de adaptadores.
- Substituição do Carla, do PipeWire ou da implantação atual verificada antes de
  demonstrar paridade de funcionalidades.
- Subperfis combináveis ou vários perfis simultâneos em um único slot.

## Plano de Entrega

### Fase 1: Preservar o Comportamento Atual

- Introduzir schemas para Rig, slot, Perfil de Rig, Perfil de Dispositivo e
  Preset de Hardware.
- Descrever a configuração existente como
  `pedro-performance-rig/full-live-rack`.
- Extrair um Perfil de Dispositivo atual para cada slot de controlador.
- Compilar ou materializar essas definições nos artefatos existentes.
- Comprovar paridade com as verificações atuais de plugins, conexões, serviços,
  assets e validação live.

### Fase 2: CLI e Serviço de Runtime

- Adicionar o serviço de troca residente e o cliente `music-rig`.
- Implementar status, listagem, validação, dry-run, preparação e troca
  transacional.
- Começar com a troca de Perfis de Dispositivo somente de controle e medi-la.
- Adicionar propriedade de deltas do grafo e overrides por dispositivo.

### Fase 3: Primeiros Perfis Alternativos

- Implementar uma alternativa de baixo risco para o SMC-Mixer, provavelmente
  `multilevel-volume`.
- Implementar atribuições independentes de `drum-set` e
  `pad-layer-controller` para o SMC-PAD e o SMC-PAD Pocket.
- Adicionar um perfil focado em sound design para a Arturia depois de selecionar
  um plugin cujos parâmetros possam ser endereçados de forma confiável.

### Fase 4: Gatilhos MIDI

- Adicionar mapeamentos persistentes de gerenciamento para os pads da Arturia ou
  outros controles.
- Encaminhar eventos CC/note/program diretamente para o serviço de troca.
- Adicionar debounce, validação de conflitos, aplicação da política de prontidão
  e feedback.
- Medir a latência de ponta a ponta dos gatilhos e as trocas repetidas sob carga
  de áudio.

### Fase 5: Troca de Áudio Preparada

- Adicionar preparação em background e cache limitado de recursos aquecidos.
- Adicionar rampas/crossfades sem clicks e coordenação do ciclo de vida dos
  motores.
- Implementar e medir trocas globais entre Perfis de Rig materialmente
  diferentes, como rack completo, órgão, synth, coral e piano modelado.

### Fase 6: Entrega da Segunda Plataforma

- Compilar o core portátil e a CLI no Windows usando os mesmos schemas e
  contratos compilados.
- Implementar adaptadores Windows para IPC, caminhos/estado, descoberta de
  dispositivos, MIDI, áudio/controle de grafo, host de plugins e ciclo de vida.
- Adicionar bindings de plataforma para um Perfil de Rig completo.
- Executar as mesmas suítes de troca, recuperação, estado e desempenho nas
  máquinas de referência Linux e Windows.

## Critérios de Aceitação

- O Perfil de Rig `full-live-rack` é validado em relação à configuração atual
  versionada e a reproduz.
- Uma troca global pela CLI efetiva todos os perfis selecionados ou não altera
  nada.
- Uma troca de dispositivo pela CLI mantém inalterados os mapeamentos e rotas de
  áudio de dispositivos não relacionados.
- Os dois controladores de pads podem executar Perfis de Dispositivo diferentes
  ao mesmo tempo.
- Solicitações da CLI e MIDI produzem o mesmo estado de destino validado.
- Um gatilho de gerenciamento continua utilizável depois de mudar o Perfil de
  Dispositivo musical da Arturia.
- Um perfil frio é informado ou preparado sem bloquear o processamento de
  MIDI/áudio.
- Latência de troca, CPU ociosa, memória residente e xruns são medidos em relação
  às metas deste documento.
- Testes de desconexão, falha de dependência e erro no meio da troca preservam ou
  restauram a geração válida anterior.
- A instalação e a validação existentes para transferência entre máquinas
  continuam funcionando.
- Um Perfil de Rig completo é instalado e passa por troca global, troca de
  dispositivo, restauração de estado, validação e rollback no Linux e Windows.
- Os mesmos documentos de perfis musicais e o mesmo contrato da CLI são usados
  nas duas plataformas; apenas bindings e adaptadores de plataforma diferem.
- Capacidades sem suporte falham explicitamente antes da ativação.

## Decisões a Revisar Antes da Implementação

1. Confirmar **Rig de Performance (Performance Rig)** como o nome da
   configuração completa.
2. Confirmar **Perfil de Rig (Rig Profile)** para um perfil global e **Perfil de
   Dispositivo (Device Profile)** para a função de um dispositivo.
3. Confirmar **Preset de Hardware (Hardware Preset)** como substituto do termo
   "controller profile" usado na documentação atual de configurações internas
   do hardware.
4. Confirmar `music-rig` como nome da CLI e `pedro-performance-rig` como
   identificador do primeiro Rig.
5. Selecionar o primeiro Perfil de Dispositivo alternativo a ser implementado e
   medido.
6. Decidir quais mensagens dos pads da Arturia e qual canal MIDI serão
   reservados para gatilhos persistentes de gerenciamento.
7. Decidir o limite aceitável de memória para perfis preparados na máquina de
   referência e quais perfis devem ser fixados para troca live instantânea.
8. Selecionar a máquina Windows de referência e as capacidades mínimas dos
   adaptadores Windows de áudio, MIDI, host de plugins e ciclo de vida.
