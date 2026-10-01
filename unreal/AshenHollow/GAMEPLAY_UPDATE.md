# Ficha e inventário

Abra a ficha com **P**. Ela pausa o jogo e reúne atributos, valores derivados,
equipamento, mochila e descanso. P ou Fechar retoma a partida.

- Criação: ancestralidade → classe → compra de atributos → kit inicial.
- Compra: 27 pontos, valores de 8 a 15 antes dos bônus de ancestralidade.
  Sugestão restaura uma distribuição válida para a classe.
- A ficha aplica atributos à vida, ataque, dano, iniciativa, defesa,
  ataque/CD de magia e concentração. O equipamento altera a ficha e a arma visível.
- Mochila: Equipar troca o item com o espaço correspondente; Guardar devolve o
  equipado à mochila. Armas de duas mãos guardam o escudo. A mochila tem páginas.
- Poções: consomem uma unidade, curam até o máximo e custam uma ação em combate.
  Não são gastas com a vida cheia. Trocas de equipamento ficam fora de combate.
- Descanso curto fora da cidade: avança uma hora, gasta um dado de vida se houver
  ferimentos e recupera habilidades de descanso curto. Recuperação Arcana é
  limitada por descanso longo.
- Descanso longo na cidade: avança oito horas, recupera vida, recursos e ao menos
  metade dos dados de vida máximos. Inimigos próximos impedem descansar.
- O relógio controla uma transição simples de iluminação diurna/noturna.
  Um dia dura aproximadamente 24 minutos reais; a ficha pausa o relógio.

As regras usam a base de atributos/compra por pontos de 5e, com adaptações do RPG
(nível máximo 4, catálogo e habilidades próprios). Não são uma implementação
integral de todas as regras de mesa. O inventário desta etapa usa os kits
iniciais e os saques de inimigos; comércio ainda não está implementado.

## Exploração, saques e evolução

- **P**: ficha, prévia do personagem equipado e mochila em grade. Arraste um item
  para o espaço compatível; arraste um equipado de volta à mochila para guardar.
  Clicar seleciona um item, e o botão abaixo permite equipar ou beber uma poção.
- **P > Evolução**: mostra habilidades de classe, subclasse e espaços de magia.
  No nível 4, escolha Robusto, Alerta, Móvel ou +2 em um atributo (máximo 20).
  A escolha não depende de terminar o vale e só pode ser feita uma vez.
- **G** perto de um saque: recolhe ouro e item. Animais deixam provisões;
  bandidos deixam ouro e um item do catálogo. Cada saque só pode ser recolhido uma vez.
- **J**: diário de missões. Moradores oferecem combate contra bandidos, lobos,
  ursos e uma visita a um marco antigo. Volte ao solicitante para receber XP e poção.
  Azul no minimapa indica pedido disponível, dourado o objetivo, verde a entrega.
- Dungeons: detecção e testemunhas limitadas a **3 metros**, com linha de visão.
  Membros distantes do mesmo grupo não entram automaticamente; alertas não se
  propagam de grupo em grupo. Em furtividade, percepção é rolada a cada quatro
  segundos contra 10 + Destreza (+ proficiência para ladinos), dentro do alcance.
- Lobos e ursos usam modelos originais em `Art/Wildlife/Wildlife.blend`, com
  animação procedural das quatro patas. Encontros externos exigem uma rota válida
  até o jogador antes de nascer, para evitar inimigos presos sobre estruturas.

Missões e saques pertencem ao vale atual. Esta versão ainda não adiciona salvamento
persistente da jornada nem animações esqueléticas próprias para os animais.

## Verificação

Os testes `AshenHollow.Sheet` cobrem compra por pontos, cálculos derivados,
comandos de criação, inventário e recuperação. `AshenHollow.WorldAccess` verifica
as rotas das dungeons e a saída física de um inimigo. Resultados de execução
ficam em `Saved/Automation`.

Os modelos autorais estão em `Art/LifeKit/LifeKit.blend`. `Scripts/Import-Kit.py`
salva explicitamente os vínculos de material e verifica as conexões do shader.
`Scripts/Make-LifeAudio.py` e `Import-LifeAudio.py` geram/importam os sons locais.
