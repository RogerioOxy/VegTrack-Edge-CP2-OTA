# Testes e evidências do CP2

## Resultado da execução principal

O log [wokwi-ota-success.txt](evidencias/wokwi-ota-success.txt) comprova a execução real no Wokwi com os três arquivos corretos no editor: `sketch.ino`, `diagram.json` e `partitions.csv`.

| Teste | Critério | Evidência observada | Status |
|---:|---|---|---|
| 1 | Firmware 1.0 com cinco leituras, média e LED azul | FW 1.0 iniciou em `app0`; cinco leituras, média e `LED azul` no log; captura em [wokwi-fw1-primeiro-teste.png](evidencias/wokwi-fw1-primeiro-teste.png) | PASS |
| 2 | Nova sessão em 48 s, e não em 56 s | Inícios em 0, 48000, 96000 e 144000 ms; leituras em 0, 2000, 4000, 6000 e 8000 ms | PASS |
| 3 | Manifesto indica 2.0 após três ciclos | Ciclo 3 em 104001 ms; HTTPS do manifesto HTTP 200; versão instalada 1.0 e disponível 2.0 | PASS |
| 4 | OTA baixa, grava, reinicia e executa FW 2.0 | Firmware HTTP 200 com 1.115.792 bytes; gravação em `app1` 0x150000; imagem validada; reboot; FW 2.0 em `app1` com MD5 `1547bb7841adda858261ab0b438749e2` | PASS |
| 5 | FW 2.0 exibe média, ordem e mediana | Diagnóstico e leituras de campo exibem vetor original, ordem crescente, média e `Mediana (3º valor)` | PASS |
| 6 | Mediana >= 16 gera ALERTA e vermelho | Diagnóstico do limite 16 e leitura de campo com mediana 17: `ALERTA; LED vermelho`; captura em [wokwi-fw2-alerta.png](evidencias/wokwi-fw2-alerta.png) | PASS |
| 7 | 14 < mediana < 16 mantém estado | Diagnóstico com mediana 15 conserva NORMAL e ALERTA conforme o estado anterior | PASS |
| 8 | Mediana <= 14 gera NORMAL e verde | Diagnóstico com mediana 14 e 12; leitura de campo com mediana 12: `NORMAL; LED verde`; captura em [wokwi-fw2-normal.png](evidencias/wokwi-fw2-normal.png) | PASS |

## Cinco cenários de erro

Todos os cenários foram executados no mesmo runtime e estão no log [wokwi-erros-e-recuperacao.txt](evidencias/wokwi-erros-e-recuperacao.txt).

| Cenário | Procedimento | Resultado literal | Status |
|---|---|---|---|
| Sem Wi-Fi | Comando `1`, desconexão antes da consulta | `ERRO 1: sem conexão Wi-Fi. Diagnóstico após desconectar a rede.` | PASS |
| Manifesto inacessível | Comando `2`, URL de manifesto inexistente | `Manifesto HTTP=404 bytes=14` e `ERRO 2: manifesto inacessível ou tamanho inválido.` | PASS |
| Versão já atual | Comando `m` no FW 2.0 | `Versão instalada=2.0 disponível=2.0` e `INFO 3: versão instalada já é a mais recente; nenhuma gravação.` | PASS |
| Binário indisponível | Comando `4`, URL de binário inexistente | `Firmware HTTP=404 bytes=14` e `ERRO 4: download indisponível ou tamanho não informado.` | PASS |
| Falha na atualização | Comando `5`, manifesto usado como imagem | `ERRO 5: gravação OTA falhou; código=13` e `Decryption error`; `app1` permaneceu com MD5 `1547bb7841adda858261ab0b438749e2` | PASS |

## Verificação local complementar

O teste host em `tests` executou as funções extraídas diretamente dos firmwares em todos os 161.051 vetores inteiros possíveis. A saída observada foi:

```text
Actual extracted firmware functions: 161051 vectors, 966335 checks, 0 failures
```

Esse teste confirma ordenação, média, mediana, histerese, versões e limites. Ele não substitui o runtime Wokwi e não testa Wi-Fi, HTTPS, `Update`, partições ou LEDs.

## Observações de reprodução

- O runtime de sucesso foi feito no editor Wokwi não salvo. O projeto final ainda precisa ser salvo após login para obter um link público próprio.
- O endereço `475727719892918273` é somente o template original e não deve ser usado como link de entrega.
- A primeira tentativa online usou a tabela factory do projeto inicial. Ela recebeu HTTP 200 e o tamanho declarado do binário, mas parou antes de iniciar a transferência e a gravação. O reteste com `partitions.csv` comprovou todos os bytes gravados em `app1` e o reboot.
- O primeiro boot registrado foi compilado online com core 3.3.7; o FW 2.0 baixado foi compilado localmente com core 3.3.11. O botão de iniciar do Wokwi utiliza o compilador online. Para repetir os builds locais, use core 3.3.11.
- A manutenção dos links por dez dias após a entrega ainda é uma obrigação futura, não uma observação já concluída.
