# VegTrack: monitoramento de vegetação com OTA

Projeto de Edge Computing, Ciências da Computação, 4º semestre.

- Rogério Deligi Ferreira Filho - RM561942
- Maria Fernanda Garavelli Dantas - RM562686

O VegTrack representa um nó de campo do Projeto Motiva. O ESP32 simula a altura da vegetação entre 10 e 20 cm. A atualização OTA permite acrescentar funções sem acessar fisicamente o equipamento.

Projeto Wokwi: https://wokwi.com/projects/475727719892918273

Repositório: https://github.com/RogerioOxy/VegTrack-Edge-CP2-OTA

## Funcionamento

O Firmware 1.0 acende o LED azul. Cada sessão coleta cinco valores pseudoaleatórios, em 0, 2, 4, 6 e 8 segundos, armazena o vetor e mostra a média. O início seguinte é agendado somando 48 segundos ao início anterior. Assim, o intervalo não se torna 56 segundos. O Serial informa tempos reais e previstos, em milissegundos; pequenas diferenças de execução do simulador podem aparecer.

Após os três primeiros ciclos completos, aproximadamente 104 segundos depois da primeira sessão, uma tarefa conecta o ESP32 à Wokwi-GUEST. Ela sincroniza o relógio, consulta o manifesto por HTTPS, compara os dois componentes numéricos da versão e baixa o binário quando há versão mais nova. O processo usa `Update.begin`, `Update.write` e `Update.end`; apenas uma imagem validada seleciona a partição do próximo boot. Em seguida, `ESP.restart()` reinicia o dispositivo. A tarefa de rede separada mantém a coleta ativa enquanto a rede espera.

O Firmware 2.0 mantém as leituras, a média e o período. Ordena uma cópia usando inserção e usa seu terceiro elemento como mediana. A ordem original permanece disponível. Mediana maior ou igual a 16 cm leva ao estado ALERTA e LED vermelho; mediana menor ou igual a 14 cm leva ao estado NORMAL e LED verde. Entre esses limites, o estado anterior permanece. Essa regra segue a tabela formal do enunciado, que é mais precisa que seu exemplo contraditório.

A média considera todos os valores. A mediana é o valor central ordenado, menos afetado por um valor extremo. A histerese usa dois limites e o estado anterior para evitar alternâncias próximas do limite.

O FW2 começa com um diagnóstico identificado como tal, usando vetores fixos e as mesmas funções da coleta. Ele mostra o exemplo do enunciado, a manutenção em 15 cm nos dois sentidos, o retorno em 14 cm e a entrada em 16 cm. Os LEDs aparecem por 500 ms em cada caso. Ao fim, o estado NORMAL é restaurado, e só então começa a sessão zero. Esses vetores não são apresentados como leituras aleatórias.

## Circuito e arquitetura

ESP32 DevKit -> rede Wokwi-GUEST -> manifesto version.json -> binário firmware_v2.bin -> partição OTA inativa -> reboot.

Há três LEDs, cada um com resistor de 220 ohms: vermelho no GPIO25, verde no GPIO26 e azul no GPIO27; os cátodos se conectam ao GND. Não há sensor físico. `diagram.json` contém o circuito.

O HTTPS verifica o servidor usando a coleção completa de CAs incorporada ao core ESP32 3.3.11. O relógio é sincronizado por NTP para validar datas dos certificados. Uma falha de conexão, relógio ou certificado interrompe a consulta e é informada. Não há desativação da validação TLS.

## Arquivos e compilação

- `firmware_v1.ino` e `firmware_v2.ino`: fontes completas, independentes de cabeçalhos do projeto, também presentes nas pastas Arduino `fw1` e `fw2`.
- `firmware_v1.bin`: imagem de aplicação inicial.
- `firmware_v2.bin`: imagem de aplicação usada no download OTA.
- `version.json`: manifesto com `version` e `url`.
- `partitions.csv`: tabela com otadata e dois slots OTA, necessária no projeto Wokwi online; a tabela padrão online observada tinha factory e não permitiu iniciar OTA.
- `build.ps1`: compila ambas com o core instalado, copia os binários e mostra tamanho e SHA-256.
- `build1/fw1.ino.merged.bin`: imagem inicial completa com bootloader e tabela de partições, produzida pelo build local. Não é o arquivo de OTA.
- `wokwi.toml`: configuração para a extensão Wokwi local após a compilação.
- `tests`: teste C++ das funções extraídas diretamente do firmware.

Ambiente local: Arduino ESP32 3.3.11, FQBN `esp32:esp32:esp32`, tabela `default`. Os dois slots OTA têm 0x140000 bytes (1.310.720 bytes) cada, além de `otadata`. O boot imprime a versão do core, partição, tamanho e MD5 da imagem em execução. O SHA-256 dos arquivos distribuídos está em `ARTIFACTS.json`.

Em PowerShell, execute `./build.ps1`. O script requer Python e pressupõe o Arduino CLI instalado no caminho indicado em seu início. Em outro computador, ajuste apenas esse caminho e instale o mesmo core antes de compilar. Para compilar manualmente: `arduino-cli compile --fqbn esp32:esp32:esp32 --build-property "build.partitions=default" --output-dir build1 fw1`; repita com build2 e fw2.

## Execução e observação

1. Verifique se `version.json` e `firmware_v2.bin` estão públicos no repositório. O manifesto aponta para o arquivo `.bin` de aplicação, não para a imagem merged.
2. Inicie o projeto Wokwi com o Firmware 1.0 e abra o Serial Monitor a 115200 baud. Para uma imagem compilada localmente, use F1 no editor e a opção de carregar firmware; selecione a imagem inicial completa `build1/fw1.ino.merged.bin` para manter a tabela de partições do build.
3. Observe o azul, cinco leituras e média. Compare os inícios previstos 0, 48000 e 96000 ms. A terceira quinta leitura é prevista em 104000 ms.
4. Registre a versão do core, consulta, HTTP 200, versão 2.0, URL, bytes gravados e mensagem de reinício. Depois do reboot, registre FW2, partição diferente, diagnóstico e uma sessão aleatória completa com original, crescente, média, mediana e estado.
5. Preserve o log real. Compilar, ter um link ou mostrar mensagens preparadas não comprova atualização remota.

## Diagnóstico das cinco situações

Após três ciclos completos e quando a tarefa anterior terminar, envie um comando pelo Serial Monitor. Aguarde a mensagem de encerramento antes do próximo comando. A coleta continua; esses comandos são identificados como diagnóstico.

| Situação | Comando/procedimento | Evidência esperada | Estado da verificação Wokwi |
|---|---|---|---|
| Sem Wi-Fi | `1` desconecta a rede e verifica o estado real | ERRO 1 | Não verificado nesta etapa |
| Manifesto inacessível | `2` solicita caminho inexistente | HTTP 404 e ERRO 2 | Não verificado nesta etapa |
| Versão já atual | `m` executando FW2 com manifesto 2.0 | INFO 3, sem gravação | Não verificado nesta etapa |
| Binário indisponível | `4` solicita binário inexistente | HTTP 404 e ERRO 4 | Não verificado nesta etapa |
| Falha na atualização | `5` baixa o JSON como imagem inválida | ERRO 5 da biblioteca Update, sem reboot | Não verificado nesta etapa |

`m` repete a consulta normal, reconectando a rede se necessário. `p` mostra a partição e a identidade da imagem a qualquer momento. O comando 5 exercita uma rejeição real da biblioteca; não é uma atualização válida nem prova de OTA concluída.

## Testes obrigatórios e evidência disponível

O teste local C++ requer Python e Visual Studio 2022 com MSVC. Ele usa MSVC e extrai as funções do firmware antes de cada execução. Executa todos os 161.051 vetores possíveis, comparando a ordenação por inserção a `std::sort`. Saída observada: `Actual extracted firmware functions: 161051 vectors, 966335 checks, 0 failures`. Também verifica versões inválidas, comparação numérica e limites. Rode `./run-host-tests.cmd` a partir da pasta `tests`. Este teste não executa Wi-Fi, cJSON, Update, LEDs nem o simulador.

| # | Teste exigido | Evidência local | Situação Wokwi |
|---|---|---|---|
| 1 | FW1: cinco leituras, média, LED azul | Código compilado e média verificada localmente | Serial observado: 5 valores e média correta; aceitação visual separada |
| 2 | Sessões a cada 48 s, não 56 s | Agendamento por referência anterior | Observado: inícios em 0, 48000, 96000 e 144000 ms |
| 3 | Encontrar FW2 após três ciclos | Fluxo HTTPS e comparação implementados | Observado: ciclo 3 em 104001 ms; manifesto 200 e versão 2.0 |
| 4 | OTA, reboot e execução de FW2 | APIs reais de gravação e prova de partição implementadas | Primeiro teste baixou 1115792 bytes; bloqueado antes da gravação pela tabela online inicial. Reteste necessário com partitions.csv |
| 5 | FW2: média, ordenação, mediana | Todos os vetores inteiros possíveis passaram no teste local | Não verificado nesta etapa |
| 6 | Mediana >= 16: ALERTA, vermelho | Função e fronteira 16 passaram; diagnóstico usa LED real | Não verificado nesta etapa |
| 7 | 14 < mediana < 16: manter estado | 15 nos dois estados passou no teste local | Não verificado nesta etapa |
| 8 | Mediana <= 14: NORMAL, verde | Retorno ALERTA -> NORMAL em 14 passou localmente | Não verificado nesta etapa |

A primeira execução online usou core 3.3.7, diferente do core local 3.3.11, e partição factory. Ela confirmou rede, TLS, manifesto e download, mas não iniciou a gravação. O reteste deve usar a tabela OTA em partitions.csv.

A validação final depende do log da execução Wokwi. Os tempos de rede, o boot da partição gravada e a disponibilidade pública ainda precisam ser observados no ambiente final. Mantenha o projeto, o manifesto e o binário publicados por pelo menos dez dias após a entrega. A entrega acadêmica é o PDF indicado no enunciado.
