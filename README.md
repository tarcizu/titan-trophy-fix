# TITAN Trophy Fix

PS5 payload that makes trophies work for games installed as PKG by [TITAN CORE](https://github.com/devchmod/TITAN-CORE).

**[English](#english)** · **[Português](#português)**

---

## English

### The problem

When TITAN CORE runs a game installed as PKG, the game's root (`/app0`) is the TITAN title cache at `/data/titan-core/caches/<ID>`. The `sce_sys` folder in that cache has no `nptitle.dat`, `trophy2/` or `uds/`. Without them, the game never calls the system trophy and UDS services:

- new games **don't show** their trophy list;
- games registered earlier **show** the list, but **nothing unlocks**.

Games installed as folders are not affected.

### What the payload does

For every game installed as PKG by TITAN, it copies whatever is missing into `/data/titan-core/caches/<ID>/sce_sys/`:

| File | Source |
|---|---|
| `nptitle.dat`, `trophy2/npbind.dat`, `uds/npbind.dat` | `/system_data/priv/appmeta/<ID>/` (the system already keeps them decrypted) |
| `trophy2/trophy00.ucp`, `uds/uds00.ucp` | plaintext entries of the installed `app.pkg` |

It needs no keys and does not modify TITAN or the PKG.

- It only touches **installed** games: the `appmeta` folder must exist and the source PKG must be accessible. Caches left behind by deleted games are skipped.
- It only writes what is missing, so running it again is harmless.
- It can run while TITAN is open. It only writes to the cache and never touches `/user/app/<ID>`.
- It shows a notification with the result. Details go to `/data/titan-core/trophyfix.log`.

### Usage

1. Run `titan_trophy_fix_vX.Y.elf` with your payload loader (elfldr on port 9021, Prospero Manager, etc.). TITAN can stay open.
2. Start the game. Trophies are registered the first time the game runs after the fix.

Run it again **every time you install a game with TITAN**. A forced cache rebuild in TITAN keeps the copied files. If a game's cache is recreated from scratch (for example, if the folder is deleted), just run the payload again.

#### Prospero Manager

Under **Payloads → Catalog**, add the source `https://github.com/tarcizu/titan-trophy-fix`. Prospero Manager lists the versions published under [Releases](https://github.com/tarcizu/titan-trophy-fix/releases) and downloads the `.elf` straight to the console.

### Building

Requires the [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk) (v0.45) and clang/lld 18.

```bash
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
make VERSION=1.0
```

With Docker:

```bash
docker build -t ps5-payload-build .
docker run --rm -v "$PWD:/src" ps5-payload-build
```

Every `vX.Y` tag pushed to the repository builds the payload with GitHub Actions and publishes a Release with `titan_trophy_fix_vX.Y.elf`. The version is part of the file name, which is how the Payload Manager displays it.

### License

GPL-3.0-or-later. Uses the [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk), also GPL-3.0-or-later.

---

## Português

### O problema

Quando o TITAN CORE abre um jogo instalado por PKG, a raiz do jogo (`/app0`) é a cache do TITAN em `/data/titan-core/caches/<ID>`. O `sce_sys` dessa cache não tem `nptitle.dat`, `trophy2/` nem `uds/`. Sem eles, o jogo nunca chama os serviços de troféus e UDS do sistema:

- jogos novos **não mostram** a lista de troféus;
- jogos que já tinham sido registrados **mostram** a lista, mas **nada desbloqueia**.

Jogos instalados em pasta não têm esse problema.

### O que o payload faz

Para cada jogo instalado por PKG pelo TITAN, ele copia para `/data/titan-core/caches/<ID>/sce_sys/` o que estiver faltando:

| Arquivo | Origem |
|---|---|
| `nptitle.dat`, `trophy2/npbind.dat`, `uds/npbind.dat` | `/system_data/priv/appmeta/<ID>/` (o sistema já os deixa descriptografados) |
| `trophy2/trophy00.ucp`, `uds/uds00.ucp` | entradas sem criptografia do `app.pkg` instalado |

Ele não usa nenhuma chave e não altera o TITAN nem o PKG.

- Só mexe em jogos **instalados**: o `appmeta` precisa existir e o PKG de origem precisa estar acessível. Caches de jogos excluídos são ignoradas.
- Só grava o que estiver faltando, então rodar mais de uma vez não faz mal.
- Pode rodar com o TITAN aberto. Ele grava só na cache, nunca em `/user/app/<ID>`.
- Mostra uma notificação com o resultado. Os detalhes ficam em `/data/titan-core/trophyfix.log`.

### Como usar

1. Execute o `titan_trophy_fix_vX.Y.elf` pelo seu carregador de payloads (elfldr na porta 9021, Prospero Manager etc.). O TITAN pode ficar aberto.
2. Abra o jogo. Os troféus são registrados na primeira vez que o jogo roda depois da correção.

Execute de novo **sempre que instalar um jogo pelo TITAN**. A reconstrução forçada da cache pelo TITAN preserva os arquivos copiados. Se a cache de um jogo for recriada do zero (por exemplo, se a pasta for apagada), basta rodar o payload outra vez.

#### Prospero Manager

Em **Payloads → Catálogo**, adicione a fonte `https://github.com/tarcizu/titan-trophy-fix`. O Prospero Manager lista as versões publicadas em [Releases](https://github.com/tarcizu/titan-trophy-fix/releases) e baixa o `.elf` direto no console.

### Compilar

É preciso o [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk) (v0.45) e clang/lld 18.

```bash
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
make VERSION=1.0
```

Com Docker:

```bash
docker build -t ps5-payload-build .
docker run --rm -v "$PWD:/src" ps5-payload-build
```

Cada tag `vX.Y` enviada ao repositório compila o payload pelo GitHub Actions e publica uma Release com o `titan_trophy_fix_vX.Y.elf`. O nome leva a versão, que é como o Payload Manager a exibe.

### Licença

GPL-3.0-or-later. Usa o [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk), também sob GPL-3.0-or-later.
