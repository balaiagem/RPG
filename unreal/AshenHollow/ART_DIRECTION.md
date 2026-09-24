# Direcao de arte — Ashen Hollow

Decisao de 23/09/2026: **fantasia estilizada**. Silhuetas legiveis, cores saturadas,
pintura a mao. Nao e o caminho do realismo de BG3; e o caminho que um notebook com
RTX 3050 de 6 GB roda e que pacotes gratuitos conseguem sustentar sem parecer remendo.

## A regra que decide tudo: o esqueleto

As nove animacoes autorais (`AH_SwordSlash`, `AH_AxeCleave`, `AH_MaceStrike`,
`AH_StaffStrike`, `AH_Cast`, `AH_Heal`, `AH_Rage`, `AH_Guard`, `AH_Evade`), mais o blend
space de locomocao, a morte e a reacao a dano, foram feitas sobre `SKM_Manny_Simple` —
o esqueleto do Manequim UE5.

**Qualquer personagem novo precisa usar o esqueleto do Manequim UE5 ou passar por um IK
Retargeter.** Um pacote com esqueleto proprio quebra as treze animacoes de uma vez e
devolve o jogo ao T-pose. Antes de reivindicar qualquer personagem no Fab, confira na
pagina do anuncio se ele declara compatibilidade com o esqueleto do UE5 Mannequin.

Armas, cenario, props e VFX nao tem essa restricao — sao malhas estaticas.

## Ordem de trabalho

1. **Iluminacao e grade** — feito. `Scripts/Light-Courtyard.ps1`. Nenhum asset necessario.
2. **Armas** — trocar as primitivas procedurais de `AHEquipmentComponent` por malhas reais.
   Precisa de um pacote de armas estilizadas.
3. **Cenario** — substituir os cubos do patio por um kit modular estilizado.
4. **Personagens** — por ultimo, e so com esqueleto compativel.

## O que verificar em cada anuncio do Fab

- Licenca: confirme que e gratuito de fato e nao "gratis por tempo limitado" ja expirado.
- Versao do motor: precisa cobrir 5.8.
- Personagens: esqueleto do Manequim UE5, explicitamente.
- Densidade de textura: misturar 512px com 4K na mesma cena e o que mais denuncia
  remendo. Prefira um pacote grande a tres pequenos.
- Contagem de triangulos: o patio inteiro deve caber no orcamento de um 3050 de 6 GB.

## Candidatos vistos no canal gratuito do Fab (23/09/2026)

Nomes observados na listagem; **nenhum foi verificado quanto a esqueleto ou licenca** —
a pagina de cada anuncio e bloqueada para leitura automatica.

| Uso | Candidatos |
|---|---|
| Armas | Stylized Newbie Weapons Pack; Free Melee Weapon Pack |
| Cenario | FANTASTIC - Village Pack; Free Pack - Rocks Stylized; Cartoon City Free |
| Rochas / natureza | Rock Environment Pack; Environment - Rock Collection 04 |
| Personagem | Paladin RPG Set; Quantum Modular Character Free Sample |

Para personagens, o conteudo gratuito publicado pela propria Epic tende a ser a aposta
mais segura, porque e autorado para o esqueleto do UE5.

## Iluminacao aplicada

`Scripts/Light-Courtyard.py` e idempotente: encontra os atores pelo rotulo e atualiza,
entao pode ser rodado varias vezes enquanto se ajusta. Cada escrita de propriedade passa
por `setp()`, que registra e segue adiante quando um nome nao existe — nomes de
propriedade mudam entre versoes do motor e uma falha nao deve abandonar o nivel pela
metade. O relatorio sai em `Saved/Lighting-console.log`.

- Sol quente a 5200 K, angulo baixo (-32 graus): o angulo original achatava os pilares.
- Luz de ceu fria como preenchimento. O contraste quente/frio e a maior parte do que faz
  arte estilizada parecer deliberada em vez de chapada.
- Neblina exponencial com fog volumetrico e inscattering frio, para profundidade.
- Volume de pos-processamento sem limites: exposicao **travada** (auto exposicao e o
  principal motivo de uma cena estilizada parecer lavada ao girar a camera), bloom
  discreto, vinheta, tonalizacao dividida (realces quentes, sombras frias), curva
  filmica com mais ombro para o dourado nao estourar, e oclusao de ambiente.

F6 agora alterna Lumen e Virtual Shadow Maps junto com o perfil de Qualidade, entao da
para medir o custo real nesta maquina em vez de adivinhar. `DefaultEngine.ini` continua
com ambos desligados por padrao.

O fog volumetrico e o item mais caro do conjunto. Ele acompanha `sg.EffectsQuality`,
entao o perfil Desempenho ja o reduz.

## Armas: malhas reais com retorno seguro (2026-09-24)

`AHEquipmentComponent` agora consulta uma **tabela de arte**, uma linha por
`EAHWeaponKind`, no topo de `AHEquipmentComponent.cpp`. Cada linha tem o caminho de
uma malha, deslocamento, rotacao, escala e `TipHeight`, mais os mesmos campos para o
escudo (usado so por espada e maca).

Com o caminho vazio — o estado atual — a arma e construida com cubos e cilindros
exatamente como antes. Um caminho vazio, errado ou de um asset renomeado nunca quebra
a compilacao nem deixa o personagem de maos vazias: `LoadArt` devolve nulo em silencio
e o construtor primitivo assume. Por isso a troca de cada arma e uma string, e pode ser
feita uma de cada vez, testando entre elas.

`TipHeight` so move onde nascem os efeitos de magia e de impacto. **Nenhuma regra de
combate le a forma da arma** — alcance, dano e acerto vem todos de `FAHClassSheet`. Uma
arma feia e uma arma errada tem exatamente o mesmo comportamento em jogo.

Materiais: quando a malha vem de um pacote, o componente nao sobrescreve o material do
slot 0 — o asset mantem o que veio com ele. Os quatro materiais do projeto
(`M_WeaponSteel`, `M_WeaponGold`, `M_WeaponLeather`, `M_CombatGlow`) continuam servindo
so ao caminho primitivo.

Pacotes escolhidos: Melee Weapon Pack, Newbie Weapon Pack (armas) e Fantastic Village
(cenario). Decisao de 2026-09-24: a arena sera **montada com pecas da vila**, mantendo
os 18x18 m que a navegacao e os spawns esperam, em vez de jogar no mapa de demonstracao
do pacote.

### Orientacao medida, nao adivinhada (2026-09-24)

Primeira tentativa com malhas reais: a espada saiu deitada na horizontal e o cajado
atravessado na diagonal. Pacotes discordam sobre em que eixo a arma corre, e cada
palpite custa um build inteiro para descobrir.

Em vez de adivinhar, `UprightRotation()` **mede a malha**. Dois arranjos cobrem
praticamente tudo: cabo na origem do asset, onde o centro do bounding box fica ao
longo do comprimento da arma e o sinal diz para que lado e o topo; e malha centrada
em si mesma, onde o deslocamento nao diz nada e o lado mais longo da caixa e o
comprimento. Uma arma que ja nasce vertical se mede como vertical e recebe rotacao
zero, entao deixar ligado e seguro.

O escudo nunca passa por isso: e um disco, o eixo mais longo dele e o diametro, e
endireitar esse eixo deixaria o escudo de perfil.

`bAutoUpright=false` na linha da tabela volta a usar a rotacao escrita a mao, para o
caso de um asset esquisito.

## A arena: Praca da Vila (2026-09-24)

`Scripts/Build-Arena.py` + `.ps1` constroem `Content/AshenHollow/Maps/ArenaVillage`.

**Courtyard nao e tocado.** O script duplica o mapa cinza uma vez, esvazia a copia e
constroi dentro dela. Se a arena nova sair errada, abrir Courtyard devolve o jogo
exatamente como estava, sem conserto nenhum. O `.ps1` so troca o mapa padrao em
`DefaultEngine.ini` **depois** que a construcao deu certo, e imprime como desfazer.

O quadrado jogavel continua com 18x18 m e os mesmos pontos de nascimento, porque a
navegacao, o alcance de 9 m de movimento e o `AHGameMode` foram equilibrados contra
esse tamanho. Tudo que e cenario fica fora dele: seis casas, cerca em volta, quatro
braseiros nos cantos com luz quente, poco, carrocas, barraca de feira, arvores.
Dentro do quadrado ficam so dois aglomerados de obstaculo (carroca e barris de um
lado, fardos e caixotes do outro), de proposito fora da linha reta entre os dois
nascimentos: a aproximacao inicial nunca fica bloqueada, mas o espaco passa a pedir
uma escolha de caminho.

### O principio: medir, nao adivinhar

Nada aqui fixa tamanho de asset. Cada peca e criada, perguntada pelo seu bounding
box e so entao posicionada:

- as casas sao empurradas para fora ate a parede de dentro de cada uma cair na mesma
  linha, qualquer que seja a largura dela;
- a cerca e ladrilhada medindo o comprimento de uma peca e girando conforme o eixo
  longo dela;
- o volume de navegacao e criado, medido e so entao escalado para 18x18 m, porque o
  tamanho do brush padrao e detalhe de versao do motor;
- os planos de calcamento sao escalados para 3 m exatos a partir do tamanho medido.

Adivinhar dimensao de pacote de arte custa uma rodada inteira de editor para
descobrir que errou, e o chute nunca acerta de primeira.

### Chao em tres camadas

Uma laje solida de 54x54 m carrega colisao e portanto a navegacao — superficie unica,
sem emenda de ladrilho para o Recast tropecar. Por cima, duas grades de planos que
sao so pintura: 3 m com pedra na praca, 6 m com material de chao no entorno. Cada
plano recebe a UV inteira do material, entao a textura repete num tamanho crivel em
vez de esticar 54 m num cubo so. O volume de navegacao cobre apenas os 18x18 m
centrais, entao o chao grande nao muda em nada onde da para andar.

Blueprints com efeito (braseiro, fogueira) **nao** sao assentados pela medida: o
bounding box de um sistema de particulas passa longe do modelo e enterraria o
braseiro. Esses confiam no pivo do pacote; so malhas estaticas simples sao assentadas.

Sem verticalidade por enquanto: nao temos regra de altura nem de cobertura, e rampa
mal ligada quebra navegacao. Quando as condicoes entrarem, ai vale.

### Por que a primeira construcao saiu vazia

`spawn_actor_from_object` passa pelas *actor factories* do editor, e um commandlet
headless (`-run=pythonscript`) nunca popula essa lista. Resultado: a funcao devolve
`None` para **todo** asset — inclusive o cubo do proprio motor — sem lancar excecao e
sem escrever erro nenhum. O nivel sai vazio e o log fecha com "Success, 0 errors".

`spawn_actor_from_class` nao passa por la e funciona igual nos dois modos. Entao o
`place()` cria a classe (`StaticMeshActor`, ou a classe gerada do Blueprint) e so
depois atribui a malha. Foi tambem o que explicou por que o script de iluminacao
"funcionou": ele so editava atores que ja existiam e criava volumes por classe.

Duas travas para isso nao se repetir em silencio:

- os marcadores de sucesso saem em **nivel de aviso**. O `unreal.log` comum entra
  como Display, e a captura de console que o `.ps1` le descarta Display — um
  marcador de sucesso que ninguem consegue ler nao serve para nada;
- o `.ps1` le a contagem de atores do marcador e **se recusa** a trocar o mapa
  padrao se vier abaixo de 100. Um mapa que "construiu" mas esta vazio nao tem chao,
  e entrar nele derruba o personagem pelo vazio.

### O mapa estava fixo no atalho

Depois de a arena existir e o `DefaultEngine.ini` apontar para ela, o jogo continuava
abrindo o Courtyard. O motivo nao estava em nenhum dos dois: `Play-Unreal.cmd`, na
raiz do repositorio, passava `/Game/AshenHollow/Maps/Courtyard` **na linha de comando**,
e argumento de linha de comando vence `GameDefaultMap`.

Esse e o atalho que o Lucas usa desde o primeiro dia — nao o botao Play do editor.
Vale conferir qual caminho a pessoa realmente usa antes de explicar por que o
resultado nao mudou.

O `.cmd` agora **le** `GameDefaultMap` do `DefaultEngine.ini` e deriva dali o caminho
do `.umap` para conferir se existe. Uma fonte da verdade so: trocar o mapa do projeto
passa a bastar, e nao ha um segundo lugar para esquecer de atualizar.

## Projeteis: cada ataque com a sua cara (2026-09-24)

Antes, **todo** ataque a distancia desenhava as mesmas tres esferas arcanas: flecha,
Raio de Fogo, Misseis Magicos e Raio Guiador eram a mesma imagem. `AAHMagicVisual`
agora tem uma tabela de estilo, uma linha por `EAHProjectileLook`, com cor, curvatura
da trajetoria, tamanho da cabeca e comprimento do rastro.

- **Flecha**: haste, ponta e empenacao de verdade, montadas apontando para o +X local
  do ator — assim mirar e so `SetActorRotation(direcao.Rotation())` por quadro. Voa
  quase reta, com uma queda leve; flecha que faz laco de missil magico para de parecer
  flecha. Nenhum dos pacotes tem malha de flecha, entao ela e feita de primitivas,
  como as armas procedurais eram.
- **Fogo / Gelo / Arcano / Radiante / Necrotico**: continuam esferas com rastro, mas
  tingidas. O material antigo (`M_ArcaneDart`) nao expoe parametro nenhum, entao nao
  dava para colorir; `M_CombatGlow` expoe `Tint`. Foi por isso que a troca de material
  aconteceu — conferir os parametros do material antes de escrever o codigo evitou uma
  rodada inteira de build.

A escolha vem **do ataque, nao da classe**. O Raio de Fogo de nivel de classe do mago e
a flecha do patrulheiro chegam os dois sem id de magia; so a arma na ficha os separa.

O arco tambem dispara de verdade: `SK_Bow_Newbie_01` usa o esqueleto `SKEL_Weapon_Bow`,
que vem com `A_Bow_Attack`. A tabela de arte ganhou um campo de clipe de disparo, e
`UAHEquipmentComponent::PlayShot()` toca isso no componente da propria arma quando a
flecha sai. Arma sem clipe simplesmente nao faz nada.

### O que ainda falta

O **corpo** do personagem nao tem pose de arquearia. Nenhuma animacao de arco no
projeto esta no esqueleto `SKM_Manny_Simple` — as do pacote animam o arco, nao quem
segura. Atirar continua tocando `AH_Cast`. Resolver isso pede uma animacao de arco
para o mannequin do UE5, de fora do que temos hoje.

O arco tambem fica na mao direita, porque e onde esta o ponto de encaixe. Arqueiro
segura na esquerda; junto com a animacao do corpo, e a mesma correcao.
