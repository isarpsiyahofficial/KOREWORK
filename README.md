# KOREWORK

Knight Online Fire Drake v1453 dönemini temel alan, Windows ve Linux uyumlu offline PC rework projesi.

## Mevcut durum

> **Önemli:** Repodaki mevcut küçük executable, yalnızca tek süreçte sunucusuz çalışma yaklaşımını doğrulayan teknik bir prototiptir. Fire Drake offline rework tamamlanmış değildir ve bu prototip nihai oyun olarak kabul edilmez.

Gerçek offline sürümün tamamlanmış sayılması için aşağıdakilerin oyuna aktarılması ve birlikte çalışması gerekir:

- Fire Drake haritaları, terrain, collision, warp ve spawn verileri
- Karakter modelleri, iskeletler ve animasyonlar
- Zırh, silah, kalkan ve ekipman görünüm değişimleri
- Canavar modelleri, animasyonları, AI kayıtları ve spawn tabloları
- Skill tabloları, animasyon zamanlamaları, efektler, projectile ve buff/debuff mantıkları
- Warrior, Rogue, Mage ve Priest jobları ile stat/katsayı sistemleri
- Item, drop, upgrade, envanter ve ekipman kuralları
- NPC, görev, mağaza ve etkileşim verileri
- Fire Drake dönemine uygun HUD ve tek responsive `1 2 3 4 5 6 7 8 9 0` skill bar
- Yerel karakter oluşturma, seçme, kayıt/yükleme ve haritaya giriş
- Windows ve Linux üzerinde aynı içerikle açılabilen build

Bu maddeler gerçekleşmeden proje “oynanabilir Fire Drake offline sürümü” olarak tanımlanmayacaktır.

## Hedef offline çalışma modeli

Nihai KOREWORK tek süreç olarak çalışacaktır:

- GameServer çalıştırmayacak.
- AIServer çalıştırmayacak.
- LoginServer çalıştırmayacak.
- SQL Server, PostgreSQL, MySQL veya SQLite sunucusu istemeyecek.
- `localhost` bağlantısı açmayacak.
- Socket veya ağ bağlantısı kullanmayacak.
- Karakter, harita, canavar, skill, drop ve kayıt işlemlerini aynı executable içinde yürütecek.
- Kayıt dosyalarını kullanıcının profil dizininde tutacak.

## Teknik prototipte doğrulananlar

- Tek süreçte çalışan Windows/Linux executable
- Socket, localhost ve SQL olmadan runtime
- Yerel kayıt/yükleme yaklaşımı
- Basit hareket, kamera, AI, saldırı, drop ve envanter döngüsü
- Responsive 10 slotlu `1 2 3 4 5 6 7 8 9 0` skill bar yaklaşımı

Bu maddeler yalnızca mimari doğrulamadır; Fire Drake içerik aktarımının yerine geçmez.

## Güvenlik ilkeleri

- Orijinal kaynak yapısı referans olarak değişmeden korunur.
- Hazır `.exe`, `.dll`, launcher, injector, hook veya anti-cheat bileşenleri çalıştırılmaz.
- Kaynağı olmayan binary dosyalar güvenilir kabul edilmez.
- Rework kodu temiz kaynak koddan yeniden derlenir.
- Fire Drake upstream'i oyun kuralı ve veri referansı olarak izole tutulur.

## Upstream

Fire Drake v1453 kaynağı sabitlenmiş bir Git submodule olarak `upstream/fire-drake-v1453` altında tutulmaktadır.

Sabitlenen upstream commit: `0f520272ae1f11472623d62bff76fff98562e7b3`
