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
