# VegTrack: monitoramento de vegetação com OTA

Projeto de Edge Computing, Ciências da Computação, 4º semestre.

Autores:

- Rogério Deligi Ferreira Filho, RM561942
- Maria Fernanda Garavelli Dantas, RM562686

O VegTrack simula um nó de campo do Projeto Motiva. O ESP32 coleta cinco estimativas de altura da vegetação entre 10 e 20 cm, calcula média e, no Firmware 2.0, também ordena uma cópia das leituras, calcula a mediana e aplica histerese aos estados NORMAL e ALERTA.

## Publicação

Repositório remoto: [RogerioOxy/VegTrack-Edge-CP2-OTA](https://github.com/RogerioOxy/VegTrack-Edge-CP2-OTA)

O código, `version.json` e `firmware_v2.bin` foram verificados publicamente. O manifesto e o binário retornaram HTTP 200. O binário remoto tem 1.115.792 bytes e SHA-256 `eef3c48b0a3e4a09bb63457935b54c2a59ec0b9177c0b0512550647d5b688ffd`.

Link Wokwi: **publicação pendente de login**. O endereço `https://wokwi.com/projects/475727719892918273` é o template original e não deve ser apresentado como o projeto final público. A execução que comprovou o OTA foi feita no editor não salvo, com `sketch.ino`, `diagram.json` e `partitions.csv` corretos. O projeto completo precisa ser salvo após o login antes da entrega do PDF.

Depois da entrega, os links e a estrutura devem permanecer públicos por pelo menos dez dias, conforme o enunciado. Esse período ainda não foi observado.

## Arquitetura

```text
ESP32 DevKit no Wokwi
        |
        | Wokwi-GUEST + HTTPS
        v
version.json -> firmware_v2.bin -> partição OTA inativa -> reboot
```

O manifesto possui exatamente dois campos:

```json
{
  "version": "2.0",
  "url": "https://raw.githubusercontent.com/RogerioOxy/VegTrack-Edge-CP2-OTA/main/firmware_v2.bin"
}
```

O formato adotado neste projeto tem dois campos: `version` e `url`. O parser rejeita campos extras e aceita URLs HTTPS em `raw.githubusercontent.com/RogerioOxy/`, que é a conta usada pela dupla. Essa é uma limitação deliberada desta implementação, não uma exigência geral do enunciado. Para acrescentar campos ou trocar a hospedagem, é necessário ajustar a validação e recompilar.

O Firmware 1.0 foi iniciado em `app0` e consultou o manifesto depois de três ciclos completos. O download remoto foi gravado em `app1`, validado e selecionado para o próximo boot. Após `ESP.restart()`, o Firmware 2.0 iniciou em `app1` com o mesmo MD5 do arquivo usado no download: `1547bb7841adda858261ab0b438749e2`.

## Firmware 1.0

- LED azul para identificar a versão.
- Cinco leituras pseudoaleatórias entre 10 e 20 cm.
- Leituras em 0, 2, 4, 6 e 8 segundos.
- Vetor original preservado e média exibida.
- Nova sessão iniciada a cada 48 segundos contados do início da sessão anterior.
- Após três ciclos, conexão ao `Wokwi-GUEST`, consulta HTTPS do manifesto e comparação numérica das versões.

## Firmware 2.0

- Mantém as leituras, a média e a temporização do Firmware 1.0.
- Ordena uma cópia do vetor e exibe a ordem original e a crescente.
- Usa o terceiro elemento ordenado como mediana.
- Mediana maior ou igual a 16 cm: ALERTA e LED vermelho.
- Mediana menor ou igual a 14 cm: NORMAL e LED verde.
- Entre os dois limites: mantém o estado anterior.

O diagnóstico inicial do Firmware 2.0 usa vetores fixos identificados como diagnóstico. Ele comprova média, ordenação, mediana, limites 14 e 16 e manutenção do estado em 15, sem apresentar esses vetores como leituras de campo.

## Partições, compilação e execução

`partitions.csv` contém `otadata`, `ota_0` e `ota_1`, com 0x140000 bytes por slot. O Firmware 2.0 usado no OTA tem 1.115.792 bytes e cabe no slot.

O build local usa Arduino ESP32 3.3.11, FQBN `esp32:esp32:esp32` e a tabela `default` equivalente. O compilador online do Wokwi gerou o primeiro firmware com core 3.3.7; o binário recebido por OTA foi compilado localmente com core 3.3.11. O log registra as duas versões e a atualização entre elas funcionou. Para repetir as compilações locais, use o core 3.3.11.

Para compilar localmente:

```powershell
./build.ps1
```

O script pressupõe Arduino CLI e Python 3 instalados. Em outro computador, ajuste a variável `$cli` no início de `build.ps1` para o caminho do seu Arduino CLI.

No navegador, abra o projeto Wokwi completo com o Firmware 1.0 em `sketch.ino`, o circuito em `diagram.json` e a tabela em `partitions.csv`. Clique no botão verde de iniciar a simulação. O próprio Wokwi compila o fonte; não é preciso fazer o build local para usar esse caminho. Abra o Serial Monitor a 115200 baud e aguarde os três ciclos completos, a consulta, a gravação e o reboot.

Como alternativa após o build local, `build1/fw1.ino.merged.bin` inclui bootloader e tabela de partições para carregar uma imagem inicial completa. O arquivo OTA é somente `firmware_v2.bin`, que contém a aplicação.

## Evidências

- [Log completo do OTA e do reboot](docs/evidencias/wokwi-ota-success.txt)
- [Log dos cinco cenários de erro e recuperação](docs/evidencias/wokwi-erros-e-recuperacao.txt)
- [LED azul durante a execução do Firmware 1.0](docs/evidencias/wokwi-fw1-primeiro-teste.png)
- [Firmware 2.0 em ALERTA](docs/evidencias/wokwi-fw2-alerta.png)
- [Firmware 2.0 em NORMAL](docs/evidencias/wokwi-fw2-normal.png)
- [Matriz dos testes obrigatórios](docs/TESTES.md)

Os testes locais das funções reais extraídas dos firmwares executaram 161.051 vetores, 966.335 verificações e 0 falhas. Essa evidência é distinta da execução no Wokwi: ela não substitui a comprovação de Wi-Fi, HTTPS, `Update`, reboot, LEDs ou partições.

## Diagnóstico

Após a consulta inicial terminar, os comandos do Serial Monitor exercitam os cinco cenários pedidos no enunciado:

| Comando | Cenário | Resultado observado |
|---|---|---|
| `1` | Sem Wi-Fi | `ERRO 1`, sem atualização |
| `2` | Manifesto inacessível | HTTP 404 e `ERRO 2` |
| `m` | Versão já atual | HTTP 200, `INFO 3`, nenhuma gravação |
| `4` | Binário indisponível | HTTP 404 e `ERRO 4` |
| `5` | Imagem inválida | `ERRO 5`, código 13, `Decryption error`, `app1` preservada |

O comando `5` não é um OTA válido. Ele verifica que o caminho de erro rejeita o conteúdo e que o firmware em execução permanece recuperável.
