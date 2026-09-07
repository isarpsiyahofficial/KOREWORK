# Premium Plus Combo — Mimari ve Koruma Notları

## 1. Temel mimari

Uygulama native Win32 C++ tek-EXE mimarisindedir. Input üretimi native Windows input katmanından geçer. Çalışan modüllerin aynı anda tuş göndermesi için FIFO gate yaklaşımı kullanılır; amaç Minor/Cure/Attack/MOB/Warrior komutlarının birbirini rastgele ezmesini önlemektir.

## 2. Bilerek kullanılmayan yaklaşımlar

Bu proje çizgisinde şu yöntemler kullanılmaz:
- DLL injection,
- process memory manipulation,
- packet manipulation,
- anti-cheat bypass,
- server cooldown/stat bypass,
- wallhack/speedhack benzeri istemci kural aşımı.

MOB/Warrior tarafındaki çalışma ekran görüntüsü ve normal kullanıcı input otomasyonuyla sınırlandırılmıştır.

## 3. Input sıralaması

- Bar/slot skillleri mevcut input transport'tan geçer.
- Uzun `Sleep()` süreleri FIFO lock altında tutulmamalıdır.
- MOB R chase yalnız confirmed target sonrası.
- MOB skill yalnız confirmed target sonrası.
- Warrior equip yalnız sağ tık; drag/left-click yolu yasaktır.

## 4. Görüntü algılama ilkeleri

### Inventory
- 28 slot = 7×4 grid.
- Kalibrasyon yardımcı hız/doğrulama verisidir; tek güven kaynağı değildir.
- Slot geometry doğrudan rastgele crop'u 7×4'e bölerek tahmin edilmez; doğrulanmış grid üzerinden hesaplanır.

### Battle Cry
- Kullanıcı buff/scroll bölgesini tanıtır.
- Skill bar bölgesi kalibrasyon alanına dahil edilmemelidir.
- Cast komutu başarı değildir; buff ikonunun gerçekten oluşması gerekir.

### MOB
- User en fazla 7 target kaydı tutabilir.
- Görsel kayıt için mob gövdesi yerine kırmızı nameplate tercih edilir.
- Camera angle / model animation nedeniyle gövde template'i karar verici güven sinyali değildir.
- Görsel candidate yalnız adaydır.
- Attack authorization: target HP + target header doğrulaması.
- Mouse başarısızlığı halinde bounded Z fallback kullanılabilir; Z de yalnız aday seçim mekanizmasıdır.

## 5. İsim eşleşmesi hakkında önemli teknik not

UI'daki `Tam isim` seçeneği v4.8.24/v4.8.25 çizgisinde gerçek genel amaçlı OCR motoru değildir. Kullanıcının yazdığı isim normalize edilerek kayıt/kimlik yönetiminde tutulur; görsel nameplate maskesi tam veya kaydırmalı/subwindow görsel eşleşme modunu belirler.

Bu nedenle raporlarda `OCR ile mob adı kesin okundu` iddiası yapılmamalıdır. Gerçek OCR daha sonra eklenirse ayrı modül ve ayrı test gerektirir.

## 6. Range

Range dünya koordinat modeli X/Z Öklid mesafesi olarak ele alınır:

`distance = sqrt((x-anchorX)^2 + (z-anchorZ)^2)`

Kullanıcı önce koordinat alanını tanıtır ve `FARM MERKEZİNİ KAYDET` ile merkez kaydeder. Koordinat güvenilir okunamıyorsa range kontrolü fail-closed davranmalıdır; ekran pikseli veya R/W basma süresinden sahte dünya mesafesi üretilmez.

## 7. Responsive UI

v4.8.25'te yalnız fontu DPI ile büyütmek yerine tüm child HWND'ler oluşturulurken logical `x/y/w/h` değerleri kaydedilir. Minimum logical client 760×620'dir. Daha büyük pencere/DPI durumunda child rectangle'lar client X/Y oranlarıyla yeniden `MoveWindow` edilir. Sidebar/panel/title boyaması aynı scale değişkenlerini kullanır.

Bu yaklaşım mevcut `MoveWindow/GetClientRect` yüzeyini kullanır; binary import setine yeni Windows API eklemez.

## 8. Binary / antivirus regresyon koruması

v4.8.11 known-good üretim şekli referanstır. Her final release'te:
- v4.8.11 control EXE aynı compile/link ailesiyle üretilir,
- final EXE import seti kontrol ile karşılaştırılır,
- extra/missing import varsa build reddedilir,
- section count ve DLL characteristics kontrol edilir.

Defender diagnostic yalnız gerçekten başarılı bir taramada `NO_THREATS` olarak raporlanır. Runner servisi unavailable/skipped ise açıkça bu şekilde yazılır.

## 9. Yedekleme ilkesi

Canlı olarak denenmiş veya final doğrulanmış her önemli sürümden önce ayrı backup branch bırakılır. v4.8.25 çalışmasından hemen önceki v4.8.24 backup:

`backup/premiumplus-v4824-before-live-mob-ui-fix-20260905`

Bu sayede yeni MOB/UI düzeltmesi başarısız olsa bile v4.8.24 kaynak durumu kaybolmaz.
