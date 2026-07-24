# KOREWORK Offline Architecture

## Kesin çalışma ilkesi

KOREWORK oyun çalışırken tek işletim sistemi süreci kullanır. İstemci ile oyun kuralları arasında TCP, UDP, WebSocket, named pipe, localhost veya ayrı servis bulunmaz.

```text
KOREWORK executable
├── Render / input / responsive UI
├── OfflineRuntime
│   ├── Player state
│   ├── Monster AI
│   ├── Skill engine
│   ├── Combat engine
│   ├── Drop engine
│   ├── Inventory
│   └── Save/load
└── Local immutable game data
```

## Olmayan bağımlılıklar

- GameServer
- AIServer
- LoginServer
- SQL Server
- ODBC
- SQLite servis süreci
- localhost
- internet bağlantısı
- launcher
- injector/hook
- anti-cheat binary

## Save modeli

Oyuncu kaydı işletim sisteminin kullanıcı profilinde tutulur:

- Windows: `%APPDATA%/.korework/saves/offline_profile.kosave`
- Linux: `$HOME/.korework/saves/offline_profile.kosave`

Save sürüm başlığı içerir. Gelecekte format değişiklikleri migration ile yapılacaktır.

## Fire Drake veri aktarımı

Fire Drake v1453 upstream'i doğrudan çalıştırılmaz. Aşağıdaki veriler temiz ve platform bağımsız KOREWORK şemalarına dönüştürülecektir:

- `K_MONSTER` -> `MonsterTemplate`
- `ITEM` -> item template/presentation
- `MAGIC` ve skill tipleri -> `SkillDefinition` + effects
- Drop tabloları -> deterministic local drop table
- Map spawn kayıtları -> offline world spawn chunks

İlk çekirdek, gerçek `K_MONSTER` Kecoon ailesinin adlarını ve temel statlarını kullanır.

## Responsive skill bar

Ana bar tek satırda 10 slottur:

```text
[1] [2] [3] [4] [5] [6] [7] [8] [9] [0]
```

Slot boyutu ekran genişliğine göre sınırlandırılmış şekilde ölçeklenir. Bar bottom-center anchor kullanır ve pencere yeniden boyutlandırıldığında konumunu korur.

## Platform hedefi

Aynı C++20 kaynak kodu:

- Windows x64
- Linux x64

için derlenir. Platform farkları yalnızca kullanıcı kayıt dizininin seçiminde kullanılır; oyun mantığı ortak kalır.

## Güvenlik kapısı

`scripts/verify_offline_invariant.py`, runtime kaynaklarında socket ve veri tabanı API kullanımını reddeder. GitHub Actions bu kontrol geçmeden paket üretmez.
