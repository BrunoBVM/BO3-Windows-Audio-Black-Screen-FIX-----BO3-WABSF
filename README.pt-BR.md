# BO3 - Windows Audio Black Screen FIX

**Nome curto: BO3-WABSF**

**Alfa (v0.0.2a)** · por [BrunoBVM](https://github.com/BrunoBVM) · licença MIT

🇬🇧 [Read in English](README.md)

Um possível contorno para a tela preta e o travamento na inicialização do **Call of Duty: Black Ops III (Steam, PC)** quando o Windows tem muitos dispositivos de áudio (por exemplo, dispositivos virtuais do VoiceMeeter ou do VB-Cable). Ele limita quantos dispositivos o jogo enxerga. Não altera nenhuma configuração de áudio do Windows.

> **Situação:** funciona na máquina do autor. Não foi verificado em nenhum outro lugar. Sem qualquer vínculo com Activision, Treyarch, Valve ou VB-Audio. Use por sua conta e risco e leia antes a seção [Segurança e anti-cheat](#segurança-e-anti-cheat).

## O problema

Com muitos dispositivos de áudio ativos, o jogo abre, toca o áudio do vídeo de introdução, mostra uma tela preta e para de responder quando o vídeo termina. Desativar dispositivos de áudio no Windows é o contorno conhecido, mas nem sempre isso é possível, por exemplo em uma configuração de stream com dois PCs, em que os dispositivos virtuais fazem o roteamento do áudio.

## O que isto faz

Uma pequena `dsound.dll` x64 colocada ao lado do `BlackOps3.exe`. O jogo importa o DirectSound, então o Windows carrega esta cópia primeiro. Ela:

1. repassa todas as chamadas do DirectSound para a `dsound.dll` verdadeira e registra as chamadas em `dsound_proxy.log`;
2. altera os slots de importação do próprio jogo para três funções do `winmm`, `mixerGetNumDevs`, `waveOutGetNumDevs` e `waveInGetNumDevs`, para que nunca informem mais de N dispositivos (padrão: 16).

Nada no Windows é modificado, e os outros programas continuam vendo todos os seus dispositivos.

## O que foi descoberto (uma única máquina, trate como uma pista, não como prova)

- Uma pilha de chamadas do Process Monitor mostrou o `BlackOps3.exe` chamando `winmmbase!mixerGetNumDevs`, o que faz o `MMDevAPI` percorrer todos os dispositivos de áudio no registro, inclusive os não presentes.
- Um dump do processo travado mostrou a thread principal em um laço de Sleep e nenhuma thread bloqueada dentro de uma chamada de áudio.
- Depois do vídeo de introdução, o jogo relia um banco de sons (`core_patch.all.sabs`) em blocos de 256 KB, cerca de um bloco a cada 1,5 segundo, por minutos, sem terminar.
- Com o clamp ativo, o jogo iniciou normalmente. O log mostrou:
  `mixerGetNumDevs real=28 -> returned 16` e `waveInGetNumDevs real=13 -> returned 13`.
  Só a contagem do mixer foi realmente reduzida, então essa chamada parece ser a que importa.
- **Desconhecido:** o limite exato e o que o jogo faz internamente com essa contagem.

## Instalação

### Opção A: baixar a DLL pronta (mais fácil)

1. Baixe a `dsound.dll` na [página de Releases](https://github.com/BrunoBVM/BO3-WABSF/releases). As versões são marcadas como pré-lançamento (alfa).
2. Recomendado: confira o arquivo com o SHA-256 informado nas notas da versão. Para a v0.0.2a ele é `971001F82716D9739C58EE3FE451CAB3E3565AA04492E6553FDA5871033FAC88`. No PowerShell:
   ```
   Get-FileHash .\dsound.dll -Algorithm SHA256
   ```
3. Copie a `dsound.dll` para a pasta do Black Ops III, ao lado do `BlackOps3.exe`.

A DLL não é assinada e foi compilada pelo autor com MinGW-w64. Uma DLL na pasta do jogo roda com as permissões do jogo, e alguns antivírus sinalizam DLLs proxy. Se preferir não confiar em um binário pronto, use a Opção B.

### Opção B: compilar a partir do código-fonte

MinGW-w64 (x64): execute `build-mingw.cmd`, ou

```
g++ -shared -O2 -static -o dsound.dll src/dsound.cpp src/dsound.def
```

MSVC (Prompt de Comando x64 Native Tools):

```
cl /LD /O2 /EHsc src\dsound.cpp /link /DEF:src\dsound.def /OUT:dsound.dll
```

O jogo é de 64 bits, então compile uma DLL de 64 bits. Depois copie a `dsound.dll` para ao lado do `BlackOps3.exe`.

#### Instalando um compilador (escolha um)

**MinGW-w64 (download menor, recomendado)**

1. Abra o PowerShell e execute:
   ```
   winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
   ```
   Se o winget não encontrar esse ID, execute `winget search winlibs` e use o ID que ele listar.
2. Feche o terminal e abra um novo, para que o `g++` esteja no PATH.
3. Confira se funciona e se é de 64 bits:
   ```
   g++ --version
   g++ -dumpmachine
   ```
   O segundo comando deve mostrar `x86_64-w64-mingw32`. Se mostrar `i686`, você instalou um toolchain de 32 bits e a DLL não vai carregar no jogo.

Alternativa: instale o [MSYS2](https://www.msys2.org/), abra o terminal "MSYS2 UCRT64", execute `pacman -S mingw-w64-ucrt-x86_64-gcc` e adicione `C:\msys64\ucrt64\bin` ao PATH.

**MSVC (Visual Studio Build Tools, download de vários gigabytes)**

1. Baixe o "Build Tools for Visual Studio" na [página de downloads do Visual Studio](https://visualstudio.microsoft.com/downloads/) (em "Tools for Visual Studio").
2. No instalador, marque a carga de trabalho **Desenvolvimento para desktop com C++** e instale.
3. Abra o **x64 Native Tools Command Prompt for VS** pelo menu Iniciar. Um terminal comum mostrará `'cl' não é reconhecido`.
4. Entre na pasta do projeto com `cd` e execute o comando `cl` mostrado acima.

### Depois de instalar

1. Opcional: crie um `dsound_proxy.cfg` na pasta do jogo contendo um único número, o máximo de dispositivos a informar (padrão 16, `0` informa nenhum).
2. Inicie o jogo. Procure em `dsound_proxy.log` uma linha como `clamp cap=16; import slots patched: ...`. Se todas as contagens de slots alterados forem 0, o jogo não chama essas funções pela tabela de importação na sua configuração, e esta abordagem não vai ajudar.
3. Para remover, apague `dsound.dll`, `dsound_proxy.cfg` e `dsound_proxy.log`.

O `scripts/launch-bo3.ps1` instala o proxy apenas durante uma sessão e o remove quando o jogo fecha. Por padrão ele procura a `dsound.dll` na raiz do repositório; use `-ProxyDll <caminho>` para usar o arquivo baixado. Ele foi testado só de forma básica, então leia o script antes de executar.

## Segurança e anti-cheat

Isto não é um cheat e não mexe na jogabilidade, mas coloca uma DLL na pasta do jogo e altera a tabela de importação do jogo na memória. Trate isso como um risco real.

- O Black Ops III tem seu próprio anti-cheat da Treyarch (TAC). A [Política de Segurança e Aplicação](https://support.activision.com/call-of-duty--black-ops-iii/articles/call-of-duty-black-ops-iii-security-enforcement-policy) publicada pela Treyarch pune o uso de software de terceiros não autorizado para obter vantagem ou manipular estatísticas ou dados do jogo. Ela cita injetores e "qualquer outro software usado para modificar deliberadamente dados do jogo no disco ou na memória" como exemplos. Este projeto não faz nenhuma das primeiras coisas, mas, pela letra dessa última frase, um patch em memória poderia ser interpretado como incluído, e a Treyarch não publica como o TAC detecta as coisas.
- As fontes divergem sobre se o Valve Anti-Cheat também se aplica ao BO3 na Steam. Um espelho de terceiros da página da loja Steam mostra "Valve Anti-Cheat enabled", enquanto posts em fóruns descrevem o TAC como o sistema ativo. O [FAQ do VAC](https://help.steampowered.com/faqs/view/647C-5CC1-7EA9-3C29) da própria Valve diz que os banimentos do VAC vêm de conectar a servidores protegidos pelo VAC enquanto se usa software de cheat.
- **A opinião do autor:** a Activision parece não se importar mais muito com este jogo. O anti-cheat é antigo, há mods e cheaters por toda parte e muitas falhas de segurança nunca foram corrigidas, então as pessoas provavelmente estão seguras ao usar isto. Isso é uma opinião, não uma garantia. A política acima continua valendo, as punições são definitivas e ninguém fora da Treyarch sabe o que o TAC detecta.
- Este projeto é pensado para jogo offline, solo e em LAN. Se precisar de certeza, consulte o Suporte da Activision antes de usar.
- O proxy apenas adiciona arquivos à pasta do jogo; ele não modifica nenhum arquivo existente do jogo.

## Relatando resultados

Informações úteis se você testar: a versão do seu Windows, as linhas `real=` do `dsound_proxy.log`, o número de dispositivos de reprodução e gravação ativos e se o jogo chegou ao menu. Limites diferentes (por exemplo 24 ou 28) ajudariam a encontrar onde fica o limiar.

## Versões

Veja o [CHANGELOG.pt-BR.md](CHANGELOG.pt-BR.md). Versão atual: **v0.0.2a (alfa)**.

## Licença

MIT, copyright (c) 2026 BrunoBVM. Veja `LICENSE`.
