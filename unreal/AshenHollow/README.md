# Ashen Hollow — Unreal Engine 5.8

**Estado: quatro classes jogáveis, seleção inicial, locomoção nativa, duelo por turnos e d20 no Unreal 5.8.2.** O protótipo Godot continua preservado na raiz.

## O que está preparado

- Projeto C++ fixado no UE 5.8, com targets Game e Editor.
- Dependências para Enhanced Input, Gameplay Ability System, UMG, navegação e IA.
- Biblioteca de regras d20 exposta a Blueprints, incluindo modificadores, proficiência, vantagem/desvantagem, ataques, críticos e testes.
- Estrutura de resultado que permite à futura interface mostrar exatamente a rolagem que resolveu a ação.
- Classe base de personagem com câmera isométrica em perspectiva, SpringArm e AbilitySystemComponent.
- Cópias verificáveis dos dados de 12 classes, sete ancestralidades e da biblioteca atual de habilidades.
- Três suítes C++ aprovadas: `Dice`, `Combat` e `ClassesAndLocomotion`; zero falhas e avisos. Relatório: `Saved/Automation/20260913-111037/index.json`. O teste de locomoção verificou deslocamento de aproximadamente 55 cm do pé a 300 cm/s, distinguindo caminhada de oscilação em repouso.
- Guerreiro, bárbaro, clérigo e mago com fichas e habilidades distintas. Detalhes e limitações em [COMBAT_CLASSES.md](COMBAT_CLASSES.md).

## Duelo por turnos

`Play-Unreal.cmd` abre a seleção de quatro classes no pátio. Após a escolha, a ordem é sorteada com d20 + iniciativa; empates favorecem o jogador. Cada participante recebe uma ação, uma ação bônus e 9 m de movimento no começo do próprio turno. O jogador encerra seu turno pelo botão ou Enter. O inimigo executa seu turno automaticamente. Os recursos não se recuperam por tempo decorrido.

| Comando | Efeito |
| --- | --- |
| Botão direito no chão | Percorrer uma rota na NavMesh, limitada pelo movimento restante |
| Botão direito no inimigo / Q / ícone Ataque | Aproximar e atacar, consumindo uma ação |
| Espaço / Esquiva | Parar e consumir uma ação para esquivar até o próximo turno próprio |
| E / habilidade de classe | Fôlego, Fúria, Curar Ferimentos ou Mísseis Mágicos, conforme a ficha |
| R / Disparada | Uma ação para ganhar mais 9 m de movimento |
| C / Arcana | Teste d20+1 contra dificuldade 12, consumindo uma ação |
| Enter / Encerrar turno | Passar a vez após a conclusão de uma animação em andamento |
| F5 | Reiniciar o duelo e sortear a iniciativa novamente |
| F6 | Alternar Desempenho, Equilibrado e Qualidade; FPS e tempo por quadro no HUD |

O HUD apresenta ordem de iniciativa, recursos disponíveis, habilidades clicáveis com descrições, chance de acerto ao apontar para o alvo, indicadores de equipe, dano flutuante e histórico de combate. O d20 tem faces preenchidas e mostra o resultado real da ação; não existe rolagem adicional para a interface. O painel da rolagem desaparece após oito segundos, enquanto o histórico guarda os últimos cinco eventos.

As quatro classes usam golpes próprios de espada, machado, maça ou cajado no DefaultSlot, com retorno à locomoção. Há gestos distintos para conjuração, cura, fúria, guarda e evasão. Cada golpe e conjuração contém um marcador de contato que aplica o resultado uma única vez, com temporizador de segurança. Morte, perda de alcance ou obstrução podem cancelar o dano pendente; a animação não concede deslocamento gratuito. Os nove clipes e seus tempos estão documentados em COMBAT_CLASSES.md.

A prioridade atual é refinar o combate neste pátio antes de ampliar mapas. Há uma reação visual provisória a dano não letal usando a sequência `MM_HitReact_Front_Lgt_01` do pacote já importado. Ela não move a cápsula nem interrompe um ataque comprometido. Textos flutuantes distinguem dano, crítico, erro e cura, com desaparecimento gradual. Jogador e inimigo se aproximam a aproximadamente 1,25 m entre centros antes de iniciar o golpe. Ainda é necessário ajustar contatos, poses e reações com a biblioteca final de animações.

A câmera acompanha suavemente o ponto médio entre os dois combatentes. Isso mantém o inimigo inicial enquadrado quando o jogador vence a iniciativa. Indicadores de equipe e textos do mundo ficam limitados à área acima da barra de ações.

O enquadramento foi conferido com ambos os participantes iniciando a ordem. O reinício por F5 também foi validado após remover, em `DefaultInput.ini`, o atalho herdado de diagnóstico `viewmode shadercomplexity`, que alterava indevidamente a renderização ao reiniciar. A compilação final dos ajustes de apresentação passou; os testes de regras acima antecedem apenas esses ajustes de câmera, texto e configuração de entrada.

Validação dessa etapa: relatório `20260912-191450` com duas suítes aprovadas sem avisos. A suíte de combate também verifica feedback coerente com o resultado, cura distinta e ausência de dano duplicado ao chamar notify duas vezes seguido do temporizador. Na execução real, os cliques em Fôlego (+3 PV, apenas bônus consumido), Ataque (11+5=16, dano 7) e Encerrar turno funcionaram. A rodada seguinte renovou ação, bônus e 9 m. O texto verde de cura e o dano flutuante foram observados; a precisão do contato e a qualidade da reação ainda precisam de revisão quadro a quadro. Foi acrescentada sombra aos textos para melhorar a leitura sobre superfícies claras.

Ainda é um duelo de teste com uma das quatro fichas predefinidas e um inimigo. As armas e escudos têm modelos estilizados ligados às mãos; cura, impacto, defesa e magia têm efeitos próprios. Os manequins, modelos, contatos e cenário continuam provisórios e precisam de refinamento artístico; não constituem uma entrega AAA final. Faltam ataques de oportunidade, cobertura, elevação, biblioteca ampliada de magias, múltiplos integrantes, exploração fora de turnos, criador UMG e habilidades GAS concretas. O inimigo tem um limite de dez segundos para não bloquear indefinidamente o encontro se não encontrar uma rota.

Os dados de 12 classes permanecem preservados. A interface Unreal oferece quatro fichas curadas; o criador completo e as outras oito classes ainda não foram migrados. A versão Godot mantém o protótipo de referência. `Edit-Unreal.cmd` abre o mapa; `Scripts/Create-Courtyard.py` recusa sobrescrever um mapa já existente para preservar edições manuais.

## Ambiente neste computador

Validação dos contatos em 13/09/2026: build aprovado, duas suítes sem falhas/avisos (`20260913-001423`), incluindo presença de exatamente um notify dentro de cada sequência. No jogo, dois ataques do jogador e um do inimigo produziram `AH_CONTACT_NOTIFY` imediatamente antes de `AH_IMPACT`, confirmando a resolução pelo marcador. Aproximação pelo botão Ataque, alternância para o segundo golpe e avanço de rodada foram observados. Não houve entradas `Error:` ou `Fatal:` nesse log. Isso não valida animações de armas nem contato perfeito em todas as posições.

`Scripts/Inspect-CombatAnimation.py` amostra as poses sem editar assets e grava `Saved/CombatAnimationSamples.json`. `Scripts/Prepare-CombatAnimation.py` cria as cópias com contatos; preserva cópias já existentes e os assets de origem. Ambos rodam pelo UnrealEditor-Cmd com `-run=pythonscript -script=<caminho absoluto>`. Os `.uasset` preparados já estão presentes; não é necessário rodar os scripts para jogar.

- Unreal Engine **5.8.2**, confirmado pelo `Engine/Build/Build.version` da instalação em `C:\Program Files\Epic Games\UE_5.8`.
- Visual Studio Build Tools 2022 **17.14.40**, MSVC **14.44.35228**.
- Windows SDK **10.0.22621.0** e .NET Framework SDK **4.8**.
- Aproximadamente 16 GB de RAM e RTX 3050 Laptop com 6 GB de VRAM. O script limita a compilação a duas tarefas simultâneas por padrão.

Executar nesta pasta:

```powershell
.\Scripts\Check-Data.ps1
.\Scripts\Build-Editor.ps1
.\Scripts\Test-Rules.ps1
```

O teste inicia o editor sem renderização e exporta o resultado em `Saved/Automation/<data-hora>/index.json`. O script exige pelo menos um teste aprovado, sem falhas nem testes não executados. Após compilar, `Edit-Unreal.cmd` na raiz abre o projeto no editor. O mapa já inclui o duelo por turnos e o HUD; o criador de personagem ainda precisa ser migrado.

Validação de 09/09/2026: target `AshenHollowEditor Win64 Development` compilado com sucesso; relatório `Saved/Automation/20260909-222021/index.json` aprovado. Os testes exercitam críticos, falha automática de ataque com 1 natural, diferença entre ataques e testes de atributo, vantagem e modificadores. A verificação dos JSONs também passou. A validação adicional de 10/09/2026 aprovou os dois testes após a inclusão do duelo. O HUD foi conferido na execução real: ataque natural 15 + 5 = 20 contra CA 13, com dano 6, idêntico ao log. Não houve erros de runtime nesse duelo. Isso não equivale a validar desempenho, todas as condições de combate ou um jogo empacotado.

Validação de 12/09/2026: recompilação aprovada e duas suítes aprovadas sem avisos (`20260912-185630`). Corrigida a ativação do participante em `StartTurn`: a versão anterior renovava recursos, mas deixava o turno inativo. Os testes verificam a passagem da vez, avanço de rodada, renovação no próximo turno próprio, custos de ação/bônus, esquiva, limite de movimento e cancelamento de dano pendente após morte. A checagem dos dados também passou.

Na execução visual de 12/09, ambos os personagens estavam enquadrados, com anéis de equipe e HUD legível. O d20 exibiu 19 + 5 = 24 contra CA 13, dano 11, correspondente ao `AH_IMPACT HERO` do log. O log registra ataques alternados e encerramento normal, sem entradas `Error:` ou `Fatal:`. A janela foi fechada durante a checagem; o teste manual dos cliques do HUD ficou pendente nessa execução. Essa observação não valida desempenho nem sincronização quadro a quadro da animação.

Os requisitos de versão foram conferidos também nos arquivos locais `Engine/Config/Windows/Windows_SDK.json` e `Engine/Source/Programs/UnrealBuildTool/Configuration/Rules/TargetRules.cs` do motor instalado.

## Sequência da migração

1. **Concluído:** compilar esta base no UE 5.8.2 e passar os testes d20.
2. **Concluído:** mapa de teste, NavMesh, controller de clique e personagem rigado. Movimento, animação, câmera e chegada do outro lado de um pilar verificados na execução real.
3. Montar locomotion blend space/Animation Blueprint, viradas e montagens de ataque. Motion Matching entra quando tivermos uma biblioteca de animações suficiente.
4. Migrar atributos e pools para AttributeSets/GameplayEffects; implementar habilidades e tempos de ação com GAS, preservando as decisões do protótipo.
5. Recriar o criador em UMG a partir de Data Assets, e a apresentação do d20 a partir de FAHDiceOutcome. Nenhuma rolagem adicional na UI.
6. Migrar inimigo, combate, descanso, mortes e loot; conferir paridade com o protótipo de referência.
7. Construir a direção de arte com assets originais/licenciados, iluminação, materiais, animações e efeitos; medir o desempenho antes de expandir a região.

**Conversão de unidades:** o protótipo usa metros e Y para cima. Unreal usa centímetros e Z para cima. A importação de alcance/raio/velocidade deve multiplicar unidades lineares por 100; posições e rotações precisam de conversão de eixos, não apenas escala. Segundos, dados e atributos não mudam. Os JSONs são preservados em unidades originais para evitar duas fontes de verdade.

## Direção técnica e gráfica

- C++: regras, validação, dados e componentes reutilizáveis.
- Blueprints: composição, feedback, ajustes de combate e apresentação.
- Enhanced Input: comandos e futura remarcação de teclas.
- GAS: habilidades, tags de estado, custos e cooldowns.
- UMG: criador de personagem, HUD e d20.
- Animation Blueprint, Blend Spaces e montagens primeiro; Control Rig/IK e Motion Matching conforme os assets necessários estiverem disponíveis.

O perfil inicial evita Lumen e Virtual Shadow Maps até existir uma cena real para medir. Isso reduz a carga inicial no notebook e pode ser ajustado posteriormente. Não prometemos gráficos AAA ou 60 FPS apenas pela troca de motor. O objetivo é uma direção de arte consistente, bons assets e animação bem integrada, com opções de qualidade compatíveis com o hardware.

## Fontes oficiais

- Versão instalada: `Engine/Build/Build.version` (5.8.2).
- [Visual Studio para Unreal](https://dev.epicgames.com/documentation/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine)
- [Requisitos de hardware](https://dev.epicgames.com/documentation/unreal-engine/hardware-and-software-specifications-for-unreal-engine)
- [Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine)

Ver THIRD_PARTY_NOTICES.md para a atribuição das regras SRD 5.1.


