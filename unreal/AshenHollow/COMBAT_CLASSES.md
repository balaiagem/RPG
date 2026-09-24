# Classes jogáveis no duelo Unreal

A seleção aparece antes da iniciativa e confirma uma ficha predefinida de nível 1. F5 volta à seleção. Esta etapa implementa quatro arquétipos; os dados das outras oito classes permanecem preservados, mas ainda não são opções jogáveis no Unreal.

| Classe | PV / CA | Ataque básico | Habilidade E |
|---|---|---|---|
| Guerreiro | 12 / 16 | +5, 1d8+3 | Segundo Fôlego: bônus, cura 1d10+1, uma vez por encontro |
| Bárbaro | 14 / 14 | +5, 1d12+3 | Fúria: bônus, duas utilizações; +2 ao dano físico corpo a corpo e resistência física |
| Clérigo | 10 / 18 | +4, 1d6+2 | Curar Ferimentos: ação, cura própria 1d8+3, dois espaços por encontro |
| Mago | 8 / 12 | +2, 1d6 | Mísseis Mágicos: ação, três dardos de força de 1d4+1, dois espaços por encontro |

Fúria dura no máximo dez turnos próprios e termina ao encerrar um turno sem atacar nem ter recebido dano desde o fim do anterior. Resistência física arredonda o dano para baixo; dano de força a ignora. Os mísseis escolhem o inimigo visível mais próximo a até 18 m e resolvem no marcador da animação, sem rolagem de ataque. A rolagem de dano dos dardos é compartilhada. Cura de clérigo é limitada ao próprio personagem nesta etapa.

As fichas são arquétipos curados, não um criador completo de D&D: faltam edição de atributos, ancestralidade, subclasses, lista de magias, concentração e descanso. Os corpos continuam sendo manequins. As armas são modelos estilizados montados com malhas básicas, e os gestos próprios são animações por chaves, sem captura de movimento.

## Animação e desempenho

`UAHAnimInstance` avalia diretamente o blend space de parado/caminhada/corrida com a velocidade horizontal real. O slot de combate fica sobre essa base. A suíte de regressão verifica mudança real da pose do pé, além de custos, resistência e resolução única de magias.

F6 alterna configurações em memória, sem salvar preferências no disco:

| Perfil | Escalabilidade | Resolução interna | Limite de FPS |
|---|---|---|---|
| Desempenho | Média | 85% | 120 |
| Equilibrado (inicial) | Alta | 100% | 60 |
| Qualidade | Épica | 100% | 60 |

Os limites são alvos, não medições nem garantias. Lumen e Virtual Shadow Maps permanecem desligados na configuração atual. O desempenho deve ser reavaliado quando os personagens e cenários finais substituírem os placeholders.

## Legibilidade do combate

A seleção e a barra de habilidades compartilham símbolos próprios: coração para Segundo Fôlego, máscara para Fúria, cruz para cura e três dardos para Mísseis Mágicos. O comando E e os custos permanecem os mesmos.

O d20 mantém sua rotação e aparência, mas calcula a conectividade das vinte faces apenas uma vez. A ordenação por profundidade usa armazenamento local, evitando a alocação dessa lista a cada quadro. Esta é uma redução de trabalho da HUD, sem alegação de ganho de FPS medido.

Validação em 13/09/2026: compilação Development Editor aprovada no UE 5.8.2; três suítes aprovadas, zero falhas, avisos ou testes pendentes. Relatório: `Saved/Automation/20260913-180653/index.json`.

## Mísseis em 3D

Mísseis Mágicos agora cria três dardos emissivos com trajetórias curvas e seis amostras de rastro por dardo. A origem é a mão direita; o destino acompanha o alvo. O tempo de voo vem do marcador de impacto da animação. O combate remove o efeito no contato ou na morte do conjurador; o efeito também expira sozinho e desaparece se perder o alvo.

`AAHMagicVisual` é cosmético: não calcula dano, não tem colisão, não lança sombras nem cria luzes dinâmicas. Usa 21 componentes de malha durante o voo. O material próprio fica em `/Game/AshenHollow/FX/M_ArcaneDart`; `Scripts/Prepare-MagicVisual.py` pode criá-lo sem substituir um material existente. A conjuração agora usa `AH_Cast`, com levantamento dos braços e liberação em 0,8 segundo.

Validação anterior dos dardos: relatório `Saved/Automation/20260913-181222/index.json`, sem falhas, com um aviso de contexto no mundo isolado da suíte. O teste agora registra esse mundo no motor antes de destruir efeitos.

## Conjunto de combate das quatro classes

| Classe / ação | Equipamento / animação |
|---|---|
| Guerreiro | Espada e escudo; `AH_SwordSlash`, contato em 0,48 s |
| Bárbaro | Machado; `AH_AxeCleave`, contato em 0,70 s |
| Clérigo | Maça e escudo; `AH_MaceStrike`, contato em 0,55 s |
| Mago | Cajado; `AH_StaffStrike`, contato em 0,55 s; `AH_Cast` para magia |
| Cura / Segundo Fôlego | `AH_Heal`, partículas verdes ascendentes |
| Fúria | `AH_Rage`, partículas vermelhas ascendentes |
| Defesa / erro recebido | `AH_Guard` ou `AH_Evade`, efeito azul |
| Acerto / morte | Reação e queda existentes; faíscas físicas ou de força, inclusive em golpes letais |
| Arcana | Gesto `AH_Cast` e pulso arcano |
| Disparada | Corrida do blend space a até 650 cm/s, pulso na ativação; mantém o orçamento em metros |

`Scripts/Prepare-CombatSet.py` gera nove clipes e quatro materiais próprios sem substituir assets existentes. As armas acompanham os ossos das mãos; não interferem em colisão ou navegação. Os gestos de ação bloqueiam novos comandos até terminar. `AAHCombatBurst` usa 16 instâncias de uma malha por efeito, com duração limitada e sem luzes ou sombras. O sistema Niagara opcional de impacto continua disponível.

## Integração das alterações do HUD e salvaguardas

As fontes Cinzel fornecidas no projeto têm fallback de carregamento em runtime e são incluídas no staging. O HUD personalizado, PV temporários da Fúria e salvaguardas de morte foram preservados. O combate mantém o herói nocauteado na ordem; o temporizador das salvaguardas avança pelo GameMode. Um personagem estabilizado não rola novamente, e dano remove a estabilização. Recuperar 1 PV com um 20 natural restaura o controlador de animação de locomoção. Cair interrompe os mísseis pendentes.

`Scripts/Test-Navigation.ps1` executa um teste no mapa Courtyard com o controlador real, verificando deslocamento e consumo do orçamento nas quatro classes. Esse teste complementa `Test-Rules.ps1`, que cobre regras e animações em um mundo isolado. Ambos conferem o relatório JSON, pois o código de saída do Unreal sozinho não indica aprovação dos testes.

### Correção do bloqueio de movimento

`SetStaticMesh` atualiza a navegação mesmo antes do registro de um componente. Criar a arma e só depois desabilitar sua influência na navegação deixava geometria transitória que invalidava o caminho sob o personagem. `AHEquipmentComponent` agora desabilita navegação e colisão antes de atribuir a malha. A mesma ordem é aplicada aos componentes de efeitos. Não foi necessário alterar o mapa ou teletransportar o personagem.

O teste com as quatro classes equipadas passou em `Saved/Automation/Navigation-20260914-225138/index.json`. A regressão também verifica usar a habilidade de cada classe e caminhar novamente quando o gesto e os efeitos terminam.

### Validação de navegação e iniciativa — 20/09/2026

O teste existente passou antes das alterações (`Navigation-20260920-180817`, 39,220 s). A cobertura foi ampliada para executar a inicialização de iniciativa do GameMode, rejeitar Disparada fora do turno do jogador, encerrar o turno pelo controlador e conferir a restauração de ação, bônus e 900 cm. Também percorre uma rota mais longa que os 120 cm restantes e verifica a parada no limite.

Build Development Editor: aprovado em 28,98 s, sem avisos ou erros de compilação. Teste `AshenHollow.Navigation.AllClasses` em `/Game/AshenHollow/Maps/Courtyard`, `-game -NullRHI`: 1 aprovado, 0 falhas, 0 avisos, 0 pendentes, duração 51,238625 s. Relatório: `Saved/Automation/Navigation-20260920-180953/index.json`.

| Classe | Iniciativa herói / inimigo | Primeiro | Deslocamento equipado | Limite percorrido / saldo |
|---|---|---|---|---|
| Guerreiro | 7 / 6 | Herói | 204,0 cm | 120,00 / 0,00 cm |
| Bárbaro | 6 / 10 | Inimigo | 204,0 cm | 120,00 / 0,00 cm |
| Clérigo | 2 / 21 | Inimigo | 204,0 cm | 120,00 / 0,00 cm |
| Mago | 16 / 10 | Herói | 204,0 cm | 120,00 / 0,00 cm |

As quatro classes voltaram a caminhar depois da habilidade e recuperaram 9 m no turno seguinte. A passagem automática após estabilização também passou. Nenhuma nova falha de gameplay foi reproduzida; esta rodada alterou a cobertura do teste, preservando a correção anterior das armas. O teste usa o controlador real e o sistema de caminhos, mas não injeta cliques físicos nem valida renderização.

## Reações e ataques de oportunidade

Cada participante recebe uma reação por rodada, renovada no início do próprio turno,
junto com ação, ação bônus e movimento. A reação aparece como um terceiro ponto no
painel de economia de ações e como um ponto na carta de iniciativa de cada combatente.

Sair do alcance corpo a corpo de um inimigo durante o próprio movimento provoca um
ataque de oportunidade. O raio de ameaça é de 200 cm, com uma faixa de histerese de
15 cm: o movimento precisa começar claramente dentro do alcance e terminar claramente
fora dele, de modo que apenas roçar a borda não gasta a reação de ninguém. Entrar no
alcance nunca provoca, e aliados nunca provocam uns aos outros.

O ataque de oportunidade usa a mesma `UAHDiceRules::RollAttack` do ataque normal,
com o mesmo bônus de ataque, dado de dano, bônus de Fúria e desvantagem contra um
alvo em Esquiva. O painel do d20 mostra a rolagem que resolveu a reação; a interface
não rola de novo. A resolução é imediata, sem marcador de animação: o golpe cosmético
não marca o reator como ocupado, porque uma reação não consome o turno de quem reage.
O movimento de quem provocou não é interrompido, a menos que o dano o derrube.

| Comando | Efeito |
| --- | --- |
| X / Desengajar | Uma ação; o movimento deste turno não provoca ataques de oportunidade |

Desengajar ocupa na barra de ações o lugar do antigo botão Analisar, que era redundante:
passar o cursor sobre um inimigo já mostra pontos de vida, classe de armadura e chance de
acerto. A tecla C continua analisando o alvo, sem botão.

Desengajar dura até o fim do turno que o comprou e é limpo em `StartTurn`. A linha de
visão é conferida antes de gastar a reação: um alvo obstruído não provoca nada e a
reação permanece disponível.

O inimigo recua uma vez por encontro quando cai abaixo de 35% dos pontos de vida e
ainda tem mais de 300 cm de movimento. Ele não desengaja, então aceita o ataque de
oportunidade do jogador. É assim que a reação do herói fica visível em um duelo de
um contra um; com vários inimigos o caso comum passa a ser o inverso.

O registro de combate marca as reações em roxo. Um texto flutuante `REAÇÃO!` aparece
sobre quem reagiu, e o alvo recebe `OPORTUNIDADE -N` ou `OPORTUNIDADE ERROU`.

### A reação precisa ser um trinco, não uma comparação por quadro

A primeira implementação comparava, no mesmo quadro, a distância antes e depois do passo:
provocava apenas se o movimento começasse dentro de 185 cm e terminasse além de 200 cm.
Isso nunca acontece. Um personagem a 330 cm/s a 60 quadros por segundo avança cerca de
5,5 cm por quadro, portanto atravessa a faixa de histerese ao longo de vários quadros e
nenhum quadro isolado satisfaz as duas condições. O resultado era um sistema que compilava,
passava nos testes sintéticos e nunca disparava no jogo.

`UpdateThreatState` agora mantém em `InReachOf` a lista de inimigos em cujo alcance o
personagem está parado. Entra-se na lista abaixo de 185 cm e sai-se dela acima de 200 cm;
a saída provoca quando ocorre no próprio turno, sem Desengajar, e com o ameaçador ainda de
pé. A regressão cobre explicitamente permanecer dentro do alcance sem provocar e roçar a
borda sem armar nada.

## Inimigo sorteado

`BecomeEnemy` sorteia classe e ancestralidade entre as mesmas quatro opções do jogador,
aplica a ficha de nível 1 por `ApplySheet` — agora compartilhada com `ChooseClass` — e
soma 6 pontos de vida para que o duelo dure algumas rodadas. O nome exibido acompanha o
arquétipo: Espadachim, Saqueador, Oráculo ou Feiticeiro.

O inimigo usa a habilidade da própria classe: o bárbaro entra em fúria com o herói a até
6 m, o mago conjura mísseis enquanto tiver espaços, e guerreiro e clérigo se curam abaixo
de metade dos pontos de vida. A seleção de alvo dos mísseis foi corrigida — antes
procurava alvos com `bEnemy`, o que fazia um conjurador inimigo mirar em si mesmo.

A prioridade do turno inimigo é recuar, depois habilidade, depois atacar ou aproximar-se.
Recuar vem primeiro de propósito: o GameMode encerra o turno 1,2 s após a ação ser gasta,
e uma habilidade antes do recuo cortaria a fuga antes de ela sair do alcance do herói.
O limiar de recuo passou de 35% para 50% dos pontos de vida, para que o jogador
efetivamente veja a própria reação acontecer.

### Cobertura e validação pendente

A suíte `AshenHollow.Rules.Combat` foi ampliada para cobrir: reação disponível no
início da rodada, gasto ao sair do alcance, rótulo do resultado no alvo, ausência de
provocação ao entrar no alcance, apenas uma reação por rodada, Desengajar custando a
ação e protegendo o turno inteiro, expiração de Desengajar no turno seguinte, e
aliados não provocando entre si.

**Esta etapa ainda não foi compilada nem executada neste computador.** O build
(`Scripts/Build-Editor.ps1`), `Scripts/Test-Rules.ps1` e `Scripts/Test-Navigation.ps1`
precisam ser rodados antes de considerar a etapa concluída, e o relatório JSON
correspondente deve ser registrado aqui como nas etapas anteriores.

## Classes e ancestralidades como dados

`AHClassData.h` / `AHClassData.cpp` reúnem em duas tabelas o que antes estava espalhado
como `switch` e vetores de quatro posições em `AHCharacter`, `AHProgression`, `AHCombatHUD`
e `AHEquipmentComponent`. Todos indexavam pelo enum de classe: `WeaponAnimations[HeroClass]`,
`Growth[]={8,9,7,6}`, `Names[]` do inimigo e do nível 2, `HP[]={12,14,10,8}` e
`AC[]={16,14,18,12}` da tela de criação, além dos `switch` de cor e de ícone. Uma quinta
classe leria além do fim de cada um desses vetores — o que não quebra de forma confiável,
apenas devolve lixo.

`FAHClassSheet` guarda ficha, nomes, ícone, cor, arma e crescimento por nível.
`FAHAncestrySheet` guarda deslocamento, escala, bônus e o traço de sorte. `AHRules::Class`
e `AHRules::Ancestry` fazem acesso com `Clamp`, e dois `static_assert` exigem uma linha de
tabela para cada valor do enum. O equipamento passou a ser configurado por `EAHWeaponKind`,
não por classe: espada e maça carregam escudo, cajado acende a ponta.

Nada de comportamento mudou nesta etapa. A suíte `ClassesAndLocomotion` trava os números:
os quatro arquétipos e as quatro ancestralidades precisam devolver exatamente os mesmos
valores que os `switch` escritos à mão devolviam.

### O que ainda bloqueia as oito classes restantes

- **A tela de criação desenha uma única fileira de quatro cartas.** A geometria
  (`X=225+I*288`, cartas de 268 px no painel de 1200 px) é escrita para exatamente quatro.
  Doze classes exigem uma grade antes de qualquer coisa.
- **Não existe ataque à distância.** Patrulheiro usa arco longo; bruxo, druida e feiticeiro
  abrem com truques à distância. Mísseis Mágicos acerta automaticamente e não serve de base.
  Falta rolagem de ataque a distância, linha de visão, projétil e desvantagem ao atirar
  em corpo a corpo.
- **Só existem quatro clipes de ataque** (espada, machado, maça, cajado) e um de conjuração.
  Monge precisa dos clipes desarmados do pacote de manequins; arco não tem animação.
- **`CanUseClassAbility` e `UseClassAbility` continuam com `switch` por classe**, porque são
  comportamento e não dados. Cada classe nova precisa de um ramo ali.

Validação pendente: esta etapa não foi compilada nem testada neste computador.

## Tela de criação em grade

A tela desenhava uma fileira fixa de quatro cartas em `X=225+I*288`, com 268 px de largura
dentro de um painel de 1200 px. Doze arquétipos desenhariam para fora da tela.

O layout agora é derivado do tamanho da tabela. Até oito entradas usam quatro colunas;
acima disso, seis. A largura e a altura da carta saem da área disponível dividida pelas
colunas e linhas, e `Unit = Largura / 268` escala o texto e todos os deslocamentos internos,
que passaram a ser frações da altura da carta em vez de pixels absolutos. Cada linha é
centralizada de forma independente, então uma última linha incompleta não fica encostada
à esquerda.

Com quatro classes o resultado é a fileira anterior, deslocada 9 px à direita: a fileira
original não estava centralizada no painel e agora está. **O caminho de múltiplas linhas
ainda não foi exercitado** — só existirão mais de quatro entradas quando as classes novas
entrarem.

## Ataque à distância

`FAHClassSheet` ganhou `RangedRange`, `RangedSides`, `RangedBonus` e `RangedName`. Alcance
zero significa que o arquétipo só luta corpo a corpo, que é o caso de guerreiro, bárbaro e
clérigo. O mago recebeu Raio de Fogo: 18 m, 1d10, sem custo de recurso.

`TryRangedAttack` gasta a ação, confere alcance e linha de visão, rola o ataque e reaproveita
toda a máquina de impacto do golpe corpo a corpo — mesmo marcador de animação, mesmo
temporizador de segurança, mesma resolução única. A diferença está em `PendingRange`: quando
maior que zero, `ResolveImpact` valida contra o alcance da arma em vez dos 210 cm do corpo a
corpo. Sem isso, todo disparo além de dois metros seria anulado como se o alvo tivesse
escapado. O projétil reaproveita `AAHMagicVisual`, e o gesto é `AH_Cast`, porque não existe
animação de arco nem de conjuração rápida.

Atirar com um inimigo ao alcance de corpo a corpo impõe desvantagem, conforme o SRD.
`IsThreatenedInMelee` responde por isso e a dica do botão Ataque avisa antes do disparo.
O inimigo sorteado também atira quando o arquétipo tem alcance, em vez de fechar distância.

Clicar num inimigo ou apertar Q dispara de onde se está, sem caminhar, quando a classe tem
alcance e o alvo está visível. Classes corpo a corpo continuam se aproximando como antes.

A suíte de combate cobre: alvo além do alcance recusado sem gastar ação, disparo a 900 cm
que efetivamente causa dano (prova de que o limite de corpo a corpo não o anula), ameaça em
corpo a corpo detectada, e arquétipo sem alcance recusando o disparo sem custo.

Validação pendente: nada disso foi compilado nem executado neste computador.

## Sete ancestralidades e tipos de dano

Meio-orc, tiefling e draconato entraram. A tela de ancestralidade passa a ter duas fileiras
— quatro e três — e este é o primeiro uso real do caminho de múltiplas linhas da grade.

Os traços exigiram uma mudança de base. `ReceiveHit` recebia `bool bPhysical`, que só sabia
dizer "a fúria reduz isto". Resistência a fogo precisa de um tipo real, então o parâmetro
virou `EAHDamageType { Physical, Fire, Force }`. Mísseis Mágicos passam `Force`, o sopro
passa `Fire`, e todo o resto continua `Physical` por padrão. Fúria reduz apenas físico;
resistência a fogo reduz apenas fogo; não se acumulam porque não tratam do mesmo tipo.

| Ancestralidade | Traço |
|---|---|
| Meio-orc | +1 de dano; recusa uma queda a 0 PV por descanso, ficando de pé com 1 PV |
| Tiefling | +1 CA; dano de fogo pela metade |
| Draconato | +1 de dano; fogo pela metade; sopro dracônico 2d6 em cone de 4,5 m, uma vez por descanso |

Perseverança implacável é verificada logo após os pontos de vida chegarem a zero e antes da
transição para nocauteado, então o personagem simplesmente continua de pé. O texto flutuante
mostra o dano e `RESISTE!`.

O sopro gasta a ação, vira para o hostil mais próximo antes de disparar — para não punir o
ângulo da câmera — e atinge todos os hostis dentro de 4,5 m num cone de cerca de 70 graus.
**Testes de resistência ainda não existem no jogo**, então o sopro simplesmente acerta; em
5e caberia um teste de Destreza para metade do dano. Tecla T, botão na coluna lateral junto
de Surto e Círculo. O inimigo sorteado também usa o sopro quando é draconato.

Duas travas antigas foram removidas no caminho: `CombatCommand` recusava índices de raça e
de classe acima de 4, o que teria descartado silenciosamente as três ancestralidades novas.

A suíte `ClassesAndLocomotion` cobre as sete linhas da tabela, fogo reduzido no tiefling mas
não em aço, a recusa de queda funcionando uma vez e não duas, o sopro queimando um alvo à
frente e poupando um atrás, e o descanso devolvendo ambos.

Validação pendente: não compilado nem executado neste computador.

## Auditoria de regras — 23/09/2026

Revisão do projeto contra o SRD 5.1 depois da entrada de feiticeiro, ladino, paladino e
patrulheiro. O que já estava correto e foi confirmado: concentração única por vez, com teste
de CD igual ao maior valor entre 10 e metade do dano, encerrando ao cair a zero; vantagem e
desvantagem agrupadas com `||`, de modo que fontes opostas se anulam em vez de somar;
espaços de meio-conjurador em 2/3/3 a partir do nível 2; durações de Marca do Caçador,
Escudo da Fé e Bênção corretas; ataque furtivo uma vez por turno de cada criatura; e a
expiração do Raio Guia ao fim do turno seguinte do conjurador.

Quatro divergências corrigidas:

**Dano massivo não matava.** PHB 197: se o dano que sobra depois de zerar os pontos de vida
iguala ou supera o máximo, a morte é imediata, sem salvaguardas. `ReceiveHit` agora calcula
o excedente e aplica morte instantânea. Isso torna o jogo mais letal — era a diferença mais
significativa em relação a 5e.

**Cura não levantava um personagem caído.** `Health+=Amount` deixava `bDowned` ligado, e
`StartTurn` manda qualquer um com `bDowned` rolar salvaguarda em vez de agir: o personagem
ficava curado, vivo e permanentemente incapaz de jogar. Toda cura passa agora por
`ApplyHealing`, que encerra o estado de morrendo e zera as contagens. O problema era latente
porque só existe cura no próprio alvo, mas seria fatal ao introduzir grupo.

**Segundo Fôlego estava preso no nível 1.** Curava `1d10+1`; a regra é 1d10 + nível de
guerreiro. Regressão introduzida quando a progressão entrou.

**Golpes em criatura inconsciente não eram críticos.** PHB 292: um acerto corpo a corpo a
até 1,5 m é crítico automático, e um crítico custa duas falhas de salvaguarda em vez de uma.
Ataques à distância não recebem o crítico automático. A IA continua não atacando heróis
caídos, então na prática isso se aplica quando o jogador ataca um inimigo caído.

**CD de magia e teste de concentração deixaram de ser fixos.** `ConSaveModifier` e
`CastingModifier` entraram na ficha. A CD passou a ser 8 + proficiência + modificador de
conjuração e o ataque mágico, proficiência + modificador. Para conjuradores plenos de nível
1 o resultado continua 13 e +5, idêntico aos valores fixos anteriores; meio-conjuradores
agora usam CD 12, que era o valor errado antes.

Simplificações conscientes que permanecem: testes de resistência de área usam um modificador
genérico em vez do atributo específico de cada magia, e não existe sistema de condições
(caído, cego, amedrontado).

Validação pendente: não compilado nem executado neste computador.

## Armas por classe conferidas contra a ficha (2026-09-24)

`EAHWeaponKind` ganhou `Bow`. A arma de cada arquetipo passou a seguir o que a
ficha dele diz, e nao o que estava escrito antes:

- **Ladino** e **Patrulheiro** tinham `Sword`, mas os dois tem ataque a distancia na
  ficha (arco curto 24 m 1d6+3 e arco longo 36 m 1d8+3) e o roteamento de ataque
  tenta o tiro **antes** do corpo a corpo. Ou seja, na pratica eles atiram quase
  sempre e carregavam uma espada que raramente usavam. Agora usam `Bow`.
  Efeito colateral bom: o escudo so aparece para `Sword` e `Mace`, entao o ladino
  deixou de andar com escudo, o que estava errado desde o inicio.
- **Barbaro** rola 1d12, que e um machado grande, entao a linha `Axe` aponta para o
  modelo de duas maos e nao para o de uma.
- **Mago** e **Feiticeiro** tambem tem alcance na ficha, mas o deles e Raio de Fogo,
  uma magia. Continuam de cajado, como deve ser.
- **Guerreiro**, **Paladino** (espada e escudo) e **Clerigo** (maca e escudo) ja
  estavam certos.

Risco fechado no caminho: `PlayAttack` indexava `WeaponAnimations` direto pelo
`EAHWeaponKind`, sem checar limite. Acrescentar `Bow` teria estourado o array na
primeira pancada corpo a corpo de um arqueiro. Agora os dois leitores passam por
`AAHCharacter::WeaponClip()`, que checa o indice, e a lista de clipes tem uma
entrada por tipo. Atirar toca o clipe de conjuracao; a pancada com o arco pega
emprestado o golpe de cajado, que e o movimento de duas maos mais proximo que temos.

## Cobertura, terreno elevado e arena procedural (2026-09-24)

### Regras

`AHArena` guarda as duas coisas: a regra de cobertura e o sorteio do cenario.

- **Cobertura parcial: +2 CA. Cobertura de tres quartos: +5 CA.** SRD 5.1. Um
  obstaculo entre atirador e alvo conta pela altura que ele tem **medida a partir do
  chao do ALVO** — o caixote que esconde alguem sobre as lajes nao esconde ninguem em
  cima do deck. Menos de 30% do corpo nao e cobertura; a partir de 70% vira tres
  quartos.
- **Terreno elevado da vantagem em ataque.** Isso e regra do Baldur's Gate, **nao** do
  SRD: o livro nao concede vantagem por altura. Esta aqui porque paridade com o BG3 e
  o objetivo declarado. Precisa de 1 m de diferenca, entao subir num caixote nao vale;
  o deck vale.
- A vantagem por altura entra no **mesmo grupo `||`** das outras fontes. Altura mais
  ataque temerario continua sendo vantagem, nunca dois passos dela.
- Cobertura vale em corpo a corpo tambem, como no livro. Com 190 cm de alcance quase
  nunca dispara, mas regra que so vale pela metade e pior que regra que nao existe.

Cobertura **total** nao sai daqui. Quem decide se o tiro esta bloqueado e o teste de
linha de visao do `TryRangedAttack`, que tambem enxerga casas e muros que nunca
estiveram nesta lista. Dois sistemas respondendo a mesma pergunta e como eles acabam
discordando.

### Sorteio por encontro

`AHArena::Generate` produz de 9 a 14 obstaculos a partir de uma semente.
`AHGameMode::BuildArena` os cria, **mede o bounding box real de cada um** e grava raio
e altura de volta na lista — a conta de cobertura roda contra o tamanho verdadeiro do
prop, nao contra um numero digitado. Cada `NextEncounter` sorteia de novo.

Tres regras protegem o encontro de uma semente ruim: nada nasce a menos de 4 m de
qualquer um dos dois lados, nada entra no corredor de 3 m entre os nascimentos, e nada
cai dentro do deck elevado. Verificado por simulacao em 4000 sementes antes de
compilar: minimo de 9 obstaculos, nenhuma arena vazia, nenhuma violacao.

### HUD

A porcentagem de acerto agora **usa a mesma conta dos dados**: CA com cobertura,
vantagem por altura, desvantagem por esquiva ou por atirar com inimigo colado. Antes
ela ignorava vantagem por completo. Embaixo dela sai uma linha dizendo por que — o
custo de nao fazer isso ja apareceu duas vezes neste projeto, com vantagem e com a
sorte do halfling: regra invisivel le como bug.

### Arena

26x26 m, nascimentos a 10 m em (0,-500) e (0,+500). Nao e o maior possivel de
proposito: com os dois lados em pontas opostas, seriam dois ou tres turnos so andando
antes de qualquer coisa acontecer, e combate por turnos nao tem folga para turno
morto. O deck elevado fica em x=780, 1,3 m de altura, com duas rampas de ~21 graus,
bem abaixo dos 44 que o agente de navegacao aceita.

## A arena inteira passa a ser sorteada (2026-09-24)

Antes so os ~11 obstaculos eram procedurais; casas, muros, braseiros, deck e
iluminacao estavam assados no mapa, entao duas arenas eram a mesma arena com
barris em lugares diferentes. Agora `AHArena::Build` devolve a arena toda.

**Quatro arquetipos**, sorteados por semente: praca da vila (fechada dos quatro
lados, braseiros nos cantos), rua do mercado (predios nos dois lados longos, aberta
nas pontas, barracas em duas fileiras), terreiro (campo aberto, cercas cortando o
fundo, arvores, poco) e ruinas (muros de pedra quebrados, entulho, sem cerca).

**A hora do dia tambem e sorteada** — angulo, temperatura e intensidade do sol e do
ceu. Custa nada perto de geometria e muda mais a sensacao de lugar do que geometria.
Os atores de luz continuam no mapa e o `AHGameMode` so mexe nos valores deles: luz
criada em tempo de execucao nao e capturada pela sky light como uma colocada e.

### O que continua assado no mapa

So o que nao da para sortear: o chao, o volume de navegacao, o ponto de nascimento
e os atores de luz. O `Build-Arena.py` encolheu de ~430 para ~360 linhas.

### Medir antes de posicionar, de novo

Ladrilhar uma cerca sem vao nem sobreposicao exige saber o comprimento real do
painel, e isso e fato do asset, nao constante. O `AHGameMode` passa ao gerador uma
funcao de medicao que le `UStaticMesh::GetBounds()` — sem precisar criar nada — e
guarda a resposta, porque a mesma cerca e consultada dezenas de vezes seguidas.
Depois que a peca existe, raio e topo sao regravados com os limites medidos, entao
a conta de cobertura roda contra o que esta de pe.

### Dois erros de desenho pegos antes de compilar

Auditando os pontos de colocacao achei que em **Ruinas** um muro de pedra podia
atravessar o corredor entre os nascimentos e lacrar o encontro, e que em **Terreiro**
o deck podia cair em cima do inimigo. A correcao nao foi remendar cada arquetipo: o
teste do corredor virou uma funcao unica (`InLane`) pela qual passa toda colocacao
dentro da arena, e coisas grandes usam `ClearSpot`, que so devolve lugar livre. O
teste de automacao agora roda 400 sementes e exige que nenhuma peca solida caia no
corredor nem sobre um nascimento, e que os quatro arquetipos apareçam.

### `Height` virou `TopZ`

`Height` queria dizer "altura acima da propria origem", mas a origem de um deck e o
meio dele, entao origem + altura passava do topo pela metade. Dizer **onde o topo
esta** tem um sentido so, e e justamente o numero que da para medir.

## Camera que gira (2026-09-24)

A camera era fixa em -48 de inclinacao e -45 de giro. Num jogo onde cobertura e
linha de visao decidem a rolagem, nao conseguir olhar atras de uma casa nao e so
desconforto: e informacao de regra escondida. E ficou pior quando a arena passou a
ser sorteada, porque agora a casa pode nascer em qualquer lugar.

- **A e D giram em passos de 45 graus.** Q e E ja sao ataque e conjurar, entao A e D
  ficaram, que tambem sao os mais proximos da mao. Oito angulos bastam para ver
  atras de qualquer coisa.
- **Roda do mouse aproxima e afasta**, entre 12 e 36 m de distancia.

Passos de 45 graus, e nao giro livre, de proposito: a leitura isometrica e o que
deixa distancia julgavel de relance, e camera em angulo arbitrario tira isso em
silencio. O giro e suavizado entre um passo e outro — corte seco de 45 graus perde
a orientacao de quem esta olhando.

O par angulo-atual/angulo-desejado volta para -180..180 junto, quando os dois ja
coincidem, para uma sessao longa girando sempre para o mesmo lado nao acumular
angulo grande. Dobrar os dois no mesmo quadro garante que nao ha nada para ver.

**Ainda nao resolvido:** o braco da camera nao testa colisao, entao em certos
angulos ela pode atravessar uma casa. Com o giro disponivel da para sair de la, e
ligar o teste de colisao faria a camera saltar para perto sem aviso. Se incomodar,
o certo e desbotar o que esta na frente, nao mover a camera.
