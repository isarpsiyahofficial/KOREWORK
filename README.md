# KOREWORK — Offline Fire Drake

KOREWORK, Fire Drake v1453 döneminin açık kaynak veri ve varlık yapısını tek süreçte çalışan Windows/Linux çevrimdışı PC oyununa dönüştüren bir rework projesidir.

Dağıtım paketi sunucu kurulumu istemez. `KOREWORK.exe` veya `KOREWORK` doğrudan çalıştırılır; LoginServer, GameServer, AIServer, SQL sunucusu, localhost servisi ya da ağ bağlantısı açılmaz.

## Pakette çalışan sistemler

- Üç ayrı yerel karakter slotu
- Warrior, Rogue, Mage ve Priest sınıfları
- Sınıfa özel başlangıç statları ve skill seçimi
- Karakter oluşturma, seçme, silme, kayıt ve yeniden yükleme
- Fire Drake SMD terrain, yükseklik, harita sınırı, regene ve warp verileri
- OpenKO `K_NPCPOS` kayıtlarından derlenen zone ve canavar yerleşimleri
- OpenKO monster, drop, skill ve class tablolarından derlenen SQL'siz KOPACK
- Şifreli Fire Drake `Item_Org` tablosundan gerçek item adı, açıklama, slot, görünüm, ikon, hasar, zırh, dayanıklılık, fiyat ve gereksinim verileri
- Gerçek N3 canavar ve oyuncu karakter modelleri
- N3 iskelet, idle/hareket/saldırı animasyonları ve texture çözümü
- Item görünüm kimliğiyle sağ el N3 silah bağlantısı
- Canavar takip, saldırı, ölüm, yeniden doğma, EXP ve Noah döngüsü
- Drop, envanter, 14 ekipman slotu, takma/çıkarma ve upgrade
- Stat puanı dağıtımı ve türetilmiş saldırı/savunma değerleri
- Yakın dövüş, projectile ve şifa skill efektleri
- Başlangıç görevi, tüccar, şifacı ve warp etkileşimleri
- Responsive `1 2 3 4 5 6 7 8 9 0` skill bar ve HUD
- Profil başına ayrı görev, envanter, ekipman, stat ve ilerleme kaydı

## Çalıştırma

### Windows

1. `KOREWORK-Windows-Complete.zip` arşivini tamamen çıkarın.
2. `KOREWORK-Windows/KOREWORK.exe` dosyasını çalıştırın.

### Linux

1. `KOREWORK-Linux-Complete.tar.gz` arşivini çıkarın.
2. Paket klasöründe `./KOREWORK` komutunu çalıştırın.

Executable, `data/game_data.kopack`, `data/world_spawns.kospawn` ve `assets/ko` klasörü aynı paket yapısında kalmalıdır.

## Kontroller

- `W A S D`: hareket
- `Sol Shift`: koşma
- `Sağ fare + hareket`: kamera
- `Fare tekerleği`: yakınlaştırma
- `1–0`: skill kullanma
- `F`: yakındaki NPC veya warp ile etkileşim
- `I`: envanter ve ekipman
- `E`: seçilen itemi takma
- `U`: seçilen itemi yükseltme
- `C`: karakter statları
- `F1–F5`: açık stat panelinde puan dağıtma
- `F5`: manuel kayıt
- `Esc`: paneli kapatma veya karakter seçimine dönme

## Yerel veri dosyaları

- `data/game_data.kopack`: monster, drop, skill, class ve gerçek Item_Org kayıtları
- `data/world_spawns.kospawn`: OpenKO K_NPCPOS zone ve yaratık yerleşimleri
- `assets/ko`: sabitlenmiş SMD, N3 model, skeleton, animasyon, texture ve efekt ağacı
- Kullanıcı kayıtları: kullanıcı profilindeki `.korework/saves` dizini

## Çevrimdışı çalışma ilkeleri

- Socket veya internet API'si kullanılmaz.
- `localhost` bağlantısı açılmaz.
- SQL Server, PostgreSQL, MySQL veya SQLite sunucusu çalıştırılmaz.
- Oyun, AI, drop, skill, görev ve kayıt işlemleri aynı executable içinde yürütülür.
- Kaynağı olmayan hazır executable, DLL, launcher, injector, hook veya anti-cheat bileşenleri çalıştırılmaz.
- Rework kaynak koddan yeniden derlenir; upstream kaynaklar veri ve varlık referansı olarak izole tutulur.

## Otomatik kabul kapıları

Windows ve Linux paketleri yayımlanmadan önce aşağıdaki kontroller zorunludur:

- ağ ve veritabanı API'lerinin bulunmaması
- iki platformda Release derleme ve CTest geçişi
- gerçek SMD, N3 karakter, skeleton, animasyon, texture ve equipment probe'ları
- gerçek oynanabilir N3 oyuncu karakter zinciri
- OpenKO SQL → KOPACK round-trip ve checksum doğrulaması
- OpenKO K_NPCPOS → KOSPAWN round-trip ve runtime yerleşim doğrulaması
- gerçek Item_Org kataloğu
- executable, KOPACK, KOSPAWN ve 100 MB üzeri asset ağacının dağıtım arşivinde bulunması

## Sabitlenmiş upstream kaynaklar

- Fire Drake v1453 kaynak referansı: `upstream/fire-drake-v1453`
- KO varlık ağacı: `upstream/ko-assets`
- OpenKO istemci kaynak referansı: `upstream/openko-client`
- OpenKO veri seti paketleme sırasında sabit committen derlenir.
