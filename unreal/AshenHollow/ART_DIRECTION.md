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
