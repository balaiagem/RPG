# Classes jogáveis no duelo Unreal

A seleção aparece antes da iniciativa e confirma uma ficha predefinida de nível 1. F5 volta à seleção. Esta etapa implementa quatro arquétipos; os dados das outras oito classes permanecem preservados, mas ainda não são opções jogáveis no Unreal.

| Classe | PV / CA | Ataque básico | Habilidade E |
|---|---|---|---|
| Guerreiro | 12 / 16 | +5, 1d8+3 | Segundo Fôlego: bônus, cura 1d10+1, uma vez por encontro |
| Bárbaro | 14 / 14 | +5, 1d12+3 | Fúria: bônus, duas utilizações; +2 ao dano físico corpo a corpo e resistência física |
| Clérigo | 10 / 18 | +4, 1d6+2 | Curar Ferimentos: ação, cura própria 1d8+3, dois espaços por encontro |
| Mago | 8 / 12 | +2, 1d6 | Mísseis Mágicos: ação, três dardos de força de 1d4+1, dois espaços por encontro |

Fúria dura no máximo dez turnos próprios e termina ao encerrar um turno sem atacar nem ter recebido dano desde o fim do anterior. Resistência física arredonda o dano para baixo; dano de força a ignora. Os mísseis escolhem o inimigo visível mais próximo a até 18 m e resolvem no marcador da animação, sem rolagem de ataque. A rolagem de dano dos dardos é compartilhada. Cura de clérigo é limitada ao próprio personagem nesta etapa.

As fichas são arquétipos curados, não um criador completo de D&D: faltam edição de atributos, ancestralidade, subclasses, lista de magias, concentração e descanso. Os modelos, equipamentos e gestos de conjuração continuam provisórios; ataques básicos ainda usam movimentos desarmados mesmo quando a ficha representa uma arma.

## Animação e desempenho

`UAHAnimInstance` avalia diretamente o blend space de parado/caminhada/corrida com a velocidade horizontal real. O slot de combate fica sobre essa base. A suíte de regressão verifica mudança real da pose do pé, além de custos, resistência e resolução única de magias.

F6 alterna configurações em memória, sem salvar preferências no disco:

| Perfil | Escalabilidade | Resolução interna | Limite de FPS |
|---|---|---|---|
| Desempenho | Média | 85% | 120 |
| Equilibrado (inicial) | Alta | 100% | 60 |
| Qualidade | Épica | 100% | 60 |

Os limites são alvos, não medições nem garantias. Lumen e Virtual Shadow Maps permanecem desligados na configuração atual. O desempenho deve ser reavaliado quando os personagens e cenários finais substituírem os placeholders.
