# KOREWORK

Knight Online Fire Drake v1453 dönemini temel alan, Windows ve Linux uyumlu offline PC rework projesi.

## Offline çalışma modeli

KOREWORK tek süreç olarak çalışır:

- GameServer çalıştırmaz.
- AIServer çalıştırmaz.
- LoginServer çalıştırmaz.
- SQL Server, PostgreSQL, MySQL veya SQLite sunucusu istemez.
- `localhost` bağlantısı açmaz.
- Socket veya ağ bağlantısı kullanmaz.
- Karakter, canavar, skill, drop ve kayıt işlemlerini aynı executable içinde yürütür.
- Kayıt dosyasını kullanıcının profil dizininde `.korework/saves/offline_profile.kosave` altında tutar.

İnternet yalnızca kaynak kod derlenirken raylib bağımlılığını almak için kullanılır. Oluşturulan oyun executable'ı çevrimdışı çalışır.

## İlk oynanabilir çekirdek

- Yeniden boyutlandırılabilir 3D Windows/Linux istemcisi
- WASD hareket ve üçüncü şahıs kamera
- OpenKO/Fire Drake `K_MONSTER` kayıtlarından Kecoon ailesi
- Canavar takip, saldırı, ölüm ve respawn AI'sı
- HP, MP, EXP, level ve Noah
- Skill hasarı, mana maliyeti ve cooldown
- Tek responsive skill bar: `1 2 3 4 5 6 7 8 9 0`
- Drop ve envanter
- Otomatik yerel kayıt/yükleme
- Responsive HUD ve minimap

## Kontroller

- `W A S D`: hareket
- `Sol Shift`: koşu
- `Sağ mouse`: kamera
- `Mouse tekerleği`: yakınlaştırma
- `1–9, 0`: skill kullanımı
- `I`: envanter
- `F5`: kaydet

## Derleme

### Windows

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Çıktı: `build/bin/Release/KOREWORK.exe`

### Linux

Gerekli geliştirme paketleri kurulduktan sonra:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Çıktı: `build/bin/KOREWORK`

GitHub Actions her değişiklikte Windows ve Linux paketlerini otomatik üretir.

## Güvenlik ilkeleri

- Orijinal kaynak yapısı referans olarak değişmeden korunur.
- Hazır `.exe`, `.dll`, launcher, injector, hook veya anti-cheat bileşenleri çalıştırılmaz.
- Kaynağı olmayan binary dosyalar güvenilir kabul edilmez.
- Rework kodu temiz kaynak koddan yeniden derlenir.
- Fire Drake upstream'i oyun kuralı ve veri referansı olarak izole tutulur.

## Upstream

Fire Drake v1453 kaynağı sabitlenmiş bir Git submodule olarak `upstream/fire-drake-v1453` altında tutulmaktadır.

Sabitlenen upstream commit: `0f520272ae1f11472623d62bff76fff98562e7b3`
