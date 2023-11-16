# 23b - emb  - av2

Prezado aluno:

- A prova é prática, com o objetivo de avaliar sua compreensão a cerca do conteúdo da disciplina. 
- É permitido consulta a todo material pessoal (suas anotações, códigos), lembre que você mas não pode consultar outros alunos.
- Duração total: 2h 

Sobre a avaliacão:

1. Você deve satisfazer ambos os requisitos: funcional e código para ter o aceite na avaliação;
1. A avaliação de C deve ser feita em sala de aula por um dos membros da equipe (Professor ou Técnico);
2. A entrega do código deve ser realizada no git.

**Você vai precisar:**

- ▶️ Kit SAME70-XPLD
- ▶️ Conectar o OLED1 ao EXT-1
- ▶️ Conectar o Potenciômetro no pino PD30

**Periféricos que vai precisar utilizar:**

- OLED
- PIO
- RTT
- UART console (printf)

**Código exemplo fornecido:**

No código fornecido (e que deve ser utilizado) os botões e LEDs da placa OLED já foram inicializados na função (`io_init`) e os callbacks dos botões já estão configurados. Temos uma `task_oled que é inicializada e fica exibindo no OLED um ponto piscando. Pedimos para **não mexer** nessa task, pois ela serve de debug para sabermos se seu programa travou (se parar de piscar tem alguma coisa errada com o seu programa).

## Descritivo

Vamos criar um protótipo de um datalogger, onde um sistema embarcado coleta periodicamente valores e eventos do mundo real, formata os dados e envia para um dispositivo externo. O envio da informação será feito pela UART. O datalogger também irá verificar algumas condições de alarme.

### Visão geral do firmware

![](diagrama.png)

O firmware vai ser composto por três tasks: `task_adc`, `task_events` e `task_alarm` além de duas filas: `xQueueEvent` e `xQueueADC` e dois semáforos: `xSemaphoreEventAlarm` e `xSemaphoreAfecAlarm`. A ideia é que a cada evento de botão ou a cada novo valor do ADC, um log formatado seja enviado pela UART (`printf`) e uma verificação das condições de alarme checadas, se um alarme for detectado a `task_alarm` deve ser iniciada. O log (UART/printf) deve possuir um timestamp que indicará quando o dado foi lido pelo sistema (`SS:`).

A seguir mais detalhes de cada uma das tarefa:

### task_adc

| Recurso               | Explicação                                           |
|-----------------------|------------------------------------------------------|
| RTT                   | Fornecer as informações do TimeStamp                 |
| AFEC                  | Leitura analógica                                    |
|-----------------------|------------------------------------------------------|
| `xQueueAFEC`          | Recebimento do valor do ADC                          |
| `xSemaphoreAfecAlarm` | Liberação da `task_alarm` devido a condição de alarm |

A `task_adc` vai ser responsável por coletar dados de uma entrada analógica via AFEC, os dados devem ser enviados do *callback* do AFEC via a fila `xQueueADC` a uma taxa de uma amostra por segundo (1hz). A cada novo dado do AFEC a condição de alarme deve ser verificada.

A task, ao receber os dados deve realizar a seguinte ação:

1. Enviar pela UART o novo valor no formato a seguir:
    - `[AFEC ] $SS $VALOR`  --->  ( `$VALOR` deve ser o valor lido no AFEC )
1. Verificar a condicão de alarme:
    - 5 segundos com o valor do AFEC maior que 3000
    
Caso a condição de alarme seja atingida, liberar o semáforo `xSemaphoreAfecAlarm`.

#### Log

O seguinte log deve ser enviado para a serial assim que lido um valor do AFEC.

- `[AFEC] SS $Valor` (onde `$Valor` é o valor do AFEC).

### task_event 

| Recurso                | Explicação                                           |
|------------------------|------------------------------------------------------|
| RTT                   | Fornecer as informações do TimeStamp                 |
| PIO                    | Leitura dos botões                                   |
|------------------------|------------------------------------------------------|
| `xQueueEvent`          | Recebimento dos eventos de botão                     |
| `xSemaphoreEventAlarm` | Liberação da `task_alarm` devido a condição de alarm |


A `task_event` será responsável por ler eventos de botão (subida, descida), para isso será necessário usar as interrupções nos botões e enviar pela fila `xQueueEvent` o ID do botão e o status (on/off). A cada evento a task deve formatar e enviar um log pela UART e também verificar a condição de alarme.

A task, ao receber os dados deve realizar a seguinte ação:

1. Enviar pela UART o novo valor no formato a seguir:
    - `[EVENT] $SS $ID:$status`
        - `$ID`: id do botão (1,2,3)
        - `$status`: 1 (apertado), 0 (solto)
1. Verificar a condição de alarme:
    - Dois botões pressionados ao mesmo tempo
    
Caso a condição de alarme seja atingida, liberar o semáforo `xSemaphoreEventAlarm`.

#### Log

O seguinte log deve ser enviado para a serial assim que detectado um evento no botão (subida/descida).

- `[EVENT] $SS $ID:$Status`

### task_alarm

| Recurso                | Explicação                                 |
|------------------------|--------------------------------------------|
| PIO                    | Acionamento dos LEDs                       |
|------------------------|--------------------------------------------|
| `xSemaphoreAfecAlarm`  | Indica alarme ativado devido a task_afec  |
| `xSemaphoreEventAlarm` | Indica alarme ativado devido a task_event |

Responsável por gerenciar cada um dos tipos de alarme diferente: `afec` e `event`. A cada ativacão do alarme a task deve emitir um Log pela serial, O alarme vai ser um simples pisca LED, para cada um dos alarmes vamos atribuir um LED diferente da placa OLED: 

- `EVENT`: LED1
- `AFEC `: LED2

Os alarmes são ativados pelos semáforos `xSemaphoreAfecAlarm` e `xSemaphoreEventAlarm`. Uma vez ativado o alarme, o mesmo deve ficar ativo até a placa reiniciar.

#### Log

Ao ativar um alarme, a `task_alarm` deve emitir um log pela serial no formato descrito a seguir:

- `[ALARM] $SS $Alarm` (onde `$Alarm` indica qual alarme que foi ativo).

#### OLED

Exibir no OLED um log simplificado (um por linha):

```  
$SS AFEC
$SS Event
```

### Exemplo de log completo

A seguir um exemplo de log, nele conseguimos verificar a leitura do AFEC, e no segundo 04 (5ª do log) o botão 1 foi pressionado, e depois solto no segundo 05. No segundo 06 o AFEC atinge um valor maior que o limite e fica assim por mais 5 segundos, ativando o alarme no segundo 9.

``` 
    |Evento
    |
    |     |Timestamp segundo 
    |     |  
    |     |   |Valor
    v     v   v
 [AFEC ] 01 1220
 [AFEC ] 02 1222
 [AFEC ] 03 1234
 [AFEC ] 04 1225
 [EVENT] 04  1:1
 [AFEC ] 04 1245
 [AFEC ] 05 1245
 [EVENT] 05  1:0
 [AFEC ] 06 4000
 [AFEC ] 07 4004
 [AFEC ] 08 4002
 [AFEC ] 08 4001
 [AFEC ] 08 4001
 [ALARM] 09 AFEC
```

## Resumo

A seguir um resumo do que deve ser implementando:

- Leitura do AFEC via TC 1hz e envio do dado para a fila `xQueueAfec`
- Leitura dos botões do OLED via IRQ e envio do dado para fila `xQueueEvent`
- `task_afec`
    - log:  `[AFEC ] $SS $VALOR` 
    - alarme se o valor do AFEC estiver maior que 3000 durante 5s
        - libera semáforo `xSemaphoreAfecAlarm`
- `task_event`
    - log:  `[EVENT] $SS $ID:$STATUS` 
    - alarme se houver dois botões pressionados ao mesmo tempo
        - libera semáforo `xSemaphoreEventAlarm`
- `task_alarm`
    - verifica dois semáforos: `xSemaphoreEventAlarm` e `xSemaphoreAfecAlarm`
    - quanto liberado o semáforo, gerar o log:  `[ALARM] $SS $ALARM` 
    - **piscar** led 1 se alarm AFEC ativo (`xSemaphoreAfecAlarm`)
    - **piscar** led 2 se alarm EVENT ativo (`xSemaphoreEventAlarm`)
    - Exibir no OLED as informações do alarme
    

## Regras de software

- Seguir a estrutura de firmware descrita!
- Não usar variável global (apenas recursos do RTOS)
- Passar no codequality 

### Dicas

Comece pela `task_event` depois faça a `task_afec` e então a `task_alarm`.
