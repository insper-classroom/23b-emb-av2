# 22a - Avaliação Prática 2

Vamos criar um protótipo de um dataloger, um sistema embarcado coleta periodicamente valores e eventos do mundo real, formata e envia para um dispositivo externo. O envio da informação será feito pela UART. O dataloger também verificar algumas condicoes de alarme  

## Visão geral do firmware

![](diagrama.png)

O firmware vai ser composto por três tasks: `task_adc`, `task_events` e `task_alarm` além de duas filas: `xQueueEvent` e `xQueueADC` e dois semáforos: `xSemaphoreEventAlarm` e `xSemaphoreAfecAlarm`. A ideia é que a cada evento de botão ou a cada novo valor do ADC um log formatado seja enviado pela UART (`printf`) e uma verificacão das condicoes de alarme checadas, se um alarme for detectado a `task_alarm` deve ser iniciada. O log que será enviado pela UART deve possuir um timestamp que indicará quando o dado foi lido pelo sistema embarcado (DIA:MES:ANO HORA:MINUTO:SEGUNDO).

A seguir mais detalhes de cada uma das tarefa:

### task_adc

| Recurso               | Explicacao                                           |
|-----------------------|------------------------------------------------------|
| RTC                   | Fornecer as informacões do TimeStamp                 |
| TC                    | Gerar o 1hz do AFEC                                  |
| AFEC                  | Leitura analógica                                    |
|------------------------|------------------------------------------------------|
| `xQueueAFEC`          | Recebimento do valor do ADC                          |
| `xSemaphoreAfecAlarm` | Liberacao da `task_alarm` devido a condição de alarm |

A `task_adc` vai ser responsável por coletar dados de uma entrada analógica via AFEC (usando algum TC para a base de tempo), os dados devem ser enviados do *callback* do AFEC via a fila `xQueueADC` a uma taxa de uma amostra por segundo (1hz). A cada novo dado do AFEC a condicao de alarme deve ser verificada.

A task, ao receber os dados deve realizar a seguinte acao:

1. Enviar pela UART o novo valor no formato a seguir:
    - `[AFEC ] DD:MM:YYYY HH:MM:SS $VALOR` (`$VALOR` deve ser o valor lido no AFEC)
1. Verificar a condicão de alarme:
    - 15 segundos com AFEC maior que 3000
    
Caso a condicao de alarme seja atingida liberar o semáforo `xSemaphoreAfecAlarm`.

### task_event 

| Recurso                | Explicacao                                           |
|------------------------|------------------------------------------------------|
| RTC                    | Fornecer as informações do TimeStamp                 |
| PIO                    | Leitura dos botões                                   |
|------------------------|------------------------------------------------------|
| `xQueueEvent`          | Recebimento dos eventos de botão                     |
| `xSemaphoreEventAlarm` | Liberacao da `task_alarm` devido a condição de alarm |

A `task_event` será responsável por ler eventos de botão (subida, descida), para isso será necessário usar as interrupções nos botões e enviar pela fila `xQueueEvent` o ID do botão e o status (on/off). A cada evento a task deve formatar e enviar um log pela UART e também verificar a condição de alarme.

A task, ao receber os dados deve realizar a seguinte acao:

1. Enviar pela UART o novo valor no formato a seguir:
    - `[EVENT] DD:MM:YYYY HH:MM:SS $ID:$Status` (`$ID:$Status`: id do botão e status)
1. Verificar a condicão de alarme:
    - Dois botões pressionados ao mesmo tempo
    
Caso a condicao de alarme seja atingida liberar o semáforo `xSemaphoreEventAlarm`.

### task_alarm

| Recurso                | Explicacao                                 |
|------------------------|--------------------------------------------|
| PIO                    | Acionamento dos LEDs                       |
|------------------------|--------------------------------------------|
| `xSemaphoreAfecAlarm`  | Indica alarme ativado deviado a task_afec  |
| `xSemaphoreEventAlarm` | Indica alarme ativado deviado a task_event |

Responsável por gerenciar cada um dos tipos de alarme diferente: `afec` e `event`. A cada ativacão do alarme a task deve emitir um Log pela seria, O alarme vai ser um simples pisca LED, para cada um dos alarmes vamos atribuir um LED diferentes da placa OLED: 

- `AFEC`: LED1
- `EVENT`: LED2

Os alarmes são ativos pelos semáforos `xSemaphoreAfecAlarm` e `xSemaphoreEventAlarm`. Uma vez ativado o alarme o mesmo deve ficar ativo até a placa reiniciar.

Ao ativar um alarme, a `task_alarm` deve emitir um log pela serial no formato descrito a seguir:

- `[ALARM] DD:MM:YYYY HH:MM:SS $Alarm` (onde `$Alarm` indica qual alarme que foi ativo).

### Exemplo de log

A seguir um exemplo de log, no caso conseguimos verificar a leitura do AFEC e no segundo 04 o botão 1 foi pressionado,
e depois solto no segundo 05. No segundo 06 o AFEC atinge um valor maior que o limite e fica assim por mais 10 segundos, ativando
o alarme no segundo 14.

``` 
 [AFEC ] 19:03:2018 15:45:01 1220
 [AFEC ] 19:03:2018 15:45:02 1222
 [AFEC ] 19:03:2018 15:45:03 1234
 [AFEC ] 19:03:2018 15:45:04 1225
 [EVENT] 19:03:2018 15:45:04 01:PRESS
 [AFEC ] 19:03:2018 15:45:04 1245
 [AFEC ] 19:03:2018 15:45:05 1245
 [EVENT] 19:03:2018 15:45:05 01:RELEASED
 [AFEC ] 19:03:2018 15:45:06 4000
 [AFEC ] 19:03:2018 15:45:07 4004
 [AFEC ] 19:03:2018 15:45:08 4001
 
  -------- passam mais 6 segundos ------
 
 [AFEC ] 19:03:2018 15:45:14 4023
 [ALARM] 19:03:2018 15:45:14 AFEC
```

### Resumo

A seguir um resumo do que deve ser implementando:

- Leitura do AFEC via TC 1hz e envio do dado para fila `xQueueAfec`
- Leitura dos botões do OLED via IRQ e envio do dado para fila `xQueueEvent`
- `task_afec`
    - log:  `[AFEC ] DD:MM:YYYY HH:MM:SS $VALOR` 
    - alarm se valor afec maior > 3000 durante 10s
        - libera semáforo `xSemaphoreAfecAlarm`
- `task_event`
    - log:  `[EVENT] DD:MM:YYYY HH:MM:SS $ID:$STATUS` 
    - alarm se dois botões pressionados ao mesmo tempo
        - libera semáforo `xSemaphoreEventAlarm`
- `task_alarm`
    - log:  `[ALARM] DD:MM:YYYY HH:MM:SS $ALARM` 
    - Pisca led 1 dado se `xSemaphoreAfecAlarm`
    - Pisca led 2 dado se `xSemaphoreEventAlarm`

Não devem ser utilizadas variáveis globais além das filas e semáforos.

## Ganhando conceitos

- (meio conceito) desligar o alarme do AFEC se ele passar 10 segundos em um valor menor que 1000
- (meio conceito) Exibir no OLED os Logs (um por liinha)
