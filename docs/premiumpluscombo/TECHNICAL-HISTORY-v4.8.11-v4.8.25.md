# Premium Plus Combo — Teknik Gelişim Geçmişi (v4.8.11 → v4.8.25)

> Son güncelleme: 7 Eylül 2026
>
> Bu belge repository'de izlenebilir teknik durumu, kullanıcı canlı test geri bildirimlerini ve bunlara karşı yapılan değişiklikleri kaydeder. CI testi ile gerçek Knight Online istemcisindeki canlı test aynı değildir; ikisi özellikle ayrı belirtilir.

## 1. Değişmez temel / known-good referans

### v4.8.11
- Exact kaynak: `premium_plus_combo_final/workbench/makro2_v4811_exact.cpp`
- Exact kaynak SHA-256: `3AECB38864D0C248F926C9F15A8FF7F5BE4A1636ACAB966ADC40DA350C142A42`
- Known-good EXE SHA-256: `E9B5C7ECD2933EEA64EF096494CF428E2D7F410D43CB32628C7459640B85C936`
- Known-good EXE boyutu: 419,328 byte.
- PE section sayısı: 7.
- PE DLL characteristics: `0x8160`.
- Eski regresyon paketi: `180/180 PASS`.
- Bu sürüm sonraki tüm çalışmalar için çalışan çekirdeği koruma referansıdır.

Korunan ana çalışma kavramları: native scan-code `SendInput`, FIFO input sıralaması, Minor, Cure, R, W/S, Attack, Vitals ve daha sonra eklenen modüllerin birbirini bloke etmemesi.

## 2. v4.8.13 — Dinamik MOB ATTACK temeli

Eklenenler:
- Ayrı MOB ATTACK alanı.
- Dinamik MOB skill/scroll kayıtları.
- Skilllerde varsayılan sıralı, opsiyonel random çalışma.
- Priest threshold-heal modeli.
- Anchor/range tüketici modeli.
- Attack tarafında ek skilller.

Sınır: gerçek mob nameplate seçimi, gerçek coordinate producer, Battle Cry görsel doğrulama ve tam responsive MOB UI henüz yoktu.

## 3. v4.8.13.1 — MOB R Chase düzeltmesi

Canlı gereksinim: W hedefe göre dönmediği için MOB kovalaması `R` ile olmalı.

Yapılanlar:
- MOB chase komutu `R` yapıldı.
- MOB chase worker içinde `W` bulunmadığı statik testle zorunlu tutuldu.
- Legacy ATTACK W combo korundu.
- Model testleri 8/8 seviyesine çıkarıldı.

O dönemde doğrulanmış EXE SHA-256: `15A091C9FD3D69D2EBA74DEAF911DEA19FC3BCC319A3F689471DACB0DF5856DC`.

## 4. v4.8.14 — Production cleanup / Defender yüzey azaltma

Amaç: final EXE içinde gereksiz self-test/observer kodu ve kullanılmayan runtime yüzey bırakmamak.

Yapılanlar:
- Production dışı observer/test yüzeyi final binary'den ayrıldı.
- Kullanılmayan position mapping kaldırıldı.
- Minor/Cure/Attack/input motoru korunmaya çalışıldı.

Sonraki canlı geri bildirim: Warrior eklendikten sonra Defender yeniden alarm verdi. Bu nedenle CI Defender sonucu tek başına temiz kabul edilmemeye başlandı.

## 5. v4.8.15 — Compact UI

Yapılanlar:
- ATTACK ek skilllerin ayrı sağ kutu yerine Skill 1–4 devamı mantığına yaklaştırılması.
- MOB altında General/Priest alt alan denemesi.
- Font küçültme ve DPI font katmanı.

Daha sonraki canlı gereksinim Priest'in ayrı ana kategori olması yönündeydi; v4.8.24'te değiştirildi.

## 6. v4.8.16 — WARRIOR / inventory right-click

Warrior gereksinimi:
- 28 inventory slotu.
- Kullanıcı slot numarası seçer.
- Drag/drop yok; item üstünde yalnız sağ tık.

İlk implementasyonda:
- 7×4 inventory grid algılama.
- Sağ tık equip/unequip.
- Kalkan ve silah kayıtları.

Canlı geri bildirim:
- Defender alarmı yeniden başladı.
- Slot seçimi ve hız konusunda daha sonra hatalar görüldü.

## 7. v4.8.17 / v4.8.18 — Defender araştırmaları

Denemeler:
- `GetCursorPos` / `SetCursorPos` kaldırıldı.
- Global low-level keyboard hook yerine daha standart hotkey mimarisi denendi.

Canlı sonuç: Alarm tamamen çözülmedi.

Bu nedenle asıl çözüm tek tek API silmek yerine v4.8.11 binary üretim karakteristiğine dönmek oldu.

## 8. v4.8.19 — Stable-shape yeniden kurulum

Virüs/heuristic sorununu çözmede kritik checkpoint.

Yapılanlar:
- Üretim hattı v4.8.11 exact kaynak/binary karakteristiğine yaklaştırıldı.
- Ürün kimliği tekrar `PremiumPlusCombo` yapıldı.
- Build/link/manifest şekli known-good üretime döndürüldü.
- Final EXE ile v4.8.11 kontrol EXE import yüzeyi birebir karşılaştırılmaya başlandı.
- Sonraki üretimlerde yeni Windows API eklenmemesi CI kapısı oldu.

Bu noktadan sonra kullanıcının canlı bilgisayarında Defender problemi çözülmüş checkpoint olarak kullanıldı.

## 9. v4.8.20 — Warrior shared hotkey + slot fix

Canlı problem:
- Slot 4 yazılmasına rağmen slot 1'e gidiyordu.
- İlk basışlar kaçıyor, 5–6 basış gerekebiliyordu.
- Kalkan/silah için iki ayrı hotkey istenmiyordu.

Yapılanlar:
- Tek ortak `EKİPMAN DEĞİŞTİR` hotkey.
- İki kayıt: Kalkan slot / Silah slot; başarılı işlemler arasında toggle.
- Slot editleri Kaydet'e basmadan runtime'a yansıtıldı.
- Envanter alanı kalibrasyonu yardımcı/fallback destek olarak eklendi; tek güven kaynağı yapılmadı.
- Slot 4 = dördüncü sütun model testi eklendi.

## 10. v4.8.21 — Descent + otomatik süreli skill temeli

Eklenenler:
- Descent: ayrı hotkey + Bar + Slot.
- Otomatik süreli skill: Bar + Slot + saniye/dakika interval.
- Kümülatif drift oluşturmamak için mutlak deadline mantığı.
- Manuel Descent isteklerinin worker meşgulken kaybolmaması için pending yaklaşımı.

## 11. v4.8.22 — Battle Cry + fast equipment

Eklenenler:
- `OTOMATİK BATTLE CRY` ayrı opsiyon.
- Buff/scroll bölgesi kalibrasyonu.
- Battle Cry görünmeden cast'i başarılı kabul etmeme yaklaşımı.
- `ECHO` adı `OTOMATİK SÜRELİ SKILL` olarak genelleştirildi ve ek skill satırları.
- Descent sonrası opsiyonel F bar dönüşü.
- Inventory aç/kapat/right-click yolunda hız optimizasyonları.

Canlı geri bildirim:
- Slot algılama hız optimizasyonu nedeniyle tekrar kaymıştı.
- Battle Cry mevcutken tekrar basma sorunu görülmüştü.

## 12. v4.8.23 — Slot + Battle Cry LIVEFIX

Yapılanlar:
- Kalibrasyon dikdörtgeninden doğrudan 7×4 grid tahmin eden riskli kestirme kaldırıldı.
- Yalnız doğrulanmış 7×4 grid cache hızlı yol olarak kullanılmaya başladı.
- Battle Cry için coarse + fine görsel imza ve kayıp toleransı eklendi.
- Tek kötü frame yüzünden recast yapılmaması hedeflendi.

Sonraki canlı geri bildirimlerde inventory hızı ve Battle Cry tekrar deneme davranışının daha da güçlendirilmesi istendi.

## 13. v4.8.24 — Priest kategori + Attack R + MOB target/range

Ana değişiklikler:
- Sidebar sırası: `ROGUE → WARRIOR → PRIEST → ATTACK → MOB ATTACK`.
- Priest, MOB ATTACK alt alanından çıkarılıp ayrı kategori yapıldı.
- ATTACK'e opsiyonel R Attack.
- MOB skillleri listbox yerine tikli dikey skill satırları.
- `ANKOR AL` yerine `FARM MERKEZİNİ KAYDET` yaklaşımı.
- Koordinat alanı kalibrasyonu ile farm merkezi/range modeli.
- En fazla 7 mob hedef kaydı.
- Görsel nameplate kaydı.
- Kısmi / tam görsel-nameplate eşleşme modu.
- Mouse ile candidate tıklama.
- Target HP + üst target header doğrulanmadan `g_mobTargetConfirmed` açmama.
- R chase ve MOB skill yalnız confirmed target sonrası.
- Selector senaryo modeli: 48/48.
- Gerçek Windows `SendInput` mouse harness: 9/9.
- Legacy: 180/180, MOB model: 16/16, Warrior: 26/26.
- Exact import parity v4.8.11'e karşı korunmuştur.

v4.8.24 EXE SHA-256: `243DB4E95A6DB725A3A49D73DB5DAE99E4DAACC36945F6F032489B16E0048510`

v4.8.24 final source SHA-256: `F709BFE06C77F3BAE5AED78F20C99ECDE70B5B4995C351C6B2D1AF7435F082C6`

### v4.8.24 canlı testte bulunan kritik regresyonlar

1. **MOB HEDEF ekranı üst üste binme**
   - `RefreshMobLists()` aktif tabı dikkate almadan skill row HWND'lerini yeniden `SW_SHOW` yapabiliyordu.
   - Hedef satırları ile target status / save alanları da birbirine fazla yakındı.

2. **Troll gibi ince kırmızı nameplate görselini öğrenememe**
   - Eski kırmızı piksel eşiği ve 16×8 mask hücre doluluk şartı ince/anti-aliased KO fontuna fazla katıydı.
   - Sonuç: ekranda kırmızı mob adı görünürken `yeterli kırmızı mob isim görüntüsü bulunamadı` mesajı görülebiliyordu.

3. **PRIEST sidebar rengi**
   - `DrawOwnerButton()` sidebar ID listesinde `IDC_CATEGORY_PRIEST` unutulduğundan Priest normal bordo kategori yerine altın/turuncu fallback stile düşüyordu.

Bu canlı regresyonlar v4.8.25'in doğrudan gerekçesidir.

## 14. v4.8.25 — MOB UI + runtime acquisition LIVE FIX

Final CI ile doğrulanacak değişiklikler:
- Priest sidebar aynı bordo kategori stili.
- HEDEF/SKILL/SCROLL tabları birbirlerinin HWND'lerini açamaz.
- MOB HEDEF/Skill/Scroll koordinatları 760×620 temel pencerede çakışmayacak şekilde yeniden akıtıldı.
- Gerçek responsive child layout: tüm kontroller creation-time logical rect ile kaydedilir; DPI/window büyüdüğünde aynı oranda `MoveWindow` ile ölçeklenir.
- 100/125/150/200% ölçek geometri senaryoları.
- İnce/anti-aliased kırmızı nameplate toleransı güçlendirildi.
- Görsel öğrenme minimum kırmızı piksel / cell density şartı gevşetildi, fakat saldırı doğrulaması gevşetilmedi.
- Mouse candidate öncelikli seçim korunur.
- Mouse candidate bulunamaz/örtülürse bounded `Z` fallback aday dolaşımı yapılabilir.
- `Z` tek başına hedef onayı değildir.
- Her candidate `TargetHpBarVisible && HeaderMatchesTarget` doğrulamasından geçmeden confirmed olamaz.
- R chase ve MOB skills yalnız confirmed target sonrası.
- Eski çalışan Minor/Cure/R/Attack/W-S/Vitals/Warrior/BattleCry worker gövdeleri korunur.
- Exact PE/import surface parity v4.8.11 ile korunur.

## 15. Test felsefesi

Projede üç ayrı doğrulama seviyesi kullanılır:

1. **Kaynak/statik regresyon:** Korunan fonksiyon gövdeleri ve kritik gate'ler karşılaştırılır.
2. **CI/model testleri:** 180 legacy + MOB + Warrior + selector senaryoları.
3. **Gerçek Windows input harness:** `SendInput` ile gerçek mouse mesajı alan sahte oyun penceresi.

Bunların hiçbiri Knight Online client'ın canlı render/input davranışını yüzde 100 simüle etmez. Bu nedenle canlı kullanıcı testi sonucu ayrıca kaydedilir. v4.8.25 Troll/nameplate düzeltmesi gerçek kullanıcı ekran görüntüsündeki başarısızlık modeline göre tasarlanmıştır; son doğrulama yine gerçek client testidir.
